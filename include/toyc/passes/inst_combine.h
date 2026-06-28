#pragma once

#include "toyc/passes/pass.h"

namespace toyc {

class InstCombineLitePass final : public FunctionPass {
public:
  [[nodiscard]] std::string_view name() const override { return "InstCombineLite"; }
  PassResult run(Function& function) override;
};

} // namespace toyc
