#pragma once
/// Pass interface — base class for all optimization and transformation passes.

#include "toyc/ir/function.h"

#include <string_view>

namespace toyc {

struct PassResult {
  bool changed = false;
};

class FunctionPass {
public:
  virtual ~FunctionPass() = default;
  [[nodiscard]] virtual std::string_view name() const = 0;
  virtual PassResult run(Function& function) = 0;
};

} // namespace toyc
