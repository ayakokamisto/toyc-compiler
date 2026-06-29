/// Block-local register allocator for RV32I.
///
/// Assigns caller-saved registers (t2-t6) to VRegs using a per-block,
/// first-seen strategy.  Within a block, each VReg gets a unique register.
/// Cross-block reuse is safe because loadOperand falls back to lw from
/// VRegHome for the first use of a VReg in each block.

#include "toyc/target/riscv32/reg_allocator.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace toyc::riscv32 {

namespace {
constexpr const char* kRegs[] = {"t2","t3","t4","t5","t6"};
constexpr int kRegCount = 5;
} // namespace

AllocatedMachineModule RegisterAllocator::allocate(MIRModule module) {
  AllocatedMachineModule allocated;
  allocated.globals = std::move(module.globals);

  for (auto& func : module.functions) {
    AllocatedMachineFunction af;
    af.function = std::move(func);
    auto& mf = af.function;

    if (enableOpt_) {
      // Block-local assignment: per-block, assign unique registers to VRegs
      // on first encounter (either def or use).  A VReg that appears in
      // multiple blocks may get different registers in each; this is safe
      // because loadOperand always falls back to lw from VRegHome.
      for (const auto& block : mf.blocks) {
        bool used[kRegCount] = {};
        std::unordered_map<uint32_t, int> localMap;

        auto assign = [&](uint32_t vreg) {
          if (localMap.count(vreg)) return;
          for (int i = 0; i < kRegCount; ++i) {
            if (!used[i]) { used[i] = true; localMap[vreg] = i; return; }
          }
        };

        for (const auto& inst : block.insts) {
          // Def (operand[0]).
          if (!inst.operands.empty() &&
              inst.operands[0].kind == MIROperandKind::VReg)
            assign(inst.operands[0].vregId().value);
          // Uses (operand[1+]).
          for (size_t j = 1; j < inst.operands.size(); ++j)
            if (inst.operands[j].kind == MIROperandKind::VReg)
              assign(inst.operands[j].vregId().value);
        }

        for (auto& [vreg, idx] : localMap)
          af.regAssignment[vreg] = kRegs[idx];
      }
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
