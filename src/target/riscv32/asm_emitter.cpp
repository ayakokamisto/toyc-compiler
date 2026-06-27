#include "toyc/target/riscv32/asm_emitter.h"
#include "toyc/mir/mir.h"
#include "toyc/target/riscv32/registers.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>

namespace toyc::riscv32 {

std::string_view regName(GPRegister reg) {
  switch (reg) {
    case ZERO: return "zero";
    case RA:   return "ra";
    case SP:   return "sp";
    case GP:   return "gp";
    case TP:   return "tp";
    case T0:   return "t0";
    case T1:   return "t1";
    case T2:   return "t2";
    case S0:   return "s0";
    case S1:   return "s1";
    case A0:   return "a0";
    case A1:   return "a1";
    case A2:   return "a2";
    case A3:   return "a3";
    case A4:   return "a4";
    case A5:   return "a5";
    case A6:   return "a6";
    case A7:   return "a7";
    case S2:   return "s2";
    case S3:   return "s3";
    case S4:   return "s4";
    case S5:   return "s5";
    case S6:   return "s6";
    case S7:   return "s7";
    case S8:   return "s8";
    case S9:   return "s9";
    case S10:  return "s10";
    case S11:  return "s11";
    case T3:   return "t3";
    case T4:   return "t4";
    case T5:   return "t5";
    case T6:   return "t6";
  }
  return "???";
}

namespace {

std::string globalLabel(GlobalId id) {
  return ".Ltoyc.global." + std::to_string(id.value);
}

std::string functionLabel(const MIRFunction& func) {
  if (func.name == "main") return "main";
  if (func.name.rfind(".Ltoyc.", 0) == 0) return func.name;
  return ".Ltoyc.fn." + std::to_string(func.sourceFuncId.value);
}

std::string blockLabel(const MIRFunction& func, BlockId id) {
  for (const auto& block : func.blocks) {
    if (block.id == id) return block.label;
  }
  return ".Ltoyc.bb." + std::to_string(id.value);
}

bool fitsI12(int32_t value) {
  return value >= -2048 && value <= 2047;
}

class Emitter {
public:
  explicit Emitter(const AllocatedMachineModule& module) : module_(module) {}

  std::string emit() {
    emitData();
    out_ << "\n.section .text\n";
    for (const auto& func : module_.functions) {
      emitFunction(func);
    }
    emitHelpers();
    return out_.str();
  }

private:
  const AllocatedMachineModule& module_;
  std::ostringstream out_;
  const MIRFunction* func_ = nullptr;
  FrameLayout layout_;
  std::unordered_map<uint32_t, int32_t> vregOffsets_;

  std::string labelForGlobal(GlobalId id) const {
    for (const auto& global : module_.globals) {
      if (global.id == id && global.name.rfind(".Ltoyc.", 0) == 0) return global.name;
    }
    return globalLabel(id);
  }

  void emitData() {
    out_ << ".section .data\n.align 2\n";
    for (const auto& global : module_.globals) {
      out_ << labelForGlobal(global.id) << ":\n";
      out_ << "  .word " << global.staticInitialValue << "\n";
    }
  }

  void emitFunction(const AllocatedMachineFunction& allocatedFunction) {
    const auto& func = allocatedFunction.function;
    func_ = &func;
    layout_ = allocatedFunction.frameLayout;
    vregOffsets_.clear();
    for (const auto& object : func.frameObjects) {
      if (object.kind == FrameObjectKind::VRegHome && object.vregId.has_value()) {
        vregOffsets_[object.vregId->value] = object.offset;
      }
    }

    out_ << "\n";
    if (func.name == "main") {
      out_ << ".globl main\n";
    }
    out_ << functionLabel(func) << ":\n";
    adjustSp(-layout_.totalSize);
    if (func.hasCall) {
      const auto* ra = savedRaObject(func);
      if (ra != nullptr) storeReg("ra", ra->offset);
    }
    spillIncomingParameters(func);

    for (const auto& block : func.blocks) {
      out_ << block.label << ":\n";
      for (const auto& inst : block.insts) {
        emitInstruction(inst);
      }
    }
  }

  const FrameObject* frameObject(int index) const {
    if (index < 0 || index >= static_cast<int>(func_->frameObjects.size())) return nullptr;
    return &func_->frameObjects[static_cast<size_t>(index)];
  }

  const FrameObject* savedRaObject(const MIRFunction& func) const {
    for (const auto& object : func.frameObjects) {
      if (object.kind == FrameObjectKind::SavedReturnAddress) return &object;
    }
    return nullptr;
  }

  int32_t vregOffset(VRegId id) const {
    auto it = vregOffsets_.find(id.value);
    return it == vregOffsets_.end() ? 0 : it->second;
  }

  void adjustSp(int32_t amount) {
    if (amount == 0) return;
    if (fitsI12(amount)) {
      out_ << "  addi sp, sp, " << amount << "\n";
      return;
    }
    out_ << "  li t3, " << amount << "\n";
    out_ << "  add sp, sp, t3\n";
  }

