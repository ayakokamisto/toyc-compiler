#pragma once

#include "toyc/passes/pass.h"

namespace toyc {

class SimplifyCFGPass final : public FunctionPass {
public:
  [[nodiscard]] std::string_view name() const override { return "SimplifyCFG"; }
  PassResult run(Function& function) override;
};

} // namespace toyc
