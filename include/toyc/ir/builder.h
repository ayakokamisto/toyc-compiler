#pragma once
/// IRBuilder — helper for constructing ToyC IR.
///
/// Tracks the current function and insert block, allocates IDs,
/// and emits instructions and terminators.

#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/ir_type.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/support/ids.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace toyc {

class IRBuilder {
public:
  IRBuilder() = default;

  /// Set the function being built. All block/value/slot creation goes into this function.
  void setFunction(Function* func);

  /// Set the current insert block.
  void setInsertBlock(BlockId block);

  /// Create a new block in the current function.
  BlockId createBlock(std::string label = "");

  /// Get the current function.
  [[nodiscard]] Function* function() const { return func_; }

  /// Get the current block.
  [[nodiscard]] BasicBlock* insertBlock() const;

  /// Emit a ConstInt instruction.
  ValueId emitConstInt(int32_t value);

  /// Slot operations.
  ValueId emitLoadSlot(SlotId slot);
  void emitStoreSlot(SlotId slot, ValueId value);

  /// Global operations.
  ValueId emitLoadGlobal(GlobalId global);
  void emitStoreGlobal(GlobalId global, ValueId value);

  /// Unary operation.
  ValueId emitUnary(UnaryOpcode op, ValueId operand);

  /// Binary operation.
  ValueId emitBinary(BinaryOpcode op, ValueId lhs, ValueId rhs);

  /// Compare operation.
  ValueId emitCompare(ComparePredicate pred, ValueId lhs, ValueId rhs);

  /// Function call. Pass returnsValue=true if callee returns i32, false for void.
  std::optional<ValueId> emitCall(FunctionId callee, std::span<const ValueId> arguments, bool returnsValue = true);

  /// Terminators.
  void emitBranch(BlockId target);
  void emitCondBranch(ValueId condition, BlockId trueTarget, BlockId falseTarget);
  void emitReturn(std::optional<ValueId> value);

private:
  Function* func_ = nullptr;
  BlockId currentBlock_;

  /// Create a new Value in the current function and return its id.
  ValueId allocValue();
};

} // namespace toyc
