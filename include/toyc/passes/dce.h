#pragma once

#include "toyc/passes/pass.h"

namespace toyc {

class DCEPass final : public FunctionPass {
public:
  [[nodiscard]] std::string_view name() const override { return "DCE"; }
  PassResult run(Function& function) override;
};

} // namespace toyc
