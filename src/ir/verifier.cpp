/// IR Verifier — checks structural well-formedness of the IR module.

#include "toyc/ir/verifier.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace toyc {

VerificationResult verifySSAFunction(const Function& func, const Module& module);

static bool isValueDefined(const Function& func, ValueId vid) {
  for (const auto& v : func.values()) {
    if (v.id == vid) return true;
  }
  return false;
}

static bool slotExists(const Function& func, SlotId sid) {
  for (const auto& s : func.slots()) {
    if (s.id == sid) return true;
  }
  return false;
}

static bool blockExists(const Function& func, BlockId bid) {
  for (const auto& bb : func.blocks()) {
    if (bb->id() == bid) return true;
  }
  return false;
}

VerificationResult verifyModule(const Module& module, VerificationMode mode) {
  VerificationResult result;

  std::set<uint32_t> globalIds;
  for (const auto& g : module.globals()) {
    if (!globalIds.insert(g.id.value).second) {
      result.addError("Duplicate GlobalId: " + std::to_string(g.id.value));
    }
  }

  std::set<uint32_t> funcIds;
  for (const auto& f : module.functions()) {
    if (!funcIds.insert(f->id().value).second) {
      result.addError("Duplicate FunctionId: " + std::to_string(f->id().value));
    }
  }

  std::set<std::string> globalNames;
  for (const auto& g : module.globals()) {
    if (!globalNames.insert(g.name).second) {
      result.addError("Duplicate global name: " + g.name);
    }
  }

  std::set<std::string> funcNames;
  for (const auto& f : module.functions()) {
    if (!funcNames.insert(f->name()).second) {
      result.addError("Duplicate function name: " + f->name());
    }
  }

  for (const auto& func : module.functions()) {
    VerificationMode funcMode = mode;
    if (funcMode == VerificationMode::Auto) {
      funcMode = func->form() == IRForm::SSA ? VerificationMode::SSA : VerificationMode::CanonicalSlot;
    }
    auto funcResult = funcMode == VerificationMode::SSA
                          ? verifySSAFunction(*func, module)
                          : verifyFunction(*func, module, VerificationMode::CanonicalSlot);
    for (auto& err : funcResult.errors) {
      result.addError(std::move(err));
    }
  }

  return result;
}

static void verifyInst(VerificationResult& result, const Function& func, const Module& module,
                       const Inst& inst, const BasicBlock& block) {
  auto checkValue = [&](ValueId vid, const char* ctx) {
    if (!isValueDefined(func, vid)) {
      result.addError("Undefined value " + std::to_string(vid.value) + " in " + ctx +
                      " of block " + block.label());
    }
  };

  auto checkSlot = [&](SlotId sid, const char* ctx) {
    if (!slotExists(func, sid)) {
      result.addError("Invalid SlotId " + std::to_string(sid.value) + " in " + ctx +
                      " of block " + block.label());
    }
  };

  switch (inst.opcode) {
    case Opcode::Phi:
      if (func.form() != IRForm::SSA) {
        result.addError("Phi appears in Canonical Slot IR block " + block.label());
      }
      break;
    case Opcode::ConstInt:
      break;
    case Opcode::SlotLoad:
      checkSlot(inst.slot, "load.slot");
      break;
    case Opcode::SlotStore:
      checkSlot(inst.slot, "store.slot");
      checkValue(inst.lhs, "store.slot value");
      break;
    case Opcode::GlobalLoad: {
      const auto* g = module.findGlobal(inst.global);
      if (!g) {
        result.addError("Invalid GlobalId in load.global of block " + block.label());
      }
      break;
    }
    case Opcode::GlobalStore: {
      const auto* g = module.findGlobal(inst.global);
      if (!g) {
        result.addError("Invalid GlobalId in store.global of block " + block.label());
      }
      checkValue(inst.lhs, "store.global value");
      break;
    }
    case Opcode::Unary:
      checkValue(inst.unaryOperand, "unary operand");
      break;
    case Opcode::Binary:
      checkValue(inst.lhs, "binary lhs");
      checkValue(inst.rhs, "binary rhs");
      break;
    case Opcode::Compare:
      checkValue(inst.cmpLhs, "compare lhs");
      checkValue(inst.cmpRhs, "compare rhs");
      break;
    case Opcode::Call: {
      const auto* callee = module.findFunction(inst.callee);
      if (!callee) {
        result.addError("Invalid FunctionId in call of block " + block.label());
        break;
      }
      if (inst.arguments.size() != callee->params().size()) {
        result.addError("Call argument count mismatch: expected " +
                        std::to_string(callee->params().size()) + " got " +
                        std::to_string(inst.arguments.size()) +
                        " in call to " + callee->name());
      }
      for (const auto& arg : inst.arguments) {
        checkValue(arg, "call argument");
      }
      bool calleeReturnsInt = callee->returnType().isI32();
      if (calleeReturnsInt && !inst.result.has_value()) {
        result.addError("Call to int function " + callee->name() + " missing result in block " + block.label());
      }
      if (!calleeReturnsInt && inst.result.has_value()) {
        result.addError("Call to void function " + callee->name() + " has result in block " + block.label());
      }
      break;
    }
    default:
      result.addError("Illegal opcode in instruction of block " + block.label());
      break;
  }
}

