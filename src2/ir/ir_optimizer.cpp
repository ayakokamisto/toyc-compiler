#include "ir/ir_optimizer.h"

#include "ir/ir_passes.h"

namespace toyc::ir {

bool IROptimizer::run(codegen::contract::IRModule& module,
                      const IROptimizerOptions& options) {
    bool changed = false;

    for (auto& function : module.functions) {
        // Iterative fixed-point loop: run passes until no changes or maxIterations.
        for (int iteration = 0; iteration < options.maxIterations; ++iteration) {
            bool iterChanged = false;

            try {

            if (options.enableCopyPropagation) {
                iterChanged |= runCopyPropagation(function);
            }
            if (options.enableConstProp) {
                iterChanged |= runConstProp(function);
            }
            if (options.enableLocalCSE) {
                iterChanged |= runLocalCSE(function);
            }
            if (options.enableDCE) {
                iterChanged |= runDCE(function);
            }
            if (options.enableTailRecursionElimination &&
                options.level == OptimizationLevel::O3OJ) {
                iterChanged |= runTailRecursionElimination(function);
            }

            const bool enableLoopPasses =
                options.enableLICM && options.level == OptimizationLevel::O3OJ;
            if (enableLoopPasses) {
                iterChanged |= runLICM(function);
            }
            if (options.enableDCE) {
                iterChanged |= runDCE(function);
            }

            } catch (...) {
                // If any pass throws, stop iterating for this function.
                break;
            }

            changed |= iterChanged;
            if (!iterChanged) break;  // Fixed point reached.
        }
    }

    return changed;
}

} // namespace toyc::ir
