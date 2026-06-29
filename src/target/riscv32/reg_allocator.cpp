/// Register allocator — currently a pass-through to SpillAll behaviour.
/// The BlockVRegCache (t0/t1 in asm_emitter) provides intra-block forwarding.
/// Full register allocation is deferred until spike-based testing is available.

#include "toyc/target/riscv32/reg_allocator.h"

#include <algorithm>

namespace toyc::riscv32 {

AllocatedMachineModule RegisterAllocator::allocate(MIRModule module) {
  AllocatedMachineModule allocated;
  allocated.globals = std::move(module.globals);

  for (auto& func : module.functions) {
    AllocatedMachineFunction af;
    af.function = std::move(func);
    auto& mf = af.function;

    // No register assignment yet — BlockVRegCache handles t0/t1 intra-block.

    // Normalize saved ra (same as SpillAllAllocator).
    {
      auto isRa = [](const FrameObject& o) {
        return o.kind == FrameObjectKind::SavedReturnAddress;
      };
      if (!mf.hasCall)
        mf.frameObjects.erase(
            std::remove_if(mf.frameObjects.begin(), mf.frameObjects.end(), isRa),
            mf.frameObjects.end());
      else if (std::find_if(mf.frameObjects.begin(), mf.frameObjects.end(), isRa) ==
               mf.frameObjects.end()) {
        FrameObject ra;
        ra.kind = FrameObjectKind::SavedReturnAddress;
        ra.size = 4;
        mf.addFrameObject(ra);
      }
    }

    af.frameLayout = FrameLayout::compute(mf);
    allocated.functions.push_back(std::move(af));
  }

  return allocated;
}

} // namespace toyc::riscv32
