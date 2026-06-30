#include "toyc/mir/mir_dead_writeback.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace toyc {

namespace {

bool isEligibleFrameSlot(const MIRFunction& function, int frameSlot) {
  if (frameSlot < 0 || frameSlot >= static_cast<int>(function.frameObjects.size())) {
    return false;
  }
  const auto kind = function.frameObjects[static_cast<size_t>(frameSlot)].kind;
  return kind == FrameObjectKind::VRegHome || kind == FrameObjectKind::IRSlot;
}

bool isStoreFrameToEligibleSlot(const MIRFunction& function,
                                const MIRInstruction& inst,
                                int* frameSlot) {
  if (inst.opcode != MIROpcode::StoreFrame || inst.operands.size() < 2) {
    return false;
  }
  if (inst.operands[0].kind != MIROperandKind::FrameSlot) {
    return false;
  }
  int slot = inst.operands[0].frameSlotIndex();
  if (!isEligibleFrameSlot(function, slot)) {
    return false;
  }
  *frameSlot = slot;
  return true;
}

bool isLoadFrame(const MIRInstruction& inst) {
  return inst.opcode == MIROpcode::LoadFrame;
}

bool isHardBoundary(const MIRInstruction& inst) {
  switch (inst.opcode) {
    case MIROpcode::Call:
    case MIROpcode::Return:
    case MIROpcode::Branch:
    case MIROpcode::BranchIfNonZero:
    case MIROpcode::LoadGlobal:
    case MIROpcode::StoreGlobal:
    case MIROpcode::La:
      return true;
    default:
      return false;
  }
}

bool definesVRegHomeCandidate(const MIRInstruction& inst, VRegId* out) {
  if (inst.operands.empty() || inst.operands[0].kind != MIROperandKind::VReg) {
    return false;
  }
  switch (inst.opcode) {
    case MIROpcode::LoadFrame:
    case MIROpcode::LoadGlobal:
    case MIROpcode::La:
    case MIROpcode::Call:
    case MIROpcode::Return:
    case MIROpcode::Branch:
    case MIROpcode::BranchIfNonZero:
    case MIROpcode::StoreFrame:
    case MIROpcode::StoreGlobal:
    case MIROpcode::Comment:
      return false;
    default:
      *out = inst.operands[0].vregId();
      return true;
  }
}

void collectVRegUses(const MIRInstruction& inst, std::vector<VRegId>& uses) {
  auto add = [&](int index) {
    if (index < static_cast<int>(inst.operands.size()) &&
        inst.operands[static_cast<size_t>(index)].kind == MIROperandKind::VReg) {
      uses.push_back(inst.operands[static_cast<size_t>(index)].vregId());
    }
  };

  switch (inst.opcode) {
    case MIROpcode::Move:
    case MIROpcode::StoreFrame:
    case MIROpcode::StoreGlobal:
      add(1);
      break;
    case MIROpcode::Add:
    case MIROpcode::Sub:
    case MIROpcode::Xor:
    case MIROpcode::Or:
    case MIROpcode::And:
    case MIROpcode::Sll:
    case MIROpcode::Srl:
    case MIROpcode::Sra:
    case MIROpcode::Slt:
    case MIROpcode::Sltu:
      add(1);
      add(2);
      break;
    case MIROpcode::Addi:
    case MIROpcode::Xori:
    case MIROpcode::Sltiu:
      add(1);
      break;
    case MIROpcode::BranchIfNonZero:
      add(0);
      break;
    default:
      break;
  }
}

struct DefInfo {
  int blockIndex = -1;
  int instIndex = -1;
  int segment = -1;
  bool invalid = false;
  bool hasUse = false;
};

bool markBlockLocalVRegHomeSuppression(MIRFunction& function,
                                       BlockLocalDeadWritebackStats* stats) {
  bool changed = false;
  std::vector<std::vector<int>> segments(function.blocks.size());
  std::unordered_map<uint32_t, DefInfo> defs;

  for (int blockIndex = 0; blockIndex < static_cast<int>(function.blocks.size()); ++blockIndex) {
    const auto& block = function.blocks[static_cast<size_t>(blockIndex)];
    auto& blockSegments = segments[static_cast<size_t>(blockIndex)];
    blockSegments.resize(block.insts.size());

    int segment = 0;
    for (int instIndex = 0; instIndex < static_cast<int>(block.insts.size()); ++instIndex) {
      const auto& inst = block.insts[static_cast<size_t>(instIndex)];
      blockSegments[static_cast<size_t>(instIndex)] = segment;

      VRegId def;
      if (definesVRegHomeCandidate(inst, &def)) {
        auto [it, inserted] = defs.emplace(def.value, DefInfo{blockIndex, instIndex, segment});
        if (!inserted) {
          it->second.invalid = true;
        }
      }

      if (isHardBoundary(inst) || isLoadFrame(inst)) {
        ++segment;
      }
    }
  }

  for (int blockIndex = 0; blockIndex < static_cast<int>(function.blocks.size()); ++blockIndex) {
    const auto& block = function.blocks[static_cast<size_t>(blockIndex)];
    const auto& blockSegments = segments[static_cast<size_t>(blockIndex)];
    for (int instIndex = 0; instIndex < static_cast<int>(block.insts.size()); ++instIndex) {
      const auto& inst = block.insts[static_cast<size_t>(instIndex)];
      std::vector<VRegId> uses;
      collectVRegUses(inst, uses);
      for (auto use : uses) {
        auto it = defs.find(use.value);
        if (it == defs.end()) {
          continue;
        }
        auto& def = it->second;
        def.hasUse = true;
        if (isHardBoundary(inst) || isLoadFrame(inst) ||
            def.blockIndex != blockIndex ||
            def.segment != blockSegments[static_cast<size_t>(instIndex)] ||
            instIndex <= def.instIndex) {
          def.invalid = true;
        }
      }
    }
  }

  for (auto& [vreg, def] : defs) {
    (void)vreg;
    if (def.invalid || !def.hasUse) {
      continue;
    }
    auto& inst = function.blocks[static_cast<size_t>(def.blockIndex)]
                     .insts[static_cast<size_t>(def.instIndex)];
    if (!inst.suppressVRegHomeStore) {
      inst.suppressVRegHomeStore = true;
      changed = true;
      if (stats) {
        ++stats->vregHomeWritebacksSuppressed;
      }
    }
  }
  return changed;
}

} // namespace

