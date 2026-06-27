#pragma once

#include <cstddef>

namespace toyc {

class Function;

struct Mem2RegResult {
  bool changed = false;
  std::size_t promotedSlotCount = 0;
  std::size_t insertedPhiCount = 0;
  std::size_t removedLoadCount = 0;
  std::size_t removedStoreCount = 0;
};

class Mem2RegPass {
public:
  Mem2RegResult run(Function& function);
};

} // namespace toyc
