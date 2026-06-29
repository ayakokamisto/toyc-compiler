#pragma once

#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"
#include "codegen/frame/VRegAssignment.h"

namespace toyc::codegen {

struct RegisterAllocation {
    StackFrame frame;
    VRegAssignment assignment;
};

class RegisterAllocator {
public:
    // Default: every vreg gets a stack slot. With enableOpt: assign live intervals
    // to s1-s11 using linear-scan active-set allocation; spilled intervals stay on stack.
    [[nodiscard]] static RegisterAllocation allocate(const contract::IRFunction& function,
                                                     bool enableOpt = false);
};

} // namespace toyc::codegen
