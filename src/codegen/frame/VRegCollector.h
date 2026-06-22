#pragma once

#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"

namespace toyc::codegen {

// Scans every vreg referenced by IR instructions and terminators in a function,
// then configures the stack frame's outgoing argument reservation.
class VRegCollector {
public:
    static void collectInto(const contract::IRFunction& function, StackFrame& frame);
};

} // namespace toyc::codegen
