#pragma once

#include "codegen/ContractIR.h"

namespace toyc::codegen {

// Safe, live-range-shortening IR simplifications applied under -opt before
// instruction selection. Intentionally local (per basic block) so it never
// lengthens a live range the way global CSE/LICM would on this spill-heavy
// backend. Passes: constant folding + propagation, copy propagation,
// algebraic identities, and dead-code elimination of pure unused defs.
//
// Deliberately omitted: CSE and LICM. They extend live ranges and, on this
// backend, increase register pressure and stack traffic in hot loops.
class IrOptimizer {
public:
    static void optimize(contract::IRFunction& function);
    static void optimize(contract::IRModule& module);
};

} // namespace toyc::codegen
