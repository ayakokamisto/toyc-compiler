#pragma once
/// IR Builder — convenience API for constructing IR instructions.
/// This is a P0 placeholder — real building will be implemented in P4.

#include "toyc/ir/instruction.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"

namespace toyc {

/// Builder for inserting instructions into a basic block.
/// Currently a P0 placeholder.
class IRBuilder {
public:
  IRBuilder() = default;

  /// Set the insertion point.
  void setInsertPoint(BasicBlock* block);

  /// Create instructions (P0 stubs — return nullptr).
  SlotLoadInst* createSlotLoad(SlotId slot);
  SlotStoreInst* createSlotStore(SlotId slot, Value* val);
  BinaryInst* createBinary(BinaryOp op, Value* lhs, Value* rhs);
  UnaryInst* createUnary(UnaryOp op, Value* operand);
  CmpInst* createCmp(CmpPred pred, Value* lhs, Value* rhs);
  RetInst* createRet(Value* val);
  RetInst* createVoidRet();
  BrInst* createBr(BlockId target);
  CondBrInst* createCondBr(Value* cond, BlockId trueBB, BlockId falseBB);

private:
  BasicBlock* insertBB_ = nullptr;
};

} // namespace toyc
