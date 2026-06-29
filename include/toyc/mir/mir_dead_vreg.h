#pragma once
/// MIR Dead VReg Cleanup — remove unused VReg definitions.
///
/// Scans a MIR function for VReg definitions that are never used.  Removes
/// the defining instruction (if side-effect-free) and its VRegHome from
/// frameObjects, shrinking the stack frame.

#include "toyc/mir/mir.h"

namespace toyc {

/// Remove dead VReg definitions from a MIR function.
/// Returns true if any instruction was removed.
bool cleanupDeadVRegs(MIRFunction& function);

} // namespace toyc
