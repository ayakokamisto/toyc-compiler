/// Pass manager scaffold — P0 stub implementations.

#include "toyc/passes/pass_manager.h"
#include "toyc/ir/module.h"

namespace toyc {

void PassManager::addPass(std::unique_ptr<Pass> pass) {
  passes_.push_back(std::move(pass));
}

void PassManager::run(Module& module) {
  for (auto& pass : passes_) {
    pass->run(module);
  }
}

} // namespace toyc
