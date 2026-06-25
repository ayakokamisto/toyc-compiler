#pragma once

#include "codegen/ContractIR.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace toyc::codegen {

// Identifies ConstInst-defined vregs whose every use can be lowered to a
// RISC-V 12-bit immediate operand (addi / slti / seqz-after-addi). Such a
// constant never needs to be materialized into a register or stack slot, which
// removes loop-invariant `li` reloads and the mv shuffles around them.
//
// A vreg qualifies only when:
//   * it is defined exactly once, by a ConstInst, with a value in [-2047, 2047]
//   * it has at least one use
//   * every use site is immediate-eligible and its other operand is not itself
//     a foldable-const candidate (so emission can always fold without needing
//     the value in a register)
std::unordered_map<std::string, std::int32_t>
computeFoldableImmediateConsts(const contract::IRFunction& function);

} // namespace toyc::codegen