static void verifyTerminator(VerificationResult& result, const Function& func,
                             const Terminator& term, const BasicBlock& block) {
  switch (term.opcode) {
    case Opcode::Br:
      if (!blockExists(func, term.branchTarget)) {
        result.addError("Invalid branch target in block " + block.label());
      }
      break;
    case Opcode::CondBr:
      if (!isValueDefined(func, term.condCondition)) {
        result.addError("Undefined condition value in condbr of block " + block.label());
      }
      if (!blockExists(func, term.condTrueTarget)) {
        result.addError("Invalid true target in condbr of block " + block.label());
      }
      if (!blockExists(func, term.condFalseTarget)) {
        result.addError("Invalid false target in condbr of block " + block.label());
      }
      break;
    case Opcode::Ret:
      if (func.returnType().isI32()) {
        if (!term.returnValue.has_value()) {
          result.addError("Int function " + func.name() + " missing return value in block " + block.label());
        } else if (!isValueDefined(func, *term.returnValue)) {
          result.addError("Undefined return value in block " + block.label());
        }
      } else if (func.returnType().isVoid()) {
        if (term.returnValue.has_value()) {
          result.addError("Void function " + func.name() + " has return value in block " + block.label());
        }
      }
      break;
    default:
      result.addError("Unknown terminator opcode in block " + block.label());
      break;
  }
}

static VerificationMode effectiveMode(const Function& func, VerificationMode mode) {
  if (mode != VerificationMode::Auto) return mode;
  return func.form() == IRForm::SSA ? VerificationMode::SSA : VerificationMode::CanonicalSlot;
}

