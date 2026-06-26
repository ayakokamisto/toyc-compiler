#pragma once
/// Use — represents a use of a Value in the IR.
///
/// Forms an intrusive linked list on each Value for def-use tracking.

#include "toyc/support/ids.h"

namespace toyc {

class Value;
class Instruction;

/// A single use edge: "which instruction uses which value."
struct Use {
  Value* val = nullptr;         ///< The value being used.
  Instruction* user = nullptr;  ///< The instruction that uses it.
  Use* nextUse = nullptr;       ///< Next use in the same value's use-list.
  Use* prevNext = nullptr;      ///< Pointer to previous node's nextUse pointer.

  Use() = default;
  Use(Value* v, Instruction* u) : val(v), user(u) {}
};

} // namespace toyc
