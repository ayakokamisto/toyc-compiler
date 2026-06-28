#include "toyc/passes/sccp.h"

#include "toyc/analysis/cfg.h"
#include "toyc/passes/ir_utils.h"

#include <optional>
#include <unordered_map>

namespace toyc {

namespace {

enum class LatticeKind { Unknown, Constant, Overdefined };

struct LatticeValue {
  LatticeKind kind = LatticeKind::Unknown;
  int32_t value = 0;
};

static bool mergeValue(LatticeValue& target, LatticeValue incoming) {
  if (incoming.kind == LatticeKind::Unknown) return false;
  if (target.kind == LatticeKind::Unknown) {
    target = incoming;
    return true;
  }
  if (target.kind == LatticeKind::Overdefined) return false;
  if (incoming.kind == LatticeKind::Overdefined) {
    target = incoming;
    return true;
  }
  if (target.value != incoming.value) {
    target = {LatticeKind::Overdefined, 0};
    return true;
  }
  return false;
}

static LatticeValue getValue(const std::unordered_map<ValueId, LatticeValue>& values, ValueId id) {
  auto it = values.find(id);
  return it == values.end() ? LatticeValue{} : it->second;
}

static LatticeValue evalInst(const Inst& inst, const std::unordered_map<ValueId, LatticeValue>& values) {
  switch (inst.opcode) {
    case Opcode::ConstInt:
      return {LatticeKind::Constant, inst.constValue};
    case Opcode::GlobalLoad:
    case Opcode::Call:
      return {LatticeKind::Overdefined, 0};
    case Opcode::Unary: {
      auto operand = getValue(values, inst.unaryOperand);
      if (operand.kind == LatticeKind::Unknown) return {};
      if (operand.kind == LatticeKind::Overdefined) return {LatticeKind::Overdefined, 0};
      auto folded = foldUnary(inst.unaryOp, operand.value);
      return folded.folded ? LatticeValue{LatticeKind::Constant, folded.value}
                           : LatticeValue{LatticeKind::Overdefined, 0};
    }
    case Opcode::Binary: {
      auto lhs = getValue(values, inst.lhs);
      auto rhs = getValue(values, inst.rhs);
      if (lhs.kind == LatticeKind::Unknown || rhs.kind == LatticeKind::Unknown) return {};
      if (lhs.kind == LatticeKind::Overdefined || rhs.kind == LatticeKind::Overdefined) {
        return {LatticeKind::Overdefined, 0};
      }
      auto folded = foldBinary(inst.binaryOp, lhs.value, rhs.value);
      return folded.folded ? LatticeValue{LatticeKind::Constant, folded.value}
                           : LatticeValue{LatticeKind::Overdefined, 0};
    }
    case Opcode::Compare: {
      auto lhs = getValue(values, inst.cmpLhs);
      auto rhs = getValue(values, inst.cmpRhs);
      if (lhs.kind == LatticeKind::Unknown || rhs.kind == LatticeKind::Unknown) return {};
      if (lhs.kind == LatticeKind::Overdefined || rhs.kind == LatticeKind::Overdefined) {
        return {LatticeKind::Overdefined, 0};
      }
      auto folded = foldCompare(inst.cmpPred, lhs.value, rhs.value);
      return folded.folded ? LatticeValue{LatticeKind::Constant, folded.value}
                           : LatticeValue{LatticeKind::Overdefined, 0};
    }
    default:
      return inst.result.has_value() ? LatticeValue{LatticeKind::Overdefined, 0} : LatticeValue{};
  }
}

static void turnIntoConst(Inst& inst, int32_t value) {
  inst.opcode = Opcode::ConstInt;
  inst.constValue = value;
  inst.phiIncoming.clear();
  inst.arguments.clear();
}

} // namespace

PassResult SCCPPass::run(Function& function) {
  if (function.form() != IRForm::SSA) return {};
  rebuildCFG(function);

  std::unordered_map<ValueId, LatticeValue> values;
  for (const auto& param : function.params()) {
    values[param.valueId] = {LatticeKind::Overdefined, 0};
  }

  bool changedLattice = true;
  while (changedLattice) {
    changedLattice = false;
    for (const auto& block : function.blocks()) {
      for (const auto& inst : block->instructions()) {
        if (!inst->result.has_value()) continue;
        LatticeValue incoming;
        if (inst->opcode == Opcode::Phi) {
          for (const auto& edge : inst->phiIncoming) {
            changedLattice |= mergeValue(incoming, getValue(values, edge.value));
          }
        } else {
          incoming = evalInst(*inst, values);
        }
        changedLattice |= mergeValue(values[*inst->result], incoming);
      }
    }
  }

  bool changed = false;
  for (auto& block : function.blocks()) {
    for (auto& inst : block->mutableInstructions()) {
      if (!inst->result.has_value()) continue;
      auto lattice = getValue(values, *inst->result);
      if (lattice.kind == LatticeKind::Constant && inst->opcode != Opcode::ConstInt) {
        turnIntoConst(*inst, lattice.value);
        changed = true;
      }
    }

    auto* term = block->mutableTerminator();
    if (term && term->opcode == Opcode::CondBr) {
      auto cond = getValue(values, term->condCondition);
      if (cond.kind == LatticeKind::Constant) {
        Terminator br;
        br.opcode = Opcode::Br;
        br.branchTarget = cond.value != 0 ? term->condTrueTarget : term->condFalseTarget;
        block->setTerminator(br);
        changed = true;
      }
    }
  }

  if (changed) {
    rebuildCFG(function);
    changed |= removeUnreachableBlocks(function);
  }
  return {changed};
}

} // namespace toyc
