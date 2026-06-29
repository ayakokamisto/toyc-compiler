#pragma once

#include "codegen/ContractIR.h"
#include "common/optimization_level.h"

#include <vector>

namespace toyc::ir {

struct IROptimizerOptions {
    OptimizationLevel level = OptimizationLevel::O3OJ;
    bool enableCopyPropagation = true;
    bool enableConstProp = true;
    bool enableLocalCSE = true;
    bool enableDCE = true;
    bool enableTailRecursionElimination = true;
    bool enableLICM = false;
    int maxIterations = 2;
};

struct OptimizationPassReport {
    const char* name = "";
    bool changed = false;
    OptimizationLevel level = OptimizationLevel::O1Safe;
};

struct OptimizationReport {
    bool changed = false;
    bool verified = true;
    std::vector<OptimizationPassReport> passes;
};

class IROptimizer {
public:
    // Returns true if the module was modified.
    [[nodiscard]] bool run(codegen::contract::IRModule& module,
                           const IROptimizerOptions& options = {});
};

} // namespace toyc::ir
