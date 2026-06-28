#pragma once
/// PassManager — runs a pipeline of passes on a module.
/// This is a P0 placeholder.

#include "toyc/passes/pass.h"

#include <memory>
#include <ostream>
#include <vector>

namespace toyc {

class Module;

class FunctionPassManager {
public:
  FunctionPassManager() = default;

  void add(std::unique_ptr<FunctionPass> pass);

  [[nodiscard]] bool runToFixedPoint(Function& function, const Module& module,
                                     std::size_t maxIterations, std::ostream& diagnostics);

private:
  std::vector<std::unique_ptr<FunctionPass>> passes_;
};

} // namespace toyc
