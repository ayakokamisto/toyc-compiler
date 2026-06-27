#pragma once

#include "toyc/mir/mir.h"

#include <vector>

namespace toyc::riscv32 {

struct AllocatedMachineFunction {
  MIRFunction function;
  FrameLayout frameLayout;
};

struct AllocatedMachineModule {
  std::vector<IRGlobal> globals;
  std::vector<AllocatedMachineFunction> functions;
};

class SpillAllAllocator {
public:
  AllocatedMachineModule allocate(MIRModule module) const;

private:
  static void normalizeSavedReturnAddress(MIRFunction& function);
};

} // namespace toyc::riscv32
