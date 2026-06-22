#pragma once

#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"

namespace toyc::codegen {

// Phase-one conservative allocator: every vreg maps to one stack slot.
// Future register assignment can extend this module without touching emitters.
class RegisterAllocator {
public:
    [[nodiscard]] static StackFrame allocate(const contract::IRFunction& function);
};

} // namespace toyc::codegen
