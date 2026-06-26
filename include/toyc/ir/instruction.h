#pragma once
/// IR Instruction definitions for ToyC Canonical Slot IR.
///
/// Instructions are value-typed structs stored in BasicBlock's instruction list.
/// Each instruction has a unique InstId and an Opcode.
/// Terminators (Br, CondBr, Ret) are stored separately from ordinary instructions.

#include "toyc/ir/ir_type.h"
#include "toyc/ir/opcode.h"
#include "toyc/support/ids.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace toyc {

/// Discriminated instruction — stores opcode and all possible operand fields.
/// Fields not relevant to the current opcode are ignored.
struct Inst {
  Opcode opcode = Opcode::ConstInt;
  IRType resultType = VoidIRType;
  std::optional<ValueId> result;

  // ConstInt
  int32_t constValue = 0;

  // SlotLoad / SlotStore
  SlotId slot;

  // GlobalLoad / GlobalStore
  GlobalId global;

  // Unary
  UnaryOpcode unaryOp = UnaryOpcode::Negate;
  ValueId unaryOperand;

  // Binary
  BinaryOpcode binaryOp = BinaryOpcode::Add;
  ValueId lhs;
  ValueId rhs;

  // Compare
  ComparePredicate cmpPred = ComparePredicate::Equal;
  ValueId cmpLhs;
  ValueId cmpRhs;

  // Call
  FunctionId callee;
  std::vector<ValueId> arguments;

  // Phi (future P6)
  struct PhiIncoming {
    ValueId value;
    BlockId block;
  };
  std::vector<PhiIncoming> phiIncoming;
};

// ── Terminators ────────────────────────────────────────────────────────────

/// Discriminated terminator — stores opcode and all possible operand fields.
struct Terminator {
  Opcode opcode = Opcode::Br;

  // Branch
  BlockId branchTarget;

  // CondBranch
  ValueId condCondition;
  BlockId condTrueTarget;
  BlockId condFalseTarget;

  // Return
  std::optional<ValueId> returnValue;
};

} // namespace toyc
