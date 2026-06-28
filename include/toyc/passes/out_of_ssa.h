#pragma once

#include "toyc/ir/function.h"

namespace toyc {

struct OutOfSSAResult {
  bool changed = false;
  std::size_t loweredPhiCount = 0;
  std::size_t splitEdgeCount = 0;
};

class OutOfSSAPass {
public:
  OutOfSSAResult run(Function& function);
};

} // namespace toyc
