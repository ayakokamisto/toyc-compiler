/// Frequency-guided register allocator — assigns a2-a5 to hot VRegs
/// in leaf functions.  t4-t6 are reserved for promoteLeafStackSlots.
///   t0,t1 → BlockVRegCache   t2 → binary results   t3 → address
///   t4,t5,t6 → promoteLeafStackSlots   a2-a5 → this allocator

#include "toyc/target/riscv32/reg_allocator.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace toyc::riscv32 {

namespace {

constexpr const char* kRegs[] = {"a2", "a3", "a4", "a5"};
constexpr int kRegCount = 4;
constexpr int kScoreThreshold = 4;
constexpr int kLoopWeight = 12;

std::vector<bool> detectLoopBlocks(const MIRFunction& func) {
  int n = static_cast<int>(func.blocks.size());
  std::vector<bool> inLoop(n, false);
  std::unordered_map<uint32_t, int> bi;
  for (int i = 0; i < n; ++i) bi[func.blocks[i].id.value] = i;
  for (int i = 0; i < n; ++i) {
    if (func.blocks[i].insts.empty()) continue;
    const auto& last = func.blocks[i].insts.back();
    auto check = [&](BlockId tgt) {
      auto it = bi.find(tgt.value);
      if (it != bi.end() && it->second <= i)
        for (int j = it->second; j <= i; ++j) inLoop[j] = true;
    };
    if (last.opcode == MIROpcode::Branch && !last.operands.empty() &&
        last.operands[0].kind == MIROperandKind::BlockLabel)
      check(last.operands[0].blockLabel());
    if (last.opcode == MIROpcode::BranchIfNonZero && last.operands.size() >= 2 &&
        last.operands[1].kind == MIROperandKind::BlockLabel)
      check(last.operands[1].blockLabel());
  }
  return inLoop;
}

} // namespace

AllocatedMachineModule RegisterAllocator::allocate(MIRModule module) {
  AllocatedMachineModule allocated;
  allocated.globals = std::move(module.globals);

  for (auto& func : module.functions) {
    AllocatedMachineFunction af;
    af.function = std::move(func);
    auto& mf = af.function;

    if (enableOpt_) {
      auto inLoop = detectLoopBlocks(mf);

      // Score each VReg by use frequency (loop ×12, normal ×1).
      std::unordered_map<uint32_t, int> score;
      int blk = 0;
      for (auto& block : mf.blocks) {
        int w = inLoop[blk] ? kLoopWeight : 1; ++blk;
        for (auto& inst : block.insts)
          for (size_t j = 0; j < inst.operands.size(); ++j)
            if (inst.operands[j].kind == MIROperandKind::VReg)
              score[inst.operands[j].vregId().value] += w;
      }

      // Rank by score descending, take top kRegCount above threshold.
      std::vector<std::pair<uint32_t, int>> ranked(score.begin(), score.end());
      std::sort(ranked.begin(), ranked.end(),
                [](auto& a, auto& b) { return a.second > b.second; });

      for (size_t i = 0; i < ranked.size() && i < kRegCount; ++i)
        if (ranked[i].second >= kScoreThreshold)
          af.regAssignment[ranked[i].first] = kRegs[i];
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
