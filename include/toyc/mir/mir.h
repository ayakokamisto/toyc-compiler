#pragma once
/// Machine IR (MIR) — target-specific IR for RISC-V32.
///
/// P5 implements a real MIR data model used by:
///   - IR → MIR instruction selection
///   - SpillAllAllocator
///   - Assembly emission

#include "toyc/support/ids.h"
#include "toyc/ir/module.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace toyc {

// ── Physical registers ──────────────────────────────────────────────────────

/// RISC-V32 physical registers used in P5.
enum class RV32PhysReg : uint8_t {
  Zero,   // x0
  Ra,     // x1
  Sp,     // x2
  T0, T1, T2, T3, T4, T5, T6,
  A0, A1, A2, A3, A4, A5, A6, A7,
};

/// Get the ABI name of a physical register.
const char* physRegName(RV32PhysReg reg);

// ── MIR operand ─────────────────────────────────────────────────────────────

enum class MIROperandKind : uint8_t {
  VReg,
  PhysReg,
  Immediate,
  FrameSlot,
  GlobalSymbol,
  BlockLabel,
};

struct MIROperand {
  MIROperandKind kind = MIROperandKind::Immediate;
  int32_t data0 = 0;
  int32_t data1 = 0;

  static MIROperand makeVReg(VRegId id);
  static MIROperand makePhysReg(RV32PhysReg reg);
  static MIROperand makeImm(int32_t val);
  static MIROperand makeFrameSlot(int32_t slotIndex);
  static MIROperand makeGlobal(GlobalId id);
  static MIROperand makeBlockLabel(BlockId id);

  [[nodiscard]] VRegId vregId() const;
  [[nodiscard]] RV32PhysReg physReg() const;
  [[nodiscard]] int32_t imm() const;
  [[nodiscard]] int32_t frameSlotIndex() const;
  [[nodiscard]] GlobalId globalId() const;
  [[nodiscard]] BlockId blockLabel() const;
};

// ── MIR opcodes ─────────────────────────────────────────────────────────────

enum class MIROpcode : uint8_t {
  // Pseudo
  Comment,

  // Immediate
  LoadImm,        // dst = imm
  Li,             // dst = imm (alias for LoadImm, used in emission)

  // Move
  Move,           // dst = src (phys-to-phys)

  // Memory: frame slots
  LoadFrame,      // dst = [sp + offset]
  StoreFrame,     // [sp + offset] = src

  // Memory: globals
  LoadGlobal,     // dst = [@global]
  StoreGlobal,    // [@global] = src

  // Arithmetic (R-type)
  Add,            // dst = lhs + rhs
  Sub,            // dst = lhs - rhs
  Xor,            // dst = lhs ^ rhs
  Or,             // dst = lhs | rhs
  And,            // dst = lhs & rhs
  Sll,            // dst = lhs << rhs
  Srl,            // dst = lhs >> rhs (logical)
  Sra,            // dst = lhs >> rhs (arithmetic)
  Slt,            // dst = (lhs < rhs) ? 1 : 0  (signed)
  Sltu,           // dst = (lhs < rhs) ? 1 : 0  (unsigned)

  // Arithmetic (I-type)
  Addi,           // dst = lhs + imm
  Xori,           // dst = lhs ^ imm
  Sltiu,          // dst = (lhs < imm) ? 1 : 0 (unsigned)

  // Control flow
  Call,           // call function
  Return,         // ret
  Branch,         // j label
  BranchIfNonZero, // bnez src, label

  // Address materialization
  La,             // dst = address of global symbol
};

/// Get a human-readable name for a MIR opcode.
const char* mirOpcodeName(MIROpcode op);

// ── Frame objects ───────────────────────────────────────────────────────────

enum class FrameObjectKind : uint8_t {
  IncomingParameter,   // Incoming arg from caller (stack arg, i >= 8)
  IRSlot,              // Maps to an IR SlotId
  VRegHome,            // Spill slot for a virtual register
  OutgoingArgument,    // Outgoing arg to callee (stack arg, i >= 8)
  SavedReturnAddress,  // Saved ra
};

struct FrameObject {
  FrameObjectKind kind;
  int32_t size = 4;       // Always 4 bytes in P5
  int32_t offset = 0;     // Offset from sp (computed by FrameLayout)
  std::optional<SlotId> irSlot;   // For IRSlot kind
  std::optional<VRegId> vregId;   // For VRegHome kind
  int32_t paramIndex = -1;        // For IncomingParameter / OutgoingArgument
};

// ── MIR types ───────────────────────────────────────────────────────────────

struct MIRInstruction {
  MIROpcode opcode;
  std::vector<MIROperand> operands;
  std::string comment;  // For Comment opcode

  // Convenience constructors
  static MIRInstruction make(MIROpcode op, std::vector<MIROperand> ops = {});
  static MIRInstruction makeComment(std::string text);
};

struct MIRBlock {
  BlockId id;
  std::string label;
  std::vector<MIRInstruction> insts;
};

struct MIRFunction {
  FunctionId sourceFuncId;
  std::string name;
  IRType returnType = VoidIRType;
  std::vector<MIRBlock> blocks;
  std::vector<FrameObject> frameObjects;
  std::vector<VRegId> parameterVRegs;
  int32_t maxOutgoingArgCount = 0;
  bool hasCall = false;
  int entryBlockIndex = 0;

  // VReg management
  VRegId allocVReg();
  [[nodiscard]] int vregCount() const { return nextVReg_; }

  // Frame object management
  int addFrameObject(FrameObject obj);
  [[nodiscard]] const FrameObject* findVRegHome(VRegId vreg) const;
  [[nodiscard]] const FrameObject* findIRSlot(SlotId slot) const;

  // Block access
  [[nodiscard]] MIRBlock* entryBlock();
  [[nodiscard]] const MIRBlock* entryBlock() const;
  MIRBlock* findBlock(BlockId id);
  [[nodiscard]] const MIRBlock* findBlock(BlockId id) const;

private:
  int nextVReg_ = 0;
};

struct MIRModule {
  std::vector<MIRFunction> functions;
  std::vector<IRGlobal> globals;

  MIRFunction* findFunction(FunctionId id);
  [[nodiscard]] const MIRFunction* findFunction(FunctionId id) const;
  MIRFunction* findFunctionByName(const std::string& name);
};

// ── Frame layout (computed after allocation) ────────────────────────────────

struct FrameLayout {
  int32_t outgoingArgSize = 0;
  int32_t irSlotSize = 0;
  int32_t vregHomeSize = 0;
  int32_t savedRaSize = 0;
  int32_t totalSize = 0;  // 16-byte aligned

  /// Compute the frame layout for a MIRFunction.
  static FrameLayout compute(MIRFunction& func);

  /// Get the sp-relative offset for an outgoing argument at index i (i >= 8).
  [[nodiscard]] int32_t outgoingArgOffset(int paramIndex) const;

  /// Get the sp-relative offset for reading an incoming stack argument.
  /// Caller's sp = current sp + totalSize.
  [[nodiscard]] int32_t incomingArgOffset(int paramIndex) const;
};

// ── MIR verification ────────────────────────────────────────────────────────

struct MIRVerificationResult {
  bool ok = true;
  std::vector<std::string> errors;

  void addError(std::string msg) {
    ok = false;
    errors.push_back(std::move(msg));
  }
};

MIRVerificationResult verifyMIR(const MIRModule& module);

// ── MIR printer ─────────────────────────────────────────────────────────────

void dumpMIR(const MIRModule& module, std::ostream& output);

} // namespace toyc
