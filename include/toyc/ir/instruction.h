#pragma once
/// IR Instruction definitions.
///
/// Slot IR (canonical, pre-Mem2Reg):
///   slot.load, slot.store, global.load, global.store,
///   unary, binary, cmp, call, br, condbr, ret
///
/// SSA IR (post-Mem2Reg):
///   phi, unary, binary, cmp, call, br, condbr, ret
///
/// This is a P0 placeholder — real instruction classes will be added in P4+.

#include "toyc/ir/ir_type.h"
#include "toyc/ir/use.h"
#include "toyc/ir/value.h"
#include "toyc/support/ids.h"

#include <cstdint>
#include <vector>

namespace toyc {

/// Opcode enumeration — covers Slot IR and SSA IR instructions.
enum class Opcode : uint8_t {
  // Slot IR (canonical)
  SlotLoad,
  SlotStore,
  GlobalLoad,
  GlobalStore,

  // Arithmetic / logic
  Unary,
  Binary,
  Cmp,

  // Control flow
  Br,
  CondBr,
  Ret,

  // Call
  Call,

  // SSA-only (added after Mem2Reg)
  Phi,

  // Sentinel
  Unknown,
};

/// Unary operators.
enum class UnaryOp : uint8_t {
  Neg,   ///< -x
  Not,   ///< !x (logical not)
};

/// Binary operators.
enum class BinaryOp : uint8_t {
  Add, Sub, Mul, Div, Mod,
  And, Or,       ///< Logical and/or (short-circuit).
  BitAnd, BitOr, ///< Bitwise (if needed).
};

/// Comparison predicates.
enum class CmpPred : uint8_t {
  EQ, NE, LT, LE, GT, GE,
};

/// Base class for all IR instructions.
class Instruction : public Value {
public:
  explicit Instruction(Opcode op) : opcode_(op) {}

  [[nodiscard]] Opcode opcode() const { return opcode_; }
  [[nodiscard]] BlockId parentBlock() const { return parentBlock_; }
  void setParentBlock(BlockId bid) { parentBlock_ = bid; }

  /// Operands (use edges).
  [[nodiscard]] const std::vector<Use>& operands() const { return operands_; }
  void addOperand(Value* v);

protected:
  Opcode opcode_;
  BlockId parentBlock_;
  std::vector<Use> operands_;
};

// ── Concrete instruction classes (P0 stubs) ─────────────────────────────────

/// Slot IR: load from a mutable slot.
class SlotLoadInst : public Instruction {
public:
  SlotLoadInst() : Instruction(Opcode::SlotLoad) {}
  SlotId slot() const { return slot_; }
  void setSlot(SlotId s) { slot_ = s; }
private:
  SlotId slot_;
};

/// Slot IR: store to a mutable slot.
class SlotStoreInst : public Instruction {
public:
  SlotStoreInst() : Instruction(Opcode::SlotStore) {}
  SlotId slot() const { return slot_; }
  Value* value() const { return val_; }
  void setSlot(SlotId s) { slot_ = s; }
  void setValue(Value* v) { val_ = v; }
private:
  SlotId slot_;
  Value* val_ = nullptr;
};

/// Slot IR: load from a global variable.
class GlobalLoadInst : public Instruction {
public:
  GlobalLoadInst() : Instruction(Opcode::GlobalLoad) {}
  GlobalId global() const { return global_; }
  void setGlobal(GlobalId g) { global_ = g; }
private:
  GlobalId global_;
};

/// Slot IR: store to a global variable.
class GlobalStoreInst : public Instruction {
public:
  GlobalStoreInst() : Instruction(Opcode::GlobalStore) {}
  GlobalId global() const { return global_; }
  Value* value() const { return val_; }
  void setGlobal(GlobalId g) { global_ = g; }
  void setValue(Value* v) { val_ = v; }
private:
  GlobalId global_;
  Value* val_ = nullptr;
};

/// Unary operation (neg, not).
class UnaryInst : public Instruction {
public:
  explicit UnaryInst(UnaryOp op) : Instruction(Opcode::Unary), unaryOp_(op) {}
  [[nodiscard]] UnaryOp unaryOp() const { return unaryOp_; }
private:
  UnaryOp unaryOp_;
};

/// Binary operation (add, sub, mul, div, mod, and, or).
class BinaryInst : public Instruction {
public:
  explicit BinaryInst(BinaryOp op) : Instruction(Opcode::Binary), binaryOp_(op) {}
  [[nodiscard]] BinaryOp binaryOp() const { return binaryOp_; }
private:
  BinaryOp binaryOp_;
};

/// Comparison operation.
class CmpInst : public Instruction {
public:
  explicit CmpInst(CmpPred pred) : Instruction(Opcode::Cmp), pred_(pred) {}
  [[nodiscard]] CmpPred predicate() const { return pred_; }
private:
  CmpPred pred_;
};

/// Function call.
class CallInst : public Instruction {
public:
  CallInst() : Instruction(Opcode::Call) {}
  FunctionId callee() const { return callee_; }
  void setCallee(FunctionId f) { callee_ = f; }
private:
  FunctionId callee_;
};

/// Unconditional branch.
class BrInst : public Instruction {
public:
  BrInst() : Instruction(Opcode::Br) {}
  BlockId target() const { return target_; }
  void setTarget(BlockId b) { target_ = b; }
private:
  BlockId target_;
};

/// Conditional branch.
class CondBrInst : public Instruction {
public:
  CondBrInst() : Instruction(Opcode::CondBr) {}
  Value* condition() const { return cond_; }
  BlockId trueTarget() const { return trueTarget_; }
  BlockId falseTarget() const { return falseTarget_; }
  void setCondition(Value* v) { cond_ = v; }
  void setTrueTarget(BlockId b) { trueTarget_ = b; }
  void setFalseTarget(BlockId b) { falseTarget_ = b; }
private:
  Value* cond_ = nullptr;
  BlockId trueTarget_;
  BlockId falseTarget_;
};

/// Return instruction.
class RetInst : public Instruction {
public:
  RetInst() : Instruction(Opcode::Ret) {}
  Value* returnValue() const { return retVal_; }
  void setReturnValue(Value* v) { retVal_ = v; }
private:
  Value* retVal_ = nullptr;  ///< nullptr for void return.
};

/// Phi node (SSA only, added after Mem2Reg).
class PhiInst : public Instruction {
public:
  PhiInst() : Instruction(Opcode::Phi) {}

  struct Incoming {
    Value* value;
    BlockId block;
  };

  void addIncoming(Value* v, BlockId b) { incoming_.push_back({v, b}); }
  [[nodiscard]] const std::vector<Incoming>& incoming() const { return incoming_; }

private:
  std::vector<Incoming> incoming_;
};

} // namespace toyc
