/// Register allocator — currently disabled.
/// The emitter uses t0-t3 internally (scratch, result, address).
/// Assigning VRegs to these registers causes conflicts.
/// Will be re-enabled after proper register conflict analysis.

#include "toyc/target/riscv32/reg_allocator.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace toyc::riscv32 {

namespace {
// Register pool definition kept for reference when re-enabling.
} // namespace

AllocatedMachineModule RegisterAllocator::allocate(MIRModule module) {
  AllocatedMachineModule allocated;
  allocated.globals = std::move(module.globals);

  for (auto& func : module.functions) {
    AllocatedMachineFunction af;
    af.function = std::move(func);
    auto& mf = af.function;

    // Register allocation disabled — see file header for rationale.
    (void)enableOpt_;

    // Normalize saved ra.
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
