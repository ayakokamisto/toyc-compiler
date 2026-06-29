#pragma once
/// MIR Copy Propagation — eliminate redundant Move instructions.
///
/// Within each basic block, tracks VReg copy chains and replaces uses
/// of the destination VReg with the source VReg.  Removes Move
/// instructions whose destination becomes unused.

#include "toyc/mir/mir.h"

namespace toyc {

/// Run copy propagation on a single MIR function.
/// Returns true if any Move was removed.
bool propagateMIRCopies(MIRFunction& function);

} // namespace toyc
