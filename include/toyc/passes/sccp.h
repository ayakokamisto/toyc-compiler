#pragma once

#include "toyc/passes/pass.h"

namespace toyc {

class SCCPPass final : public FunctionPass {
public:
  [[nodiscard]] std::string_view name() const override { return "SCCP"; }
  PassResult run(Function& function) override;
};

} // namespace toyc
