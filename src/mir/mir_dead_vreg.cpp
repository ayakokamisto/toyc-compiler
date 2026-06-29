/// MIR Dead VReg Cleanup — remove unused VReg definitions.

#include "toyc/mir/mir_dead_vreg.h"

#include <algorithm>
#include <unordered_set>

namespace toyc {

namespace {

// Return true if this instruction defines a VReg (operand[0] is a VReg result).
bool definesVReg(const MIRInstruction& inst, VRegId* out) {
  if (inst.operands.empty()) return false;
  if (inst.operands[0].kind != MIROperandKind::VReg) return false;
  *out = inst.operands[0].vregId();
  return true;
}

// Return true if this instruction has observable side effects and must be kept.
bool hasSideEffects(const MIRInstruction& inst) {
  switch (inst.opcode) {
    case MIROpcode::StoreFrame:
    case MIROpcode::StoreGlobal:
    case MIROpcode::Call:
    case MIROpcode::Branch:
    case MIROpcode::BranchIfNonZero:
    case MIROpcode::Return:
      return true;
    default:
      return false;
  }
}

// Collect VRegs used as operands (not defs) in an instruction.
void collectUses(const MIRInstruction& inst, std::unordered_set<uint32_t>& used) {
  auto add = [&](int idx) {
    if (idx < static_cast<int>(inst.operands.size()) &&
        inst.operands[idx].kind == MIROperandKind::VReg) {
      used.insert(inst.operands[idx].vregId().value);
    }
  };
  switch (inst.opcode) {
    case MIROpcode::Move:
    case MIROpcode::StoreFrame:
    case MIROpcode::StoreGlobal:
      add(1);
      break;
    case MIROpcode::Add: case MIROpcode::Sub: case MIROpcode::Xor:
    case MIROpcode::Or: case MIROpcode::And: case MIROpcode::Sll:
    case MIROpcode::Srl: case MIROpcode::Sra: case MIROpcode::Slt:
    case MIROpcode::Sltu:
      add(1); add(2);
      break;
    case MIROpcode::Addi: case MIROpcode::Xori: case MIROpcode::Sltiu:
      add(1);
      break;
    case MIROpcode::BranchIfNonZero:
      add(0);
      break;
    default:
      break;
  }
}

} // namespace

bool cleanupDeadVRegs(MIRFunction& function) {
  // Collect all used VRegs.
  std::unordered_set<uint32_t> used;
  for (const auto& param : function.parameterVRegs) {
    used.insert(param.value);
  }
  for (const auto& block : function.blocks) {
    for (const auto& inst : block.insts) {
      collectUses(inst, used);
    }
  }

  // Remove dead definitions.
  bool changed = false;
  std::unordered_set<uint32_t> deadVRegs;

  for (auto& block : function.blocks) {
    auto& insts = block.insts;
    auto oldSize = insts.size();
    insts.erase(std::remove_if(insts.begin(), insts.end(), [&](const MIRInstruction& inst) {
      VRegId def;
      if (!definesVReg(inst, &def)) return false;
      if (hasSideEffects(inst)) return false;
      if (used.contains(def.value)) return false;
      deadVRegs.insert(def.value);
      return true;
    }), insts.end());
    if (insts.size() != oldSize) changed = true;
  }

  // Remove dead VRegHomes from frameObjects.
  if (!deadVRegs.empty()) {
    function.frameObjects.erase(
        std::remove_if(function.frameObjects.begin(), function.frameObjects.end(),
                       [&](const FrameObject& fo) {
                         return fo.kind == FrameObjectKind::VRegHome &&
                                fo.vregId.has_value() &&
                                deadVRegs.contains(fo.vregId->value);
                       }),
        function.frameObjects.end());
  }

  return changed;
}

} // namespace toyc