VerificationResult verifyFunction(const Function& func, const Module& module, VerificationMode mode) {
  VerificationResult result;
  mode = effectiveMode(func, mode);

  if (mode == VerificationMode::CanonicalSlot && func.form() != IRForm::CanonicalSlot) {
    result.addError("Function " + func.name() + " is not Canonical Slot IR");
  }
  if (mode == VerificationMode::SSA && func.form() != IRForm::SSA) {
    result.addError("Function " + func.name() + " is not SSA IR");
  }

  if (func.blocks().empty()) {
    result.addError("Function " + func.name() + " has no blocks");
    return result;
  }

  std::set<std::string> labels;
  for (const auto& bb : func.blocks()) {
    if (!labels.insert(bb->label()).second) {
      result.addError("Duplicate block label: " + bb->label() + " in function " + func.name());
    }
  }

  for (const auto& bb : func.blocks()) {
    if (!bb->hasTerminator()) {
      result.addError("Block " + bb->label() + " in function " + func.name() + " has no terminator");
    }

    for (const auto& inst : bb->instructions()) {
      if (mode == VerificationMode::CanonicalSlot && inst->opcode == Opcode::Phi) {
        result.addError("Canonical Slot IR cannot contain phi in block " + bb->label());
      }
      if (mode == VerificationMode::SSA &&
          (inst->opcode == Opcode::SlotLoad || inst->opcode == Opcode::SlotStore)) {
        result.addError("SSA IR cannot contain promoted slot access in block " + bb->label());
      }
      verifyInst(result, func, module, *inst, *bb);
    }

    if (bb->hasTerminator()) {
      verifyTerminator(result, func, *bb->terminator(), *bb);
    }
  }

  std::set<uint32_t> valueIds;
  for (const auto& v : func.values()) {
    if (!valueIds.insert(v.id.value).second) {
      result.addError("Duplicate ValueId " + std::to_string(v.id.value) + " in function " + func.name());
    }
  }

  return result;
}

namespace {

struct DefSite {
  BlockId block;
  int index = -1;
  bool argument = false;
};

static std::vector<ValueId> normalOperands(const Inst& inst) {
  switch (inst.opcode) {
    case Opcode::SlotStore:
    case Opcode::GlobalStore:
      return {inst.lhs};
    case Opcode::Unary:
      return {inst.unaryOperand};
    case Opcode::Binary:
      return {inst.lhs, inst.rhs};
    case Opcode::Compare:
      return {inst.cmpLhs, inst.cmpRhs};
    case Opcode::Call:
      return inst.arguments;
    default:
      return {};
  }
}

static bool hasDuplicatePred(const Inst& phi) {
  std::set<uint32_t> seen;
  for (const auto& incoming : phi.phiIncoming) {
    if (!seen.insert(incoming.predecessor.value).second) return true;
  }
  return false;
}

class LocalDominator {
public:
  explicit LocalDominator(const Function& func) {
    std::vector<BlockId> blocks;
    for (const auto& block : func.blocks()) {
      blocks.push_back(block->id());
      all_.insert(block->id());
    }
    if (blocks.empty()) return;

    auto entry = func.entryBlock()->id();
    for (auto block : blocks) {
      if (block == entry) {
        doms_[block].insert(block);
      } else {
        doms_[block] = all_;
      }
    }

    bool changed = true;
    while (changed) {
      changed = false;
      for (const auto& blockPtr : func.blocks()) {
        auto block = blockPtr->id();
        if (block == entry) continue;

        std::unordered_set<BlockId> next = all_;
        if (blockPtr->predecessors().empty()) {
          next.clear();
        }
        for (auto pred : blockPtr->predecessors()) {
          std::unordered_set<BlockId> intersection;
          for (auto candidate : next) {
            if (doms_[pred].contains(candidate)) intersection.insert(candidate);
          }
          next = std::move(intersection);
        }
        next.insert(block);
        if (next != doms_[block]) {
          doms_[block] = std::move(next);
          changed = true;
        }
      }
    }
  }

  bool dominates(BlockId dominator, BlockId block) const {
    auto it = doms_.find(block);
    return it != doms_.end() && it->second.contains(dominator);
  }

private:
  std::unordered_set<BlockId> all_;
  std::unordered_map<BlockId, std::unordered_set<BlockId>> doms_;
};

} // namespace

