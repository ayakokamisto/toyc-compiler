#pragma once

#include "toyc/mir/mir.h"

#include <vector>

namespace toyc::riscv32 {

struct AllocatedMachineFunction {
  MIRFunction function;
  FrameLayout frameLayout;
  // VRegId.value → physical register name (e.g. "t2", "s1"), empty if spilled.
  std::unordered_map<uint32_t, std::string> regAssignment;
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
