#pragma once
/// MIR Slot Forwarding — block-local StoreFrame→LoadFrame elimination.
///
/// Within a single basic block, if a StoreFrame writes to an IRSlot and a
/// later LoadFrame reads the same slot (with no intervening StoreFrame to
/// that slot), the LoadFrame is replaced with a Move from the stored VReg.
/// This eliminates the lw+sw pair that would otherwise go through the stack.

#include "toyc/mir/mir.h"

namespace toyc {

/// Run slot forwarding on a single MIR function.
/// Returns true if any LoadFrame was replaced.
bool forwardMIRSlots(MIRFunction& function);

} // namespace toyc
