#pragma once

#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"
#include "codegen/frame/VRegAssignment.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace toyc::codegen {

struct RegisterAllocation {
    StackFrame frame;
    VRegAssignment assignment;
};

class RegisterAllocator {
public:
    // Default: every vreg gets a stack slot. With enableOpt: assign live intervals
    // to s1-s11 using linear-scan active-set allocation; spilled intervals stay on stack.
    // Vregs in `excluded` (foldable immediate constants) get neither a register
    // nor a stack slot, since they are never materialized.
    [[nodiscard]] static RegisterAllocation
    allocate(const contract::IRFunction& function,
             bool enableOpt = false,
             const std::unordered_map<std::string, std::int32_t>& excluded = {});
};

} // namespace toyc::codegen
