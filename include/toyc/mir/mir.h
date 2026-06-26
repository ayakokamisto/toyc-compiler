#pragma once
/// Machine IR (MIR) — lowered IR for register allocation and code emission.
///
/// MIR represents:
///   - virtual registers
///   - physical registers
///   - immediates
///   - stack slots
///   - global symbols
///   - block labels
///
/// This is a P0 placeholder — MIR will be implemented in P5+.

#include "toyc/support/ids.h"

#include <cstdint>
#include <string>
#include <vector>

namespace toyc {

/// Operand kinds in MIR.
enum class MIROperandKind : uint8_t {
  VReg,           ///< Virtual register (pre-allocation).
  PhysReg,        ///< Physical register (post-allocation).
  Imm,            ///< Immediate integer value.
  StackSlot,      ///< Stack frame slot.
  GlobalSymbol,   ///< Global variable / constant reference.
  BlockLabel,     ///< Basic block label (branch target).
};

/// A MIR operand (tagged union, stored as separate fields to avoid union issues).
struct MIROperand {
  MIROperandKind kind = MIROperandKind::Imm;
  int32_t data0 = 0;     ///< First data word (vreg value, phys reg num, imm, stack offset, etc.)
  int32_t data1 = 0;     ///< Second data word (unused for most, block value for GlobalSymbol/BlockLabel)

  static MIROperand makeVreg(VRegId id);
  static MIROperand makePhysReg(uint32_t reg);
  static MIROperand makeImm(int32_t val);
  static MIROperand makeStackSlot(int32_t offset);
  static MIROperand makeGlobal(GlobalId id);
  static MIROperand makeBlockLabel(BlockId id);
};

/// A MIR instruction (placeholder).
struct MIRInst {
  uint32_t opcode;  ///< Target-specific opcode.
  std::vector<MIROperand> operands;
};

/// A MIR basic block (placeholder).
struct MIRBlock {
  BlockId id;
  std::vector<MIRInst> insts;
};

/// A MIR function (placeholder).
struct MIRFunction {
  FunctionId id;
  std::string name;
  std::vector<MIRBlock> blocks;
  int32_t frameSize = 0;  ///< Stack frame size (set after allocation).
};

} // namespace toyc