VerificationResult verifySSAFunction(const Function& func, const Module& module) {
  VerificationResult result = verifyFunction(func, module, VerificationMode::SSA);
  if (func.blocks().empty()) return result;

  LocalDominator dom(func);
  std::unordered_map<ValueId, DefSite> defs;

  auto entryId = func.entryBlock()->id();
  for (const auto& param : func.params()) {
    defs[param.valueId] = DefSite{entryId, -1, true};
  }

  for (const auto& blockPtr : func.blocks()) {
    const auto& block = *blockPtr;
    bool seenNormal = false;
    int index = 0;
    for (const auto& inst : block.instructions()) {
      if (inst->opcode == Opcode::Phi) {
        if (seenNormal) {
          result.addError("Phi outside block prefix in block " + block.label());
        }
        if (!inst->result.has_value()) {
          result.addError("Phi missing result in block " + block.label());
        } else if (!inst->resultType.isI32()) {
          result.addError("Phi result type must be i32 in block " + block.label());
        } else if (defs.contains(*inst->result)) {
          result.addError("Duplicate SSA definition for value " + std::to_string(inst->result->value));
        } else {
          defs[*inst->result] = DefSite{block.id(), index, false};
        }
      } else {
        seenNormal = true;
        if (inst->result.has_value()) {
          if (defs.contains(*inst->result)) {
            result.addError("Duplicate SSA definition for value " + std::to_string(inst->result->value));
          } else {
            defs[*inst->result] = DefSite{block.id(), index, false};
          }
        }
      }
      ++index;
    }
  }

  auto checkUse = [&](ValueId value, BlockId useBlock, int useIndex, const std::string& ctx) {
    auto it = defs.find(value);
    if (it == defs.end()) {
      result.addError("SSA use of undefined value " + std::to_string(value.value) + " in " + ctx);
      return;
    }
    const auto& def = it->second;
    if (def.argument) {
      if (!dom.dominates(def.block, useBlock)) {
        result.addError("SSA argument value does not dominate use in " + ctx);
      }
      return;
    }
    if (def.block == useBlock) {
      if (def.index >= useIndex) {
        result.addError("SSA value used before definition in " + ctx);
      }
    } else if (!dom.dominates(def.block, useBlock)) {
      result.addError("SSA value definition does not dominate use in " + ctx);
    }
  };

  for (const auto& blockPtr : func.blocks()) {
    const auto& block = *blockPtr;
    std::set<uint32_t> predSet;
    for (auto pred : block.predecessors()) predSet.insert(pred.value);
    int index = 0;
    for (const auto& inst : block.instructions()) {
      if (inst->opcode == Opcode::Phi) {
        if (hasDuplicatePred(*inst)) {
          result.addError("Phi duplicate predecessor in block " + block.label());
        }
        std::set<uint32_t> incomingPreds;
        for (const auto& incoming : inst->phiIncoming) {
          incomingPreds.insert(incoming.predecessor.value);
          if (!predSet.contains(incoming.predecessor.value)) {
            result.addError("Phi incoming predecessor is not a CFG predecessor in block " + block.label());
          }
          auto def = defs.find(incoming.value);
          if (def == defs.end()) {
            result.addError("Phi incoming uses undefined value in block " + block.label());
          } else if (!def->second.argument && def->second.block != incoming.predecessor &&
                     !dom.dominates(def->second.block, incoming.predecessor)) {
            result.addError("Phi incoming value does not dominate predecessor edge in block " + block.label());
          }
        }
        if (incomingPreds != predSet) {
          result.addError("Phi incoming predecessor set mismatch in block " + block.label());
        }
      } else {
        for (auto operand : normalOperands(*inst)) {
          checkUse(operand, block.id(), index, "instruction of block " + block.label());
        }
      }
      ++index;
    }
    if (block.hasTerminator()) {
      const auto& term = *block.terminator();
      if (term.opcode == Opcode::CondBr) {
        checkUse(term.condCondition, block.id(), index, "condbr of block " + block.label());
      } else if (term.opcode == Opcode::Ret && term.returnValue.has_value()) {
        checkUse(*term.returnValue, block.id(), index, "ret of block " + block.label());
      }
    }
  }

  return result;
}

VerificationResult verifySSAModule(const Module& module) {
  VerificationResult result;
  for (const auto& func : module.functions()) {
    auto funcResult = verifySSAFunction(*func, module);
    for (auto& err : funcResult.errors) {
      result.addError(std::move(err));
    }
  }
  return result;
}

} // namespace toyc
