#pragma once

#include "codegen/BackendOptions.h"
#include "codegen/ContractIR.h"

#include <string>

namespace toyc::codegen {

class RiscvBackend {
public:
    [[nodiscard]] std::string generate(const contract::IRModule& module,
                                       const BackendOptions& options = {});
};

} // namespace toyc::codegen
