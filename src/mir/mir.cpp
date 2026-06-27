/// MIR implementation — real data model for P5 RV32 backend.

#include "toyc/mir/mir.h"

#include <algorithm>
#include <cassert>
#include <sstream>

namespace toyc {

// ── Physical register names ─────────────────────────────────────────────────

const char* physRegName(RV32PhysReg reg) {
  switch (reg) {
    case RV32PhysReg::Zero: return "zero";
    case RV32PhysReg::Ra:   return "ra";
    case RV32PhysReg::Sp:   return "sp";
    case RV32PhysReg::T0:   return "t0";
    case RV32PhysReg::T1:   return "t1";
    case RV32PhysReg::T2:   return "t2";
    case RV32PhysReg::T3:   return "t3";
    case RV32PhysReg::T4:   return "t4";
    case RV32PhysReg::T5:   return "t5";
    case RV32PhysReg::T6:   return "t6";
    case RV32PhysReg::A0:   return "a0";
    case RV32PhysReg::A1:   return "a1";
    case RV32PhysReg::A2:   return "a2";
    case RV32PhysReg::A3:   return "a3";
    case RV32PhysReg::A4:   return "a4";
    case RV32PhysReg::A5:   return "a5";
    case RV32PhysReg::A6:   return "a6";
    case RV32PhysReg::A7:   return "a7";
  }
  return "???";
}

// ── MIROperand factories ────────────────────────────────────────────────────

MIROperand MIROperand::makeVReg(VRegId id) {
  MIROperand op;
  op.kind = MIROperandKind::VReg;
  op.data0 = static_cast<int32_t>(id.value);
  return op;
}

MIROperand MIROperand::makePhysReg(RV32PhysReg reg) {
  MIROperand op;
  op.kind = MIROperandKind::PhysReg;
  op.data0 = static_cast<int32_t>(reg);
  return op;
}

MIROperand MIROperand::makeImm(int32_t val) {
  MIROperand op;
  op.kind = MIROperandKind::Immediate;
  op.data0 = val;
  return op;
}

MIROperand MIROperand::makeFrameSlot(int32_t slotIndex) {
  MIROperand op;
  op.kind = MIROperandKind::FrameSlot;
  op.data0 = slotIndex;
  return op;
}

MIROperand MIROperand::makeGlobal(GlobalId id) {
  MIROperand op;
  op.kind = MIROperandKind::GlobalSymbol;
  op.data0 = static_cast<int32_t>(id.value);
  return op;
}

MIROperand MIROperand::makeBlockLabel(BlockId id) {
  MIROperand op;
  op.kind = MIROperandKind::BlockLabel;
  op.data0 = static_cast<int32_t>(id.value);
  return op;
}

VRegId MIROperand::vregId() const {
  assert(kind == MIROperandKind::VReg);
  return VRegId(static_cast<uint32_t>(data0));
}

RV32PhysReg MIROperand::physReg() const {
  assert(kind == MIROperandKind::PhysReg);
  return static_cast<RV32PhysReg>(data0);
}

int32_t MIROperand::imm() const {
  assert(kind == MIROperandKind::Immediate);
  return data0;
}

int32_t MIROperand::frameSlotIndex() const {
  assert(kind == MIROperandKind::FrameSlot);
  return data0;
}

GlobalId MIROperand::globalId() const {
  assert(kind == MIROperandKind::GlobalSymbol);
  return GlobalId(static_cast<uint32_t>(data0));
}

BlockId MIROperand::blockLabel() const {
  assert(kind == MIROperandKind::BlockLabel);
  return BlockId(static_cast<uint32_t>(data0));
}

// ── MIR opcode names ────────────────────────────────────────────────────────

const char* mirOpcodeName(MIROpcode op) {
  switch (op) {
    case MIROpcode::Comment:          return "comment";
    case MIROpcode::LoadImm:          return "li";
    case MIROpcode::Li:               return "li";
    case MIROpcode::Move:             return "mv";
    case MIROpcode::LoadFrame:        return "load_frame";
    case MIROpcode::StoreFrame:       return "store_frame";
    case MIROpcode::LoadGlobal:       return "load_global";
    case MIROpcode::StoreGlobal:      return "store_global";
    case MIROpcode::Add:              return "add";
    case MIROpcode::Sub:              return "sub";
    case MIROpcode::Xor:              return "xor";
    case MIROpcode::Or:               return "or";
    case MIROpcode::And:              return "and";
    case MIROpcode::Sll:              return "sll";
    case MIROpcode::Srl:              return "srl";
    case MIROpcode::Sra:              return "sra";
    case MIROpcode::Slt:              return "slt";
    case MIROpcode::Sltu:             return "sltu";
    case MIROpcode::Addi:             return "addi";
    case MIROpcode::Xori:             return "xori";
    case MIROpcode::Sltiu:            return "sltiu";
    case MIROpcode::Call:             return "call";
    case MIROpcode::Return:           return "ret";
    case MIROpcode::Branch:           return "j";
    case MIROpcode::BranchIfNonZero:  return "bnez";
    case MIROpcode::La:               return "la";
  }
  return "???";
}

// ── MIRInstruction ──────────────────────────────────────────────────────────

MIRInstruction MIRInstruction::make(MIROpcode op, std::vector<MIROperand> ops) {
  MIRInstruction inst;
  inst.opcode = op;
  inst.operands = std::move(ops);
  return inst;
}

MIRInstruction MIRInstruction::makeComment(std::string text) {
  MIRInstruction inst;
  inst.opcode = MIROpcode::Comment;
  inst.comment = std::move(text);
  return inst;
}

// ── MIRFunction ─────────────────────────────────────────────────────────────

VRegId MIRFunction::allocVReg() {
  return VRegId(static_cast<uint32_t>(nextVReg_++));
}

int MIRFunction::addFrameObject(FrameObject obj) {
  int index = static_cast<int>(frameObjects.size());
  frameObjects.push_back(std::move(obj));
  return index;
}

const FrameObject* MIRFunction::findVRegHome(VRegId vreg) const {
  for (const auto& fo : frameObjects) {
    if (fo.kind == FrameObjectKind::VRegHome && fo.vregId.has_value() && *fo.vregId == vreg) {
      return &fo;
    }
  }
  return nullptr;
}

const FrameObject* MIRFunction::findIRSlot(SlotId slot) const {
  for (const auto& fo : frameObjects) {
    if (fo.kind == FrameObjectKind::IRSlot && fo.irSlot.has_value() && *fo.irSlot == slot) {
      return &fo;
    }
  }
  return nullptr;
}

MIRBlock* MIRFunction::entryBlock() {
  if (blocks.empty()) return nullptr;
  return &blocks[entryBlockIndex];
}

const MIRBlock* MIRFunction::entryBlock() const {
  if (blocks.empty()) return nullptr;
  return &blocks[entryBlockIndex];
}

MIRBlock* MIRFunction::findBlock(BlockId id) {
  for (auto& blk : blocks) {
    if (blk.id == id) return &blk;
  }
  return nullptr;
}

const MIRBlock* MIRFunction::findBlock(BlockId id) const {
  for (const auto& blk : blocks) {
    if (blk.id == id) return &blk;
  }
  return nullptr;
}

// ── MIRModule ───────────────────────────────────────────────────────────────

MIRFunction* MIRModule::findFunction(FunctionId id) {
  for (auto& f : functions) {
    if (f.sourceFuncId == id) return &f;
  }
  return nullptr;
}

const MIRFunction* MIRModule::findFunction(FunctionId id) const {
  for (const auto& f : functions) {
    if (f.sourceFuncId == id) return &f;
  }
  return nullptr;
}

MIRFunction* MIRModule::findFunctionByName(const std::string& name) {
  for (auto& f : functions) {
    if (f.name == name) return &f;
  }
  return nullptr;
}

// ── FrameLayout ─────────────────────────────────────────────────────────────

static int32_t alignUp(int32_t value, int32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

FrameLayout FrameLayout::compute(MIRFunction& func) {
  FrameLayout layout;

  // 1. Outgoing argument area: max(8, maxOutgoingArgCount) slots for the first 8
  //    are in registers; only args beyond 8 need stack space.
  int stackArgs = std::max(0, func.maxOutgoingArgCount - 8);
  layout.outgoingArgSize = stackArgs * 4;
  for (auto& fo : func.frameObjects) {
    if (fo.kind == FrameObjectKind::OutgoingArgument) {
      fo.offset = (fo.paramIndex - 8) * 4;
    }
  }

  // 2. IR slots
  int32_t irOffset = layout.outgoingArgSize;
  for (auto& fo : func.frameObjects) {
    if (fo.kind == FrameObjectKind::IRSlot) {
      fo.offset = irOffset;
      irOffset += fo.size;
    }
  }
  layout.irSlotSize = irOffset - layout.outgoingArgSize;

  // 3. VReg homes
  int32_t vregOffset = irOffset;
  for (auto& fo : func.frameObjects) {
    if (fo.kind == FrameObjectKind::VRegHome) {
      fo.offset = vregOffset;
      vregOffset += fo.size;
    }
  }
  layout.vregHomeSize = vregOffset - irOffset;

  // 4. Saved ra (if function has calls)
  int32_t raOffset = vregOffset;
  if (func.hasCall) {
    for (auto& fo : func.frameObjects) {
      if (fo.kind == FrameObjectKind::SavedReturnAddress) {
        fo.offset = raOffset;
        raOffset += 4;
      }
    }
    layout.savedRaSize = 4;
  }

  // 5. Align to 16 bytes
  layout.totalSize = alignUp(raOffset, 16);

  return layout;
}

int32_t FrameLayout::outgoingArgOffset(int paramIndex) const {
  // Stack args start at index 8. They're at the bottom of the frame.
  return (paramIndex - 8) * 4;
}

int32_t FrameLayout::incomingArgOffset(int paramIndex) const {
  // The caller's sp is at current sp + totalSize.
  // Stack args are at caller's sp + (paramIndex - 8) * 4.
  return totalSize + (paramIndex - 8) * 4;
}

// ── MIR verification ────────────────────────────────────────────────────────

MIRVerificationResult verifyMIR(const MIRModule& module) {
  MIRVerificationResult result;

  for (const auto& func : module.functions) {
    // Check that all branch targets exist.
    std::vector<BlockId> blockIds;
    for (const auto& blk : func.blocks) {
      blockIds.push_back(blk.id);
    }

    auto blockExists = [&](BlockId id) {
      for (const auto& bid : blockIds) {
        if (bid == id) return true;
      }
      return false;
    };

    for (const auto& blk : func.blocks) {
      for (const auto& inst : blk.insts) {
        if (inst.opcode == MIROpcode::Branch) {
          if (inst.operands.size() >= 1 && inst.operands[0].kind == MIROperandKind::BlockLabel) {
            if (!blockExists(inst.operands[0].blockLabel())) {
              result.addError("Function '" + func.name + "': branch to nonexistent block");
            }
          }
        }
        if (inst.opcode == MIROpcode::BranchIfNonZero) {
          if (inst.operands.size() >= 2 && inst.operands[1].kind == MIROperandKind::BlockLabel) {
            if (!blockExists(inst.operands[1].blockLabel())) {
              result.addError("Function '" + func.name + "': bnez to nonexistent block");
            }
          }
        }
      }
    }

    // Check that every block ends with a terminator.
    for (size_t i = 0; i < func.blocks.size(); ++i) {
      const auto& blk = func.blocks[i];
      if (blk.insts.empty()) {
        result.addError("Function '" + func.name + "': block has no instructions");
        continue;
      }
      const auto& last = blk.insts.back();
      bool isTerminator = (last.opcode == MIROpcode::Return ||
                           last.opcode == MIROpcode::Branch ||
                           last.opcode == MIROpcode::BranchIfNonZero ||
                           last.opcode == MIROpcode::Call);
      // In P5, Call is not a terminator in MIR; only Return/Branch/BranchIfNonZero.
      isTerminator = (last.opcode == MIROpcode::Return ||
                      last.opcode == MIROpcode::Branch ||
                      last.opcode == MIROpcode::BranchIfNonZero);
      if (!isTerminator && i < func.blocks.size() - 1) {
        // Non-last block must end with terminator.
        result.addError("Function '" + func.name + "': non-final block '" +
                        blk.label + "' does not end with a terminator");
      }
    }
  }

  return result;
}

// ── MIR printer ─────────────────────────────────────────────────────────────

static void printOperand(const MIROperand& op, std::ostream& out) {
  switch (op.kind) {
    case MIROperandKind::VReg:
      out << "%v" << op.data0;
      break;
    case MIROperandKind::PhysReg:
      out << physRegName(op.physReg());
      break;
    case MIROperandKind::Immediate:
      out << op.data0;
      break;
    case MIROperandKind::FrameSlot:
      out << "[frame#" << op.data0 << "]";
      break;
    case MIROperandKind::GlobalSymbol:
      out << "@global." << op.data0;
      break;
    case MIROperandKind::BlockLabel:
      out << "block." << op.data0;
      break;
  }
}

void dumpMIR(const MIRModule& module, std::ostream& out) {
  for (const auto& func : module.functions) {
    out << "func " << func.name << " : " << (func.returnType.isI32() ? "i32" : "void") << "\n";
    out << "  source_func_id=" << func.sourceFuncId.value << "\n";
    out << "  has_call=" << (func.hasCall ? "true" : "false") << "\n";
    out << "  max_outgoing_args=" << func.maxOutgoingArgCount << "\n";
    out << "  vreg_count=" << func.vregCount() << "\n";

    // Frame objects
    out << "  frame_objects:\n";
    for (size_t i = 0; i < func.frameObjects.size(); ++i) {
      const auto& fo = func.frameObjects[i];
      out << "    [" << i << "] ";
      switch (fo.kind) {
        case FrameObjectKind::IncomingParameter:   out << "incoming_param"; break;
        case FrameObjectKind::IRSlot:              out << "ir_slot"; break;
        case FrameObjectKind::VRegHome:            out << "vreg_home"; break;
        case FrameObjectKind::OutgoingArgument:    out << "outgoing_arg"; break;
        case FrameObjectKind::SavedReturnAddress:  out << "saved_ra"; break;
      }
      out << " size=" << fo.size << " offset=" << fo.offset;
      if (fo.irSlot.has_value()) out << " slot=" << fo.irSlot->value;
      if (fo.vregId.has_value()) out << " vreg=" << fo.vregId->value;
      if (fo.paramIndex >= 0) out << " param=" << fo.paramIndex;
      out << "\n";
    }

    // Blocks
    for (const auto& blk : func.blocks) {
      out << "  block." << blk.id.value;
      if (!blk.label.empty()) out << " (" << blk.label << ")";
      out << ":\n";
      for (const auto& inst : blk.insts) {
        out << "    ";
        if (inst.opcode == MIROpcode::Comment) {
          out << "# " << inst.comment;
        } else {
          out << mirOpcodeName(inst.opcode);
          for (size_t i = 0; i < inst.operands.size(); ++i) {
            if (i == 0) out << " ";
            else out << ", ";
            printOperand(inst.operands[i], out);
          }
        }
        out << "\n";
      }
    }
    out << "\n";
  }
}

} // namespace toyc
