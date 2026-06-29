/// MIR Copy Propagation — eliminate redundant Move instructions.

#include "toyc/mir/mir_copy_prop.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace toyc {

namespace {

// Follow the copy chain to find the ultimate source.
uint32_t resolveCopy(uint32_t vreg,
                     const std::unordered_map<uint32_t, uint32_t>& copyOf) {
  uint32_t current = vreg;
  std::unordered_set<uint32_t> seen;
  while (true) {
    auto it = copyOf.find(current);
    if (it == copyOf.end()) break;
    if (!seen.insert(current).second) break;  // cycle detected
    current = it->second;
  }
  return current;
}

// Collect VReg uses from an instruction (same logic as mir_dead_vreg.cpp).
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

// Rewrite VReg uses in an instruction according to the copy map.
// Returns true if any operand was changed.
bool rewriteUses(MIRInstruction& inst,
                 const std::unordered_map<uint32_t, uint32_t>& copyOf) {
  bool changed = false;
  auto rewrite = [&](int idx) {
    if (idx < static_cast<int>(inst.operands.size()) &&
        inst.operands[idx].kind == MIROperandKind::VReg) {
      uint32_t resolved = resolveCopy(inst.operands[idx].vregId().value, copyOf);
      if (resolved != inst.operands[idx].vregId().value) {
        inst.operands[idx] = MIROperand::makeVReg(VRegId(resolved));
        changed = true;
      }
    }
  };
  switch (inst.opcode) {
    case MIROpcode::Move:
    case MIROpcode::StoreFrame:
    case MIROpcode::StoreGlobal:
      rewrite(1);
      break;
    case MIROpcode::Add: case MIROpcode::Sub: case MIROpcode::Xor:
    case MIROpcode::Or: case MIROpcode::And: case MIROpcode::Sll:
    case MIROpcode::Srl: case MIROpcode::Sra: case MIROpcode::Slt:
    case MIROpcode::Sltu:
      rewrite(1); rewrite(2);
      break;
    case MIROpcode::Addi: case MIROpcode::Xori: case MIROpcode::Sltiu:
      rewrite(1);
      break;
    case MIROpcode::BranchIfNonZero:
      rewrite(0);
      break;
    default:
      break;
  }
  return changed;
}

} // namespace

bool propagateMIRCopies(MIRFunction& function) {
  bool changed = false;

  for (auto& block : function.blocks) {
    // Map: dst → src for Move instructions in this block.
    std::unordered_map<uint32_t, uint32_t> copyOf;
    // VRegs that are defined by non-Move instructions (invalidate copies).
    std::unordered_set<uint32_t> redefined;

    // First pass: build copyOf map and track redefinitions.
    for (const auto& inst : block.insts) {
      if (inst.opcode == MIROpcode::Move &&
          inst.operands.size() >= 2 &&
          inst.operands[0].kind == MIROperandKind::VReg &&
          inst.operands[1].kind == MIROperandKind::VReg) {
        uint32_t dst = inst.operands[0].vregId().value;
        uint32_t src = resolveCopy(inst.operands[1].vregId().value, copyOf);
        // If dst resolves to itself, it's an identity copy — mark for removal.
        if (resolveCopy(dst, copyOf) == src) {
          // Identity: dst already maps to src.  Keep mapping, remove inst later.
          copyOf[dst] = src;
        } else if (redefined.contains(dst)) {
          // dst was redefined by a prior non-Move — can't copy-propagate.
          continue;
        } else {
          copyOf[dst] = src;
        }
      } else {
        // Non-Move instruction.  Mark its def (operand[0]) as redefined.
        if (!inst.operands.empty() &&
            inst.operands[0].kind == MIROperandKind::VReg) {
          uint32_t def = inst.operands[0].vregId().value;
          redefined.insert(def);
          // Also invalidate any copyOf entry where this VReg is the source.
          // (This is conservative — a redefinition invalidates downstream copies.)
          copyOf.erase(def);
        }
      }
    }

    if (copyOf.empty()) continue;

    // Second pass: rewrite VReg uses in ALL instructions.  This replaces
    // uses of copied VRegs with their ultimate sources.  Dead Move removal
    // is left to cleanupDeadVRegs (which runs after this pass).
    for (auto& inst : block.insts) {
      changed |= rewriteUses(inst, copyOf);
    }
  }

  return changed;
}

} // namespace toyc
