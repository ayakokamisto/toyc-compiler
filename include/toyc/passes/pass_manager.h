#pragma once
/// PassManager — runs a pipeline of passes on a module.
/// This is a P0 placeholder.

#include "toyc/passes/pass.h"

#include <memory>
#include <vector>

namespace toyc {

/// Manages and runs a sequence of passes.
class PassManager {
public:
  PassManager() = default;

  /// Add a pass to the pipeline. Takes ownership.
  void addPass(std::unique_ptr<Pass> pass);

  /// Run all passes on the module.
  void run(Module& module);

private:
  std::vector<std::unique_ptr<Pass>> passes_;
};

} // namespace toyc
