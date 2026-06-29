#pragma once

#include "codegen/ContractIR.h"

#include <string_view>

namespace toyc::codegen {

[[nodiscard]] bool isBranchCondUsedInTargets(const contract::IRFunction& function,
                                             const contract::BranchInst& branch,
                                             std::string_view branchBlockLabel = {});

} // namespace toyc::codegen
