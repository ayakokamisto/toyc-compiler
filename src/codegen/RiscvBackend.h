#pragma once

#include "codegen/ContractIR.h"

#include <string>

namespace toyc::codegen {

struct BackendOptions {
    bool enableOpt = false;
    bool emitComment = false;
};

class RiscvBackend {
public:
    [[nodiscard]] std::string generate(const contract::IRModule& module,
                                       const BackendOptions& options = {});
};

} // namespace toyc::codegen
