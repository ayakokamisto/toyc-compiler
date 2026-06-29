/// MIR Slot Forwarding — block-local StoreFrame→LoadFrame elimination.

#include "toyc/mir/mir_slot_forwarding.h"

#include <unordered_map>

namespace toyc {

bool forwardMIRSlots(MIRFunction& function) {
  bool changed = false;

  for (auto& block : function.blocks) {
    // Slot → VReg mapping, valid only within this block.
    // Key = FrameObject index (operand[0] for StoreFrame, operand[1] for LoadFrame).
    std::unordered_map<int, VRegId> slotToVReg;

    for (auto& inst : block.insts) {
      if (inst.opcode == MIROpcode::StoreFrame) {
        // StoreFrame [frameSlot], %src
        if (!inst.operands.empty() &&
            inst.operands[0].kind == MIROperandKind::FrameSlot &&
            inst.operands.size() >= 2 &&
            inst.operands[1].kind == MIROperandKind::VReg) {
          int foIndex = inst.operands[0].frameSlotIndex();
          slotToVReg[foIndex] = inst.operands[1].vregId();
        }
      } else if (inst.opcode == MIROpcode::LoadFrame) {
        // LoadFrame %dst, [frameSlot]
        if (inst.operands.size() >= 2 &&
            inst.operands[0].kind == MIROperandKind::VReg &&
            inst.operands[1].kind == MIROperandKind::FrameSlot) {
          int foIndex = inst.operands[1].frameSlotIndex();
          auto it = slotToVReg.find(foIndex);
          if (it != slotToVReg.end()) {
            // Replace with Move %dst, %cachedVReg.
            VRegId dstVReg = inst.operands[0].vregId();
            inst.opcode = MIROpcode::Move;
            inst.operands = {MIROperand::makeVReg(dstVReg),
                             MIROperand::makeVReg(it->second)};
            inst.comment.clear();
            changed = true;
          }
        }
      }
      // Note: no need to invalidate slotToVReg for other instructions.
      // A VReg redefinition (Add, LoadImm, etc.) creates a NEW VReg and
      // doesn't change the slot's value.  Only a new StoreFrame (handled
      // above) can update a slot's value.
    }
  }

  return changed;
}

} // namespace toyc
