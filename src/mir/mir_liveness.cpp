/// MIR Liveness Analysis — backward dataflow + live intervals.

#include "toyc/mir/mir_liveness.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <vector>

namespace toyc {

namespace {

constexpr int kLoopDepthOneWeight = 30;
constexpr int kLoopDepthTwoWeight = 300;
constexpr int kCallCrossingWeight = 25;

int blockWeightForDepth(int depth) {
  if (depth <= 0) return 1;
  if (depth == 1) return kLoopDepthOneWeight;
  return kLoopDepthTwoWeight;
}

struct BlockFacts {
  std::unordered_set<uint32_t> defs;
  std::unordered_set<uint32_t> uses;
  std::unordered_set<uint32_t> liveIn;
  std::unordered_set<uint32_t> liveOut;
  std::vector<BlockId> successors;
  int startPos = 0;
  int endPos = 0;
  int loopDepth = 0;
};

void collectDefsUses(const MIRInstruction& inst,
                     std::unordered_set<uint32_t>& defs,
                     std::unordered_set<uint32_t>& uses) {
  auto def = [&](int idx) {
    if (idx < static_cast<int>(inst.operands.size()) &&
        inst.operands[idx].kind == MIROperandKind::VReg)
      defs.insert(inst.operands[idx].vregId().value);
  };
  auto use = [&](int idx) {
    if (idx < static_cast<int>(inst.operands.size()) &&
        inst.operands[idx].kind == MIROperandKind::VReg)
      uses.insert(inst.operands[idx].vregId().value);
  };
  switch (inst.opcode) {
    case MIROpcode::LoadImm: case MIROpcode::Li:
    case MIROpcode::LoadFrame: case MIROpcode::LoadGlobal: case MIROpcode::La:
      def(0); break;
    case MIROpcode::Move:
      def(0); use(1); break;
    case MIROpcode::StoreFrame: case MIROpcode::StoreGlobal:
      use(1); break;
    case MIROpcode::Add: case MIROpcode::Sub: case MIROpcode::Xor:
    case MIROpcode::Or: case MIROpcode::And: case MIROpcode::Sll:
    case MIROpcode::Srl: case MIROpcode::Sra: case MIROpcode::Slt:
    case MIROpcode::Sltu:
      def(0); use(1); use(2); break;
    case MIROpcode::Addi: case MIROpcode::Xori: case MIROpcode::Sltiu:
      def(0); use(1); break;
    case MIROpcode::BranchIfNonZero:
      use(0); break;
    case MIROpcode::Call:
      if (!inst.operands.empty() && inst.operands[0].kind == MIROperandKind::VReg)
        def(0);
      break;
    default: break;
  }
}

std::vector<BlockId> blockSuccessors(const MIRBlock& block) {
  std::vector<BlockId> succs;
  if (block.insts.empty()) return succs;
  const auto& last = block.insts.back();
  if (last.opcode == MIROpcode::Branch && !last.operands.empty() &&
      last.operands[0].kind == MIROperandKind::BlockLabel)
    succs.push_back(last.operands[0].blockLabel());
  if (last.opcode == MIROpcode::BranchIfNonZero && last.operands.size() >= 2 &&
      last.operands[1].kind == MIROperandKind::BlockLabel)
    succs.push_back(last.operands[1].blockLabel());
  // Fall-through: next block in order.
  return succs;
}

bool isCall(const MIRInstruction& inst) { return inst.opcode == MIROpcode::Call; }

} // namespace

MIRLiveness analyzeMIRLiveness(const MIRFunction& func) {
  MIRLiveness result;
  int n = static_cast<int>(func.blocks.size());
  if (n == 0) return result;

  // Map BlockId → index.
  std::unordered_map<uint32_t, int> blockIndex;
  for (int i = 0; i < n; ++i) blockIndex[func.blocks[i].id.value] = i;

  std::vector<BlockFacts> facts(n);
  std::vector<int> callPositions;
  int nextPos = 0;

  // First pass: build defs/uses/successors.
  for (int i = 0; i < n; ++i) {
    const auto& block = func.blocks[i];
    BlockFacts& f = facts[i];
    f.startPos = nextPos;

    for (const auto& inst : block.insts) {
      collectDefsUses(inst, f.defs, f.uses);
      if (isCall(inst)) {
        callPositions.push_back(nextPos);
        if (!inst.operands.empty() && inst.operands[0].kind == MIROperandKind::Immediate) {
          int argCount = inst.operands[0].imm();
          result.maxOutgoingArgCount = std::max(result.maxOutgoingArgCount, argCount);
        }
        // Call defs a0 (the return value goes to a VReg via Move after call).
      }
      ++nextPos;
    }

    f.successors = blockSuccessors(block);
    // Add fall-through if the last instruction is not an unconditional branch or return.
    if (!block.insts.empty()) {
      const auto& last = block.insts.back();
      if (last.opcode != MIROpcode::Branch && last.opcode != MIROpcode::Return &&
          last.opcode != MIROpcode::BranchIfNonZero) {
        // Fall-through: the block control flows to the next block.
        // Actually, BranchIfNonZero DOES fall through. Let me handle this properly.
      }
      if (last.opcode == MIROpcode::BranchIfNonZero && i + 1 < n) {
        f.successors.push_back(func.blocks[i + 1].id);
      }
      if (last.opcode != MIROpcode::Branch && last.opcode != MIROpcode::Return &&
          last.opcode != MIROpcode::BranchIfNonZero && i + 1 < n) {
        f.successors.push_back(func.blocks[i + 1].id);
      }
    }

    f.endPos = nextPos - 1;
    if (f.endPos < f.startPos) f.endPos = f.startPos;
  }

  // Backward dataflow: liveIn = uses ∪ (liveOut \ defs).
  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = n - 1; i >= 0; --i) {
      BlockFacts& f = facts[i];
      std::unordered_set<uint32_t> newLiveOut;
      for (BlockId succ : f.successors) {
        auto it = blockIndex.find(succ.value);
        if (it != blockIndex.end())
          for (uint32_t v : facts[it->second].liveIn) newLiveOut.insert(v);
      }
      std::unordered_set<uint32_t> newLiveIn = f.uses;
      for (uint32_t v : newLiveOut)
        if (!f.defs.contains(v)) newLiveIn.insert(v);
      if (newLiveOut != f.liveOut || newLiveIn != f.liveIn) {
        f.liveOut = std::move(newLiveOut);
        f.liveIn = std::move(newLiveIn);
        changed = true;
      }
    }
  }

  // Compute loop depths: back-edge from block i to j where j <= i.
  for (int i = 0; i < n; ++i) {
    for (BlockId succ : facts[i].successors) {
      auto it = blockIndex.find(succ.value);
      if (it == blockIndex.end()) continue;
      int target = it->second;
      if (target > i) continue;  // forward edge, not a back-edge
      for (int k = target; k <= i; ++k) ++facts[k].loopDepth;
    }
  }
  for (int i = 0; i < n; ++i)
    result.blockLoopDepths[func.blocks[i].id.value] = facts[i].loopDepth;

  // Compute loop-carried VRegs.
  for (int i = 0; i < n; ++i) {
    for (BlockId succ : facts[i].successors) {
      auto it = blockIndex.find(succ.value);
      if (it == blockIndex.end()) continue;
      int header = it->second;
      if (header > i) continue;
      const auto& headerFacts = facts[header];
      for (int k = header; k <= i; ++k) {
        for (uint32_t def : facts[k].defs) {
          if (facts[k].liveOut.contains(def) && headerFacts.liveIn.contains(def))
            result.loopCarriedVRegs.insert(def);
        }
      }
    }
  }

  // Build weighted access counts.
  std::unordered_map<uint32_t, int> accessWeights;
  for (int i = 0; i < n; ++i) {
    const auto& block = func.blocks[i];
    int depth = facts[i].loopDepth;
    int w = blockWeightForDepth(depth);
    for (const auto& inst : block.insts) {
      std::unordered_set<uint32_t> defs, uses;
      collectDefsUses(inst, defs, uses);
      for (uint32_t v : defs) accessWeights[v] += w;
      for (uint32_t v : uses) accessWeights[v] += w;
    }
  }

  // Collect all VRegs.
  for (int i = 0; i < n; ++i) {
    for (const auto& inst : func.blocks[i].insts) {
      std::unordered_set<uint32_t> defs, uses;
      collectDefsUses(inst, defs, uses);
      for (uint32_t v : defs) result.allVRegs.insert(v);
      for (uint32_t v : uses) result.allVRegs.insert(v);
    }
  }
  for (auto pv : func.parameterVRegs) result.allVRegs.insert(pv.value);

  // Build live intervals.
  struct IntervalBuilder { int minP = INT_MAX, maxP = INT_MIN;
    void include(int p) { minP = std::min(minP, p); maxP = std::max(maxP, p); }
    bool seen() const { return minP != INT_MAX; }
  };
  std::unordered_map<uint32_t, IntervalBuilder> builders;
  for (uint32_t v : result.allVRegs) builders[v];

  for (auto pv : func.parameterVRegs) builders[pv.value].include(0);

  nextPos = 0;
  for (int i = 0; i < n; ++i) {
    const auto& block = func.blocks[i];
    // liveIn at block start
    for (uint32_t v : facts[i].liveIn) builders[v].include(nextPos);
    for (const auto& inst : block.insts) {
      std::unordered_set<uint32_t> defs, uses;
      collectDefsUses(inst, defs, uses);
      for (uint32_t v : uses) builders[v].include(nextPos);
      for (uint32_t v : defs) builders[v].include(nextPos);
      ++nextPos;
    }
    // liveOut at block end
    for (uint32_t v : facts[i].liveOut) builders[v].include(nextPos - 1);
  }

  for (uint32_t v : result.allVRegs) {
    auto& b = builders[v];
    if (!b.seen()) continue;
    int useCount = 0;
    for (int i = 0; i < n; ++i) {
      for (const auto& inst : func.blocks[i].insts) {
        std::unordered_set<uint32_t> defs, uses;
        collectDefsUses(inst, defs, uses);
        if (uses.contains(v)) ++useCount;
      }
    }
    int callCross = 0;
    for (int cp : callPositions)
      if (b.minP < cp && cp < b.maxP) ++callCross;
    int spillW = accessWeights[v] + callCross * kCallCrossingWeight;
    result.intervals.push_back({v, b.minP, b.maxP, useCount, spillW, callCross,
                                 result.loopCarriedVRegs.contains(v)});
  }

  return result;
}

} // namespace toyc