  void loadReg(const std::string& reg, int32_t offset) {
    if (fitsI12(offset)) {
      out_ << "  lw " << reg << ", " << offset << "(sp)\n";
      return;
    }
    out_ << "  li t3, " << offset << "\n";
    out_ << "  add t3, sp, t3\n";
    out_ << "  lw " << reg << ", 0(t3)\n";
  }

  void storeReg(const std::string& reg, int32_t offset) {
    if (fitsI12(offset)) {
      out_ << "  sw " << reg << ", " << offset << "(sp)\n";
      return;
    }
    out_ << "  li t3, " << offset << "\n";
    out_ << "  add t3, sp, t3\n";
    out_ << "  sw " << reg << ", 0(t3)\n";
  }

  std::string loadOperand(const MIROperand& operand, const std::string& scratch) {
    switch (operand.kind) {
      case MIROperandKind::VReg:
        loadReg(scratch, vregOffset(operand.vregId()));
        return scratch;
      case MIROperandKind::PhysReg:
        return std::string(physRegName(operand.physReg()));
      case MIROperandKind::Immediate:
        out_ << "  li " << scratch << ", " << operand.imm() << "\n";
        return scratch;
      default:
        return scratch;
    }
  }

  void storeDestination(const MIROperand& dst, const std::string& reg) {
    if (dst.kind == MIROperandKind::VReg) {
      storeReg(reg, vregOffset(dst.vregId()));
      return;
    }
    if (dst.kind == MIROperandKind::PhysReg) {
      const auto name = std::string(physRegName(dst.physReg()));
      if (name != reg) out_ << "  mv " << name << ", " << reg << "\n";
    }
  }

  void spillIncomingParameters(const MIRFunction& func) {
    static constexpr const char* argRegs[] = {
      "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
    };
    for (size_t i = 0; i < func.parameterVRegs.size(); ++i) {
      int32_t dst = vregOffset(func.parameterVRegs[i]);
      if (i < 8) {
        storeReg(argRegs[i], dst);
      } else {
        loadReg("t0", layout_.incomingArgOffset(static_cast<int>(i)));
        storeReg("t0", dst);
      }
    }
  }

  void emitInstruction(const MIRInstruction& inst) {
    switch (inst.opcode) {
      case MIROpcode::Comment:
        out_ << "  # " << inst.comment << "\n";
        break;
      case MIROpcode::LoadImm:
      case MIROpcode::Li:
        out_ << "  li t2, " << inst.operands[1].imm() << "\n";
        storeDestination(inst.operands[0], "t2");
        break;
      case MIROpcode::Move: {
        std::string src = loadOperand(inst.operands[1], "t0");
        storeDestination(inst.operands[0], src);
        break;
      }
      case MIROpcode::LoadFrame: {
        const auto* object = frameObject(inst.operands[1].frameSlotIndex());
        loadReg("t2", object ? object->offset : 0);
        storeDestination(inst.operands[0], "t2");
        break;
      }
      case MIROpcode::StoreFrame: {
        const auto* object = frameObject(inst.operands[0].frameSlotIndex());
        std::string src = loadOperand(inst.operands[1], "t0");
        storeReg(src, object ? object->offset : 0);
        break;
      }
      case MIROpcode::LoadGlobal:
        out_ << "  la t3, " << labelForGlobal(inst.operands[1].globalId()) << "\n";
        out_ << "  lw t2, 0(t3)\n";
        storeDestination(inst.operands[0], "t2");
        break;
      case MIROpcode::StoreGlobal: {
        std::string src = loadOperand(inst.operands[1], "t0");
        out_ << "  la t3, " << labelForGlobal(inst.operands[0].globalId()) << "\n";
        out_ << "  sw " << src << ", 0(t3)\n";
        break;
      }
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
        emitBinary(inst);
        break;
      case MIROpcode::Addi:
      case MIROpcode::Xori:
      case MIROpcode::Sltiu:
        emitImmediate(inst);
        break;
      case MIROpcode::Call:
        out_ << "  call " << inst.comment << "\n";
        break;
      case MIROpcode::Return:
        emitReturn();
        break;
      case MIROpcode::Branch:
        out_ << "  j " << blockLabel(*func_, inst.operands[0].blockLabel()) << "\n";
        break;
      case MIROpcode::BranchIfNonZero:
        loadOperand(inst.operands[0], "t0");
        out_ << "  bnez t0, " << blockLabel(*func_, inst.operands[1].blockLabel()) << "\n";
        break;
      case MIROpcode::La:
        out_ << "  la t2, " << labelForGlobal(inst.operands[1].globalId()) << "\n";
        storeDestination(inst.operands[0], "t2");
        break;
    }
  }

  void emitBinary(const MIRInstruction& inst) {
    std::string lhs = loadOperand(inst.operands[1], "t0");
    std::string rhs = loadOperand(inst.operands[2], "t1");
    out_ << "  " << mirOpcodeName(inst.opcode) << " t2, " << lhs << ", " << rhs << "\n";
    storeDestination(inst.operands[0], "t2");
  }

