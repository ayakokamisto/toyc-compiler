/// Simple register allocator — assigns caller-saved registers to VRegs
/// in order of first appearance.  Enough for basic loop performance.

#include "toyc/target/riscv32/reg_allocator.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace toyc::riscv32 {

namespace {
constexpr const char* kRegs[] = {"t3","t4","t5","t6","a2","a3","a4","a5","a6","a7"};
constexpr int kRegCount = 10;
} // namespace

AllocatedMachineModule RegisterAllocator::allocate(MIRModule module) {
  AllocatedMachineModule allocated;
  allocated.globals = std::move(module.globals);

  for (auto& func : module.functions) {
    AllocatedMachineFunction af;
    af.function = std::move(func);
    auto& mf = af.function;

    if (enableOpt_) {
      // Assign registers to VRegs in order of first appearance across all blocks.
      // Each unique VReg gets the next free register.  When the pool is exhausted
      // remaining VRegs stay spilled.
      std::unordered_map<uint32_t, int> vregToIdx;
      int nextReg = 0;

      for (auto& block : mf.blocks) {
        for (auto& inst : block.insts) {
          for (size_t j = 0; j < inst.operands.size(); ++j) {
            if (inst.operands[j].kind != MIROperandKind::VReg) continue;
            uint32_t v = inst.operands[j].vregId().value;
            if (vregToIdx.count(v)) continue;
            if (nextReg < kRegCount) {
              vregToIdx[v] = nextReg++;
            } else {
              vregToIdx[v] = -1;  // mark as seen but not assigned
            }
          }
        }
      }

      for (auto& [vreg, idx] : vregToIdx)
        if (idx >= 0) af.regAssignment[vreg] = kRegs[idx];
    }

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
