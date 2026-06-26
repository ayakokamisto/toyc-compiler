/// MIR scaffold — P0 stub implementations.

#include "toyc/mir/mir.h"
#include "toyc/mir/liveness.h"
#include "toyc/mir/register_allocator.h"

namespace toyc {

// ── MIROperand factory methods ──────────────────────────────────────────────

MIROperand MIROperand::makeVreg(VRegId id) {
  MIROperand op;
  op.kind = MIROperandKind::VReg;
  op.data0 = static_cast<int32_t>(id.value);
  return op;
}

MIROperand MIROperand::makePhysReg(uint32_t reg) {
  MIROperand op;
  op.kind = MIROperandKind::PhysReg;
  op.data0 = static_cast<int32_t>(reg);
  return op;
}

MIROperand MIROperand::makeImm(int32_t val) {
  MIROperand op;
  op.kind = MIROperandKind::Imm;
  op.data0 = val;
  return op;
}

MIROperand MIROperand::makeStackSlot(int32_t offset) {
  MIROperand op;
  op.kind = MIROperandKind::StackSlot;
  op.data0 = offset;
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

// ── LivenessInfo (P0 stub) ─────────────────────────────────────────────────

void LivenessInfo::compute(const MIRFunction& /*func*/) {
  // P0 stub — liveness analysis not yet implemented.
}

// ── LinearScanAllocator (P0 stub) ──────────────────────────────────────────

void LinearScanAllocator::allocate(MIRFunction& /*func*/) {
  // P0 stub — register allocation not yet implemented.
}

} // namespace toyc
