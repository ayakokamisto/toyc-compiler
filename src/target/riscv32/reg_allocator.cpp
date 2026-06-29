/// Frequency-guided register allocator with union-find copy propagation.
///
/// Algorithm (adapted from Java ToyC compiler, 40 pts OJ):
///   1. Union-find: scan Move instructions, union src and dst VRegs.
///   2. Count uses per equivalence class root (loop blocks ×12 weight).
///   3. Assign physical registers to classes with score ≥ 6.
///   4. Leaf functions get up to 20 registers; non-leaf get 11.

#include "toyc/target/riscv32/reg_allocator.h"

#include <algorithm>
#include <numeric>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace toyc::riscv32 {

namespace {

// ── Register pools ────────────────────────────────────────────────────
constexpr const char* kCalleeSaved[] = {
    "s1","s2","s3","s4","s5","s6","s7","s8","s9","s10","s11"};
constexpr const char* kCallerSavedExtra[] = {
    "t3","t4","t5","a2","a3","a4","a5","a6","a7"};
constexpr int kScoreThreshold = 6;
constexpr int kLoopWeight = 12;

bool functionIsLeaf(const MIRFunction& func) {
  for (auto& b : func.blocks)
    for (auto& i : b.insts)
      if (i.opcode == MIROpcode::Call) return false;
  return true;
}

// ── Union-find ────────────────────────────────────────────────────────
struct UnionFind {
  std::unordered_map<uint32_t, uint32_t> parent;
  uint32_t find(uint32_t x) {
    auto it = parent.find(x);
    if (it == parent.end()) { parent[x] = x; return x; }
    if (it->second != x) it->second = find(it->second);
    return it->second;
  }
  void unite(uint32_t a, uint32_t b) {
    uint32_t ra = find(a), rb = find(b);
    if (ra != rb) parent[ra] = rb;
  }
};

// ── Loop detection ────────────────────────────────────────────────────
// Returns vector<bool> where true = block is in a loop body.
std::vector<bool> detectLoopBlocks(const MIRFunction& func) {
  int n = static_cast<int>(func.blocks.size());
  std::vector<bool> inLoop(n, false);
  std::unordered_map<uint32_t, int> blockIndex;
  for (int i = 0; i < n; ++i) blockIndex[func.blocks[i].id.value] = i;

  for (int i = 0; i < n; ++i) {
    const auto& insts = func.blocks[i].insts;
    if (insts.empty()) continue;
    const auto& last = insts.back();
    // Check for back-edge: branch to a block at or before this one.
    if (last.opcode == MIROpcode::Branch && !last.operands.empty() &&
        last.operands[0].kind == MIROperandKind::BlockLabel) {
      auto it = blockIndex.find(last.operands[0].blockLabel().value);
      if (it != blockIndex.end() && it->second <= i) {
        for (int j = it->second; j <= i; ++j) inLoop[j] = true;
      }
    }
    // BranchIfNonZero has a fall-through too; check its target.
    if (last.opcode == MIROpcode::BranchIfNonZero && last.operands.size() >= 2 &&
        last.operands[1].kind == MIROperandKind::BlockLabel) {
      auto it = blockIndex.find(last.operands[1].blockLabel().value);
      if (it != blockIndex.end() && it->second <= i) {
        for (int j = it->second; j <= i; ++j) inLoop[j] = true;
      }
    }
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
      // Step 1: union-find over Move instructions.
      UnionFind uf;
      for (auto& block : mf.blocks) {
        for (auto& inst : block.insts) {
          if (inst.opcode == MIROpcode::Move &&
              inst.operands.size() >= 2 &&
              inst.operands[0].kind == MIROperandKind::VReg &&
              inst.operands[1].kind == MIROperandKind::VReg) {
            uf.unite(inst.operands[0].vregId().value,
                     inst.operands[1].vregId().value);
          }
        }
      }

      // Step 2: detect loop blocks.
      auto inLoop = detectLoopBlocks(mf);

      // Step 3: score each VReg (aggregated to equivalence class root).
      std::unordered_map<uint32_t, int> score;
      int blockIdx = 0;
      for (auto& block : mf.blocks) {
        int weight = inLoop[blockIdx] ? kLoopWeight : 1;
        ++blockIdx;
        for (auto& inst : block.insts) {
          // Count all VReg uses (operands[1+]).
          for (size_t j = 0; j < inst.operands.size(); ++j) {
            if (inst.operands[j].kind == MIROperandKind::VReg) {
              uint32_t v = inst.operands[j].vregId().value;
              score[uf.find(v)] += weight;
            }
          }
        }
      }

      // Step 4: collect roots with score ≥ threshold, sort descending.
      std::vector<std::pair<uint32_t, int>> ranked;
      for (auto& [root, s] : score)
        if (s >= kScoreThreshold) ranked.push_back({root, s});
      std::sort(ranked.begin(), ranked.end(),
                [](auto& a, auto& b) { return a.second > b.second; });

      // Step 5: build register pool.
      std::vector<const char*> pool;
      for (auto r : kCalleeSaved) pool.push_back(r);
      if (functionIsLeaf(mf))
        for (auto r : kCallerSavedExtra) pool.push_back(r);

      // Step 6: assign registers to top-scoring classes.
      // Also map ALL members of the equivalence class to the same register.
      std::unordered_map<uint32_t, std::string> rootToReg;
      for (size_t ri = 0; ri < ranked.size() && ri < pool.size(); ++ri)
        rootToReg[ranked[ri].first] = pool[ri];

      for (auto& block : mf.blocks) {
        for (auto& inst : block.insts) {
          auto assign = [&](int idx) {
            if (idx < static_cast<int>(inst.operands.size()) &&
                inst.operands[idx].kind == MIROperandKind::VReg) {
              uint32_t v = inst.operands[idx].vregId().value;
              auto it = rootToReg.find(uf.find(v));
              if (it != rootToReg.end())
                af.regAssignment[v] = it->second;
            }
          };
          assign(0);  // def
          for (size_t j = 1; j < inst.operands.size(); ++j) assign(static_cast<int>(j));
        }
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
