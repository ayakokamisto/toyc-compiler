#pragma once
/// Value — a typed, uniquely-identified result in the ToyC IR.
///
/// ToyC IR is in "canonical slot" form:
///   - Mutable variables are represented by Slots (load.slot / store.slot).
///   - Expression temporaries are uniquely numbered Values.
///   - No def-use chains are maintained in P4.
///
/// This file defines the Value struct (with source discriminant)
/// and the Slot struct (function-local mutable storage).

#include "toyc/ir/ir_type.h"
#include "toyc/support/ids.h"

#include <cstdint>
#include <optional>
#include <string>

namespace toyc {

/// How a Value was produced.
enum class ValueSource : uint8_t {
  Argument,           ///< Function parameter.
  InstructionResult,  ///< Produced by an instruction.
};

/// A typed value with a unique ValueId.
struct Value {
  ValueId id;
  IRType type = I32Type;
  ValueSource source = ValueSource::InstructionResult;

  /// For Argument: the function parameter index.
  /// For InstructionResult: the InstId that produced this value.
  uint32_t sourceIndex = 0;
};

/// The kind of a slot.
enum class SlotKind : uint8_t {
  Parameter,      ///< Created for a function parameter.
  LocalVariable,  ///< Created for a local variable declaration.
  Temporary,      ///< Created for short-circuit boolean materialization.
};

/// A function-local mutable storage location.
/// Slots are the canonical representation of mutable variables before Mem2Reg.
struct Slot {
  SlotId id;
  IRType type = I32Type;
  SlotKind kind = SlotKind::LocalVariable;
  std::optional<SymbolId> sourceSymbol;  ///< None for Temporary slots.
  std::string debugName;                 ///< Human-readable name for IR dump.
};

} // namespace toyc