bool eliminateBlockLocalDeadWritebacks(MIRFunction& function,
                                       BlockLocalDeadWritebackStats* stats) {
  bool changed = false;

  for (auto& block : function.blocks) {
    std::unordered_map<int, int> nextStoreForSlot;
    std::unordered_set<int> removeIndices;

    for (int i = static_cast<int>(block.insts.size()) - 1; i >= 0; --i) {
      const auto& inst = block.insts[static_cast<size_t>(i)];

      if (isHardBoundary(inst) || isLoadFrame(inst)) {
        nextStoreForSlot.clear();
        continue;
      }

      int frameSlot = -1;
      if (!isStoreFrameToEligibleSlot(function, inst, &frameSlot)) {
        continue;
      }

      auto covering = nextStoreForSlot.find(frameSlot);
      if (covering != nextStoreForSlot.end()) {
        removeIndices.insert(i);
        changed = true;
        if (stats) {
          stats->deadWritebacksRemoved++;
          stats->removals.push_back(DeadWritebackRemoval{
              function.name,
              block.label,
              frameSlot,
              i,
              covering->second,
          });
        }
      }

      nextStoreForSlot[frameSlot] = i;
    }

    if (removeIndices.empty()) {
      continue;
    }

    std::vector<MIRInstruction> kept;
    kept.reserve(block.insts.size() - removeIndices.size());
    for (int i = 0; i < static_cast<int>(block.insts.size()); ++i) {
      if (!removeIndices.contains(i)) {
        kept.push_back(std::move(block.insts[static_cast<size_t>(i)]));
      }
    }
    block.insts = std::move(kept);
  }

  changed |= markBlockLocalVRegHomeSuppression(function, stats);

  return changed;
}

bool eliminateBlockLocalDeadWritebacks(MIRModule& module,
                                       BlockLocalDeadWritebackStats* stats) {
  bool changed = false;
  for (auto& function : module.functions) {
    changed |= eliminateBlockLocalDeadWritebacks(function, stats);
  }
  return changed;
}

} // namespace toyc
