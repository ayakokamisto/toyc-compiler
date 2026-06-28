#pragma once

#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace toyc {

struct FoldResult {
  bool folded = false;
  int32_t value = 0;
};

bool removeUnreachableBlocks(Function& function);
bool replaceAllUses(Function& function, const std::unordered_map<ValueId, ValueId>& replacements);
bool replaceUse(Function& function, ValueId oldValue, ValueId newValue);
std::vector<ValueId> instructionOperands(const Inst& inst);
bool instructionHasSideEffects(const Inst& inst);
bool instructionIsRemovable(const Inst& inst);
std::unordered_set<ValueId> collectUsedValues(const Function& function);
std::optional<int32_t> constValueOf(const Function& function, ValueId value);
ValueId appendConst(BasicBlock& block, Function& function, int32_t value);
FoldResult foldUnary(UnaryOpcode op, int32_t operand);
FoldResult foldBinary(BinaryOpcode op, int32_t lhs, int32_t rhs);
FoldResult foldCompare(ComparePredicate pred, int32_t lhs, int32_t rhs);
void rewriteTerminatorTarget(Terminator& term, BlockId oldTarget, BlockId newTarget);
BasicBlock* findBlock(Function& function, BlockId id);
const BasicBlock* findBlock(const Function& function, BlockId id);

} // namespace toyc
