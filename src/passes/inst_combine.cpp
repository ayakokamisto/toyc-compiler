#include "toyc/passes/inst_combine.h"

#include "toyc/passes/ir_utils.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace toyc {

static void turnIntoConst(Inst& inst, int32_t value) {
  inst.opcode = Opcode::ConstInt;
  inst.constValue = value;
  inst.phiIncoming.clear();
  inst.arguments.clear();
}

static std::unordered_map<ValueId, int32_t> collectConstants(const Function& function) {
  std::unordered_map<ValueId, int32_t> constants;
  for (const auto& block : function.blocks()) {
    for (const auto& inst : block->instructions()) {
      if (inst->opcode == Opcode::ConstInt && inst->result.has_value()) {
        constants[*inst->result] = inst->constValue;
      }
    }
  }
  return constants;
}

static std::string valueKey(ValueId value) {
  return std::to_string(value.value);
}

static void sortCommutative(ValueId& lhs, ValueId& rhs) {
  if (rhs.value < lhs.value) std::swap(lhs, rhs);
}

static std::string expressionKey(const Inst& inst) {
  switch (inst.opcode) {
    case Opcode::Unary:
      return "u:" + std::to_string(static_cast<int>(inst.unaryOp)) + ":" + valueKey(inst.unaryOperand);
    case Opcode::Binary: {
      ValueId lhs = inst.lhs;
      ValueId rhs = inst.rhs;
      if (inst.binaryOp == BinaryOpcode::Add || inst.binaryOp == BinaryOpcode::Multiply) {
        sortCommutative(lhs, rhs);
      }
      return "b:" + std::to_string(static_cast<int>(inst.binaryOp)) + ":" + valueKey(lhs) + ":" + valueKey(rhs);
    }
    case Opcode::Compare: {
      ValueId lhs = inst.cmpLhs;
      ValueId rhs = inst.cmpRhs;
      auto pred = inst.cmpPred;
      if (pred == ComparePredicate::Equal || pred == ComparePredicate::NotEqual) {
        sortCommutative(lhs, rhs);
      }
      return "c:" + std::to_string(static_cast<int>(pred)) + ":" + valueKey(lhs) + ":" + valueKey(rhs);
    }
    default:
      return {};
  }
}

PassResult InstCombineLitePass::run(Function& function) {
  if (function.form() != IRForm::SSA) return {};

  bool changed = false;
  auto constants = collectConstants(function);
  std::unordered_map<ValueId, ValueId> replacements;
  std::vector<ValueId> erasedValues;

  for (auto& block : function.blocks()) {
    std::unordered_map<std::string, ValueId> localExpressions;
    auto& insts = block->mutableInstructions();
    for (auto& inst : insts) {
      if (!inst->result.has_value()) continue;
      auto replaceWith = [&](ValueId value) {
        replacements[*inst->result] = value;
        erasedValues.push_back(*inst->result);
        changed = true;
      };

      auto key = expressionKey(*inst);
      if (!key.empty()) {
        auto found = localExpressions.find(key);
        if (found != localExpressions.end()) {
          replaceWith(found->second);
          continue;
        }
      }

      switch (inst->opcode) {
        case Opcode::Unary: {
          auto operand = constants.find(inst->unaryOperand);
          if (operand != constants.end()) {
            auto folded = foldUnary(inst->unaryOp, operand->second);
            if (folded.folded) {
              turnIntoConst(*inst, folded.value);
              constants[*inst->result] = folded.value;
              changed = true;
            }
          }
          break;
        }
        case Opcode::Binary: {
          auto lhsIt = constants.find(inst->lhs);
          auto rhsIt = constants.find(inst->rhs);
          bool hasLhs = lhsIt != constants.end();
          bool hasRhs = rhsIt != constants.end();
          int32_t lhs = hasLhs ? lhsIt->second : 0;
          int32_t rhs = hasRhs ? rhsIt->second : 0;
          if (hasLhs && hasRhs) {
            auto folded = foldBinary(inst->binaryOp, lhs, rhs);
            if (folded.folded) {
              turnIntoConst(*inst, folded.value);
              constants[*inst->result] = folded.value;
              changed = true;
            }
            break;
          }
          if (hasRhs) {
            if ((inst->binaryOp == BinaryOpcode::Add || inst->binaryOp == BinaryOpcode::Subtract) && rhs == 0) {
              replaceWith(inst->lhs);
            } else if ((inst->binaryOp == BinaryOpcode::Multiply || inst->binaryOp == BinaryOpcode::Divide) &&
                       rhs == 1) {
              replaceWith(inst->lhs);
            } else if (inst->binaryOp == BinaryOpcode::Modulo && rhs == 1) {
              turnIntoConst(*inst, 0);
              constants[*inst->result] = 0;
              changed = true;
            } else if (inst->binaryOp == BinaryOpcode::Multiply && rhs == 0) {
              turnIntoConst(*inst, 0);
              constants[*inst->result] = 0;
              changed = true;
            } else if (inst->binaryOp == BinaryOpcode::Multiply && rhs == 2) {
              inst->binaryOp = BinaryOpcode::Add;
              inst->rhs = inst->lhs;
              changed = true;
            } else if (inst->binaryOp == BinaryOpcode::Modulo && rhs != 0 && inst->lhs == inst->rhs) {
              turnIntoConst(*inst, 0);
              constants[*inst->result] = 0;
              changed = true;
            }
          }
          if (hasLhs) {
            if (inst->binaryOp == BinaryOpcode::Add && lhs == 0) {
              replaceWith(inst->rhs);
            } else if (inst->binaryOp == BinaryOpcode::Multiply && lhs == 1) {
              replaceWith(inst->rhs);
            } else if (inst->binaryOp == BinaryOpcode::Multiply && lhs == 0) {
              turnIntoConst(*inst, 0);
              constants[*inst->result] = 0;
              changed = true;
            } else if (inst->binaryOp == BinaryOpcode::Multiply && lhs == 2) {
              inst->binaryOp = BinaryOpcode::Add;
              inst->lhs = inst->rhs;
              changed = true;
            }
          }
          break;
        }
        case Opcode::Compare: {
          auto lhsIt = constants.find(inst->cmpLhs);
          auto rhsIt = constants.find(inst->cmpRhs);
          if (lhsIt != constants.end() && rhsIt != constants.end()) {
            auto folded = foldCompare(inst->cmpPred, lhsIt->second, rhsIt->second);
            if (folded.folded) {
              turnIntoConst(*inst, folded.value);
              constants[*inst->result] = folded.value;
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
            constants[*inst->result] = value;
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
      key = expressionKey(*inst);
      if (!key.empty() && instructionIsRemovable(*inst)) {
        localExpressions.emplace(std::move(key), *inst->result);
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