  void emitImmediate(const MIRInstruction& inst) {
    std::string lhs = loadOperand(inst.operands[1], "t0");
    int32_t imm = inst.operands[2].imm();
    if (fitsI12(imm)) {
      out_ << "  " << mirOpcodeName(inst.opcode) << " t2, " << lhs << ", " << imm << "\n";
    } else {
      out_ << "  li t1, " << imm << "\n";
      const char* op = inst.opcode == MIROpcode::Addi ? "add" :
                       inst.opcode == MIROpcode::Xori ? "xor" : "sltu";
      out_ << "  " << op << " t2, " << lhs << ", t1\n";
    }
    storeDestination(inst.operands[0], "t2");
  }

  void emitReturn() {
    if (func_->hasCall) {
      const auto* ra = savedRaObject(*func_);
      if (ra != nullptr) loadReg("ra", ra->offset);
    }
    adjustSp(layout_.totalSize);
    out_ << "  ret\n";
  }

  bool textUses(const std::string& needle) const {
    for (const auto& allocatedFunction : module_.functions) {
      for (const auto& block : allocatedFunction.function.blocks) {
        for (const auto& inst : block.insts) {
          if (inst.opcode == MIROpcode::Call && inst.comment == needle) return true;
        }
      }
    }
    return false;
  }

  void emitHelpers() {
    if (textUses(".Ltoyc.mul_i32")) emitMulHelper();
    if (textUses(".Ltoyc.div_i32")) emitDivHelper();
    if (textUses(".Ltoyc.rem_i32")) emitRemHelper();
  }

  void emitMulHelper() {
    out_ << R"ASM(
.Ltoyc.mul_i32:
  mv t0, a0
  mv t1, a1
  li t2, 0
.Ltoyc.mul_i32.loop:
  andi t3, t1, 1
  beqz t3, .Ltoyc.mul_i32.skip
  add t2, t2, t0
.Ltoyc.mul_i32.skip:
  slli t0, t0, 1
  srli t1, t1, 1
  bnez t1, .Ltoyc.mul_i32.loop
  mv a0, t2
  ret
)ASM";
  }

  void emitUnsignedDivCore(const std::string& suffix) {
    out_ << ".Ltoyc.udiv_core_" << suffix << ":\n"
         << "  li t2, 0\n"
         << "  li t3, 0\n"
         << "  li t4, 32\n"
         << ".Ltoyc.udiv_core_" << suffix << ".loop:\n"
         << "  slli t3, t3, 1\n"
         << "  srli t5, t0, 31\n"
         << "  or t3, t3, t5\n"
         << "  slli t0, t0, 1\n"
         << "  slli t2, t2, 1\n"
         << "  bltu t3, t1, .Ltoyc.udiv_core_" << suffix << ".skip\n"
         << "  sub t3, t3, t1\n"
         << "  ori t2, t2, 1\n"
         << ".Ltoyc.udiv_core_" << suffix << ".skip:\n"
         << "  addi t4, t4, -1\n"
         << "  bnez t4, .Ltoyc.udiv_core_" << suffix << ".loop\n";
  }

  void emitDivHelper() {
    out_ << R"ASM(
.Ltoyc.div_i32:
  beqz a1, .Ltoyc.div_i32.zero
  xor t6, a0, a1
  mv t0, a0
  bgez t0, .Ltoyc.div_i32.lhs_abs
  sub t0, zero, t0
.Ltoyc.div_i32.lhs_abs:
  mv t1, a1
  bgez t1, .Ltoyc.div_i32.rhs_abs
  sub t1, zero, t1
.Ltoyc.div_i32.rhs_abs:
)ASM";
    emitUnsignedDivCore("div");
    out_ << R"ASM(
  bgez t6, .Ltoyc.div_i32.done
  sub t2, zero, t2
.Ltoyc.div_i32.done:
  mv a0, t2
  ret
.Ltoyc.div_i32.zero:
  li a0, 0
  ret
)ASM";
  }

  void emitRemHelper() {
    out_ << R"ASM(
.Ltoyc.rem_i32:
  beqz a1, .Ltoyc.rem_i32.zero
  mv t6, a0
  mv t0, a0
  bgez t0, .Ltoyc.rem_i32.lhs_abs
  sub t0, zero, t0
.Ltoyc.rem_i32.lhs_abs:
  mv t1, a1
  bgez t1, .Ltoyc.rem_i32.rhs_abs
  sub t1, zero, t1
.Ltoyc.rem_i32.rhs_abs:
)ASM";
    emitUnsignedDivCore("rem");
    out_ << R"ASM(
  bgez t6, .Ltoyc.rem_i32.done
  sub t3, zero, t3
.Ltoyc.rem_i32.done:
  mv a0, t3
  ret
.Ltoyc.rem_i32.zero:
  li a0, 0
  ret
)ASM";
  }
};

} // namespace

std::string emitAssembly(const AllocatedMachineModule& module) {
  return Emitter(module).emit();
}

} // namespace toyc::riscv32
