#pragma once

#include "common/optimization_level.h"

namespace toyc::codegen {

struct BackendOptions {
    bool enableOpt = false;
    OptimizationLevel optLevel = OptimizationLevel::O3OJ;
    bool emitComment = false;
};

} // namespace toyc::codegen
