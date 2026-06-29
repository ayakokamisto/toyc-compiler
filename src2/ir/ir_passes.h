#pragma once

#include "codegen/ContractIR.h"

// IR-level optimization passes operating on contract::IRModule.
// Each pass returns true if it modified the function.

namespace toyc::ir {

// Single-block forward copy propagation.
// Replaces uses of vregs that were produced by CopyInst with their source,
// then removes the CopyInst.
[[nodiscard]] bool runCopyPropagation(codegen::contract::IRFunction& function);

// Single-block local value numbering CSE.
// Removes redundant pure-computation instructions whose operands match
// an earlier instruction in the same block.
[[nodiscard]] bool runLocalCSE(codegen::contract::IRFunction& function);

// Conservative dead-code elimination.
// Removes pure-computation instructions whose result vreg has zero uses,
// cascading when source use-counts drop to zero.
[[nodiscard]] bool runDCE(codegen::contract::IRFunction& function);

// Tail recursion elimination for direct self-recursive tail calls.
// Rewrites "call self(args); return result" into parameter updates plus a
// jump to an internal loop header after the function prologue.
[[nodiscard]] bool runTailRecursionElimination(codegen::contract::IRFunction& function);

// Loop Invariant Code Motion.
// Hoists loop-invariant pure computations from natural loops into preheader blocks.
[[nodiscard]] bool runLICM(codegen::contract::IRFunction& function);

// Constant propagation, folding, and algebraic simplification.
// Tracks known constants within each block, folds constant expressions,
// and simplifies algebraic identities (x+0→x, x*1→x, x*0→0).
[[nodiscard]] bool runConstProp(codegen::contract::IRFunction& function);

} // namespace toyc::ir
