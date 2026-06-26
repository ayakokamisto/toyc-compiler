#pragma once
/// Opcode definitions for ToyC Canonical Slot IR.

#include <cstdint>
#include <string_view>

namespace toyc {

/// Unary operation opcodes.
enum class UnaryOpcode : uint8_t {
  Negate,       ///< -x
  LogicalNot,   ///< !x  (result is 0 or 1)
};

/// Binary operation opcodes.
enum class BinaryOpcode : uint8_t {
  Add,          ///< +
  Subtract,     ///< -
  Multiply,     ///< *
  Divide,       ///< /
  Modulo,       ///< %
};

/// Comparison predicate.
enum class ComparePredicate : uint8_t {
  Equal,        ///< ==
  NotEqual,     ///< !=
  Less,         ///< <
  LessEqual,    ///< <=
  Greater,      ///< >
  GreaterEqual, ///< >=
};

/// Instruction opcodes.
enum class Opcode : uint8_t {
  // Constants
  ConstInt,     ///< %v = const <value>

  // Slot operations
  SlotLoad,     ///< %v = load.slot $slot
  SlotStore,    ///< store.slot $slot, %v

  // Global operations
  GlobalLoad,   ///< %v = load.global @global
  GlobalStore,  ///< store.global @global, %v

  // Arithmetic / logic
  Unary,        ///< %v = <op> %operand
  Binary,       ///< %v = <op> %lhs, %rhs
  Compare,      ///< %v = cmp.<pred> %lhs, %rhs

  // Control flow (terminators)
  Br,           ///< br <target>
  CondBr,       ///< condbr %cond, <true>, <false>
  Ret,          ///< ret [%value]

  // Calls
  Call,         ///< [%v =] call @func(%args...)

  // SSA (future P6)
  Phi,          ///< %v = phi [%val, %block]...
};

constexpr std::string_view opcodeName(Opcode op) {
  switch (op) {
    case Opcode::ConstInt:   return "const";
    case Opcode::SlotLoad:   return "load.slot";
    case Opcode::SlotStore:  return "store.slot";
    case Opcode::GlobalLoad: return "load.global";
    case Opcode::GlobalStore:return "store.global";
    case Opcode::Unary:      return "unary";
    case Opcode::Binary:     return "binary";
    case Opcode::Compare:    return "cmp";
    case Opcode::Br:         return "br";
    case Opcode::CondBr:     return "condbr";
    case Opcode::Ret:        return "ret";
    case Opcode::Call:       return "call";
    case Opcode::Phi:        return "phi";
  }
  return "?";
}

constexpr std::string_view unaryOpcodeName(UnaryOpcode op) {
  switch (op) {
    case UnaryOpcode::Negate:     return "neg";
    case UnaryOpcode::LogicalNot: return "not";
  }
  return "?";
}

constexpr std::string_view binaryOpcodeName(BinaryOpcode op) {
  switch (op) {
    case BinaryOpcode::Add:      return "add";
    case BinaryOpcode::Subtract: return "sub";
    case BinaryOpcode::Multiply: return "mul";
    case BinaryOpcode::Divide:   return "div";
    case BinaryOpcode::Modulo:   return "mod";
  }
  return "?";
}

constexpr std::string_view comparePredicateName(ComparePredicate pred) {
  switch (pred) {
    case ComparePredicate::Equal:        return "eq";
    case ComparePredicate::NotEqual:     return "ne";
    case ComparePredicate::Less:         return "lt";
    case ComparePredicate::LessEqual:    return "le";
    case ComparePredicate::Greater:      return "gt";
    case ComparePredicate::GreaterEqual: return "ge";
  }
  return "?";
}

} // namespace toyc
