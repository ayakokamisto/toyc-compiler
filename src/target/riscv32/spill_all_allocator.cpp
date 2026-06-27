#include "toyc/target/riscv32/spill_all_allocator.h"

#include <algorithm>

namespace toyc::riscv32 {

void SpillAllAllocator::normalizeSavedReturnAddress(MIRFunction& function) {
  auto isSavedRa = [](const FrameObject& object) {
    return object.kind == FrameObjectKind::SavedReturnAddress;
  };

  if (!function.hasCall) {
    function.frameObjects.erase(
        std::remove_if(function.frameObjects.begin(), function.frameObjects.end(), isSavedRa),
        function.frameObjects.end());
    return;
  }

  auto it = std::find_if(function.frameObjects.begin(), function.frameObjects.end(), isSavedRa);
  if (it == function.frameObjects.end()) {
    FrameObject savedRa;
    savedRa.kind = FrameObjectKind::SavedReturnAddress;
    savedRa.size = 4;
    function.addFrameObject(savedRa);
  }
}

AllocatedMachineModule SpillAllAllocator::allocate(MIRModule module) const {
  AllocatedMachineModule allocated;
  allocated.globals = std::move(module.globals);

  for (auto& function : module.functions) {
    normalizeSavedReturnAddress(function);
    auto layout = FrameLayout::compute(function);
    allocated.functions.push_back(AllocatedMachineFunction{std::move(function), layout});
  }

  return allocated;
}

} // namespace toyc::riscv32
