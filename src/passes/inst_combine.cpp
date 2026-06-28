#include "toyc/passes/inst_combine.h"

#include "toyc/passes/ir_utils.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace toyc {

static void turnIntoConst(Inst& inst, int32_t value) {
  inst.opcode = Opcode::ConstInt;
  inst.constValue = value;
  inst.phiIncoming.clear();
  inst.arguments.clear();
}

PassResult InstCombineLitePass::run(Function& function) {
  if (function.form() != IRForm::SSA) return {};

  bool changed = false;
  std::unordered_map<ValueId, ValueId> replacements;
  std::vector<ValueId> erasedValues;

  for (auto& block : function.blocks()) {
    auto& insts = block->mutableInstructions();
    for (auto& inst : insts) {
      if (!inst->result.has_value()) continue;

      auto replaceWith = [&](ValueId value) {
        replacements[*inst->result] = value;
        erasedValues.push_back(*inst->result);
        changed = true;
      };

      switch (inst->opcode) {
        case Opcode::Unary: {
          auto operand = constValueOf(function, inst->unaryOperand);
          if (operand.has_value()) {
            auto folded = foldUnary(inst->unaryOp, *operand);
            if (folded.folded) {
              turnIntoConst(*inst, folded.value);
              changed = true;
            }
          }
          break;
        }
        case Opcode::Binary: {
          auto lhs = constValueOf(function, inst->lhs);
          auto rhs = constValueOf(function, inst->rhs);
          if (lhs.has_value() && rhs.has_value()) {
            auto folded = foldBinary(inst->binaryOp, *lhs, *rhs);
            if (folded.folded) {
              turnIntoConst(*inst, folded.value);
              changed = true;
            }
            break;
          }
          if (rhs.has_value()) {
            if ((inst->binaryOp == BinaryOpcode::Add || inst->binaryOp == BinaryOpcode::Subtract) && *rhs == 0) {
              replaceWith(inst->lhs);
            } else if ((inst->binaryOp == BinaryOpcode::Multiply || inst->binaryOp == BinaryOpcode::Divide) &&
                       *rhs == 1) {
              replaceWith(inst->lhs);
            } else if (inst->binaryOp == BinaryOpcode::Modulo && *rhs == 1) {
              turnIntoConst(*inst, 0);
              changed = true;
            } else if (inst->binaryOp == BinaryOpcode::Multiply && *rhs == 0) {
              turnIntoConst(*inst, 0);
              changed = true;
            }
          }
          if (lhs.has_value()) {
            if (inst->binaryOp == BinaryOpcode::Add && *lhs == 0) {
              replaceWith(inst->rhs);
            } else if (inst->binaryOp == BinaryOpcode::Multiply && *lhs == 1) {
              replaceWith(inst->rhs);
            } else if (inst->binaryOp == BinaryOpcode::Multiply && *lhs == 0) {
              turnIntoConst(*inst, 0);
              changed = true;
            }
          }
          break;
        }
        case Opcode::Compare: {
          auto lhs = constValueOf(function, inst->cmpLhs);
          auto rhs = constValueOf(function, inst->cmpRhs);
          if (lhs.has_value() && rhs.has_value()) {
            auto folded = foldCompare(inst->cmpPred, *lhs, *rhs);
            if (folded.folded) {
              turnIntoConst(*inst, folded.value);
              changed = true;
            }
          } else if (inst->cmpLhs == inst->cmpRhs) {
            int32_t value = 0;
            switch (inst->cmpPred) {
              case ComparePredicate::Equal:
              case ComparePredicate::LessEqual:
              case ComparePredicate::GreaterEqual:
                value = 1;
                break;
              case ComparePredicate::NotEqual:
              case ComparePredicate::Less:
              case ComparePredicate::Greater:
                value = 0;
                break;
            }
            turnIntoConst(*inst, value);
            changed = true;
          }
          break;
        }
        case Opcode::Phi: {
          if (!inst->phiIncoming.empty()) {
            auto first = inst->phiIncoming.front().value;
            bool allSame = true;
            for (const auto& incoming : inst->phiIncoming) {
              if (incoming.value != first) allSame = false;
            }
            if (allSame) replaceWith(first);
          }
          break;
        }
        default:
          break;
      }
    }
  }

  if (!replacements.empty()) {
    changed |= replaceAllUses(function, replacements);
    for (auto& block : function.blocks()) {
      auto& insts = block->mutableInstructions();
      insts.erase(std::remove_if(insts.begin(), insts.end(), [&](const auto& inst) {
                    return inst->result.has_value() && replacements.contains(*inst->result) &&
                           instructionIsRemovable(*inst);
                  }),
                  insts.end());
    }
    function.eraseValues(erasedValues);
  }

  return {changed};
}

} // namespace toyc
