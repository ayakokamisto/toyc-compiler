/// Register allocator — union-find copy propagation + frequency scoring.
///
/// Algorithm adapted from Java ToyC compiler (40 OJ points).
///   1. Union-find over Move instructions (copy propagation).
///   2. Score equivalence classes by use count (loop blocks ×12).
///   3. Assign physical registers to classes with score >= threshold.
///
/// Register partition:
///   t0,t1   → BlockVRegCache (emitter-managed, excluded from pool)
///   t2-t6   → caller-saved pool (8 regs, leaf only)
///   a2-a5   → caller-saved pool extension (4 regs, leaf only)
///   s1-s11  → callee-saved pool (11 regs, leaf functions)
///   a0,a1   → call args / return value (never assigned)
///   t3      → emitter scratch for large offsets (never assigned)

#include "toyc/target/riscv32/reg_allocator.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace toyc::riscv32 {

namespace {

constexpr const char* kCallerSaved[] = {"t2","t3_notused","t4","t5","t6","a2","a3","a4","a5"};
// t3 excluded — used by emitter for large offset address computation
constexpr const char* kCalleeSaved[] = {
    "s1","s2","s3","s4","s5","s6","s7","s8","s9","s10","s11"};
constexpr int kScoreThreshold = 6;
constexpr int kLoopWeight = 12;

bool functionIsLeaf(const MIRFunction& func) {
  for (auto& b : func.blocks)
    for (auto& i : b.insts)
      if (i.opcode == MIROpcode::Call) return false;
  return true;
}

struct UnionFind {
  std::unordered_map<uint32_t, uint32_t> parent;
  uint32_t find(uint32_t x) {
    auto it = parent.find(x);
    if (it == parent.end()) { parent[x] = x; return x; }
    if (it->second != x) it->second = find(it->second);
    return it->second;
  }
  void unite(uint32_t a, uint32_t b) {
    parent[find(a)] = find(b);
  }
};

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
      // Step 1: union-find over Move instructions AND slot forwarding.
      UnionFind uf;

      // 1a. Move instructions (copy propagation).
      for (auto& block : mf.blocks)
        for (auto& inst : block.insts)
          if (inst.opcode == MIROpcode::Move &&
              inst.operands.size() >= 2 &&
              inst.operands[0].kind == MIROperandKind::VReg &&
              inst.operands[1].kind == MIROperandKind::VReg)
            uf.unite(inst.operands[0].vregId().value,
                     inst.operands[1].vregId().value);

      // 1b. Cross-block slot forwarding: unite StoreFrame src VRegs
      // with LoadFrame dst VRegs for the same slot.  This makes the
      // allocator assign the same register, eliminating the lw/sw
      // round-trip in loop headers.
      std::unordered_map<uint32_t, int> blockIndex;
      for (int i = 0; i < static_cast<int>(mf.blocks.size()); ++i)
        blockIndex[mf.blocks[i].id.value] = i;

      for (int bi = 0; bi < static_cast<int>(mf.blocks.size()); ++bi) {
        auto& block = mf.blocks[bi];
        // Find the last StoreFrame to each slot in this block.
        std::unordered_map<int, uint32_t> lastStore; // foIndex → VReg
        for (auto& inst : block.insts) {
          if (inst.opcode == MIROpcode::StoreFrame &&
              inst.operands.size() >= 2 &&
              inst.operands[0].kind == MIROperandKind::FrameSlot &&
              inst.operands[1].kind == MIROperandKind::VReg)
            lastStore[inst.operands[0].frameSlotIndex()] =
                inst.operands[1].vregId().value;
        }
        // For each successor block, unite with the first LoadFrame of same slot.
        for (auto& [foIdx, storeVReg] : lastStore) {
          // Look at successor blocks (next block for fall-through, branch targets).
          std::vector<int> succs;
          if (!block.insts.empty()) {
            const auto& last = block.insts.back();
            if (last.opcode == MIROpcode::Branch && !last.operands.empty() &&
                last.operands[0].kind == MIROperandKind::BlockLabel) {
              auto it = blockIndex.find(last.operands[0].blockLabel().value);
              if (it != blockIndex.end()) succs.push_back(it->second);
            } else if (last.opcode == MIROpcode::BranchIfNonZero &&
                       last.operands.size() >= 2 &&
                       last.operands[1].kind == MIROperandKind::BlockLabel) {
              auto it = blockIndex.find(last.operands[1].blockLabel().value);
              if (it != blockIndex.end()) succs.push_back(it->second);
              // Fall-through.
              if (bi + 1 < static_cast<int>(mf.blocks.size()))
                succs.push_back(bi + 1);
            } else if (last.opcode != MIROpcode::Return) {
              // Fall-through only.
              if (bi + 1 < static_cast<int>(mf.blocks.size()))
                succs.push_back(bi + 1);
            }
          }
          for (int si : succs) {
            for (auto& inst : mf.blocks[si].insts) {
              if (inst.opcode == MIROpcode::LoadFrame &&
                  inst.operands.size() >= 2 &&
                  inst.operands[0].kind == MIROperandKind::VReg &&
                  inst.operands[1].kind == MIROperandKind::FrameSlot &&
                  inst.operands[1].frameSlotIndex() == foIdx) {
                uf.unite(inst.operands[0].vregId().value, storeVReg);
                break; // Only first LoadFrame in successor
              }
            }
          }
        }
      }

      // Step 2: detect loops.
      auto inLoop = detectLoopBlocks(mf);

      // Step 3: score each equivalence class root.
      std::unordered_map<uint32_t, int> score;
      int blk = 0;
      for (auto& block : mf.blocks) {
        int w = inLoop[blk] ? kLoopWeight : 1; ++blk;
        for (auto& inst : block.insts)
          for (size_t j = 0; j < inst.operands.size(); ++j)
            if (inst.operands[j].kind == MIROperandKind::VReg)
              score[uf.find(inst.operands[j].vregId().value)] += w;
      }

      // Step 4: rank roots by score descending.
      std::vector<std::pair<uint32_t, int>> ranked;
      for (auto& [root, s] : score)
        if (s >= kScoreThreshold) ranked.push_back({root, s});
      std::sort(ranked.begin(), ranked.end(),
                [](auto& a, auto& b) { return a.second > b.second; });

      // Step 5: build register pool.
      std::vector<std::string> pool;
      bool leaf = functionIsLeaf(mf);
      // Caller-saved (exclude t2=emitter result, t3=emitter address, t0/t1=cache).
      pool.emplace_back("t4"); pool.emplace_back("t5"); pool.emplace_back("t6");
      pool.emplace_back("a2"); pool.emplace_back("a3"); pool.emplace_back("a4"); pool.emplace_back("a5");
      // Callee-saved (leaf only — non-leaf needs save/restore in prologue/epilogue).
      if (leaf)
        for (auto r : kCalleeSaved) pool.emplace_back(r);

      // Step 6: assign top classes to registers.
      std::unordered_map<uint32_t, std::string> rootToReg;
      for (size_t i = 0; i < ranked.size() && i < pool.size(); ++i)
        rootToReg[ranked[i].first] = pool[i];

      // Step 7: map every VReg to its class's register.
      for (auto& block : mf.blocks)
        for (auto& inst : block.insts)
          for (size_t j = 0; j < inst.operands.size(); ++j)
            if (inst.operands[j].kind == MIROperandKind::VReg) {
              auto it = rootToReg.find(uf.find(inst.operands[j].vregId().value));
              if (it != rootToReg.end())
                af.regAssignment[inst.operands[j].vregId().value] = it->second;
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
