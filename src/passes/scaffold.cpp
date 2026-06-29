/// Pass manager scaffold — P0 stub implementations.

#include "toyc/passes/pass_manager.h"

#include "toyc/ir/verifier.h"

namespace toyc {

void FunctionPassManager::add(std::unique_ptr<FunctionPass> pass) {
  passes_.push_back(std::move(pass));
}

bool FunctionPassManager::runToFixedPoint(Function& function, const Module& module,
                                          std::size_t maxIterations, std::ostream& diagnostics) {
  if (function.form() != IRForm::SSA) {
    diagnostics << "internal optimizer diagnostic: FunctionPassManager requires SSA IR\n";
    return false;
  }

  for (std::size_t iteration = 0; iteration < maxIterations; ++iteration) {
    bool changedThisRound = false;
    for (auto& pass : passes_) {
      auto result = pass->run(function);
      changedThisRound = changedThisRound || result.changed;
    }
    auto verify = verifySSAFunction(function, module);
    if (!verify.ok) {
      diagnostics << "internal optimizer diagnostic: fixed-point round " << iteration
                  << " produced invalid SSA IR\n";
      for (const auto& err : verify.errors) diagnostics << "  " << err << "\n";
      return false;
    }
    if (!changedThisRound) return true;
  }
  return true;
}

} // namespace toyc
