/// RV32 Instruction Selector — lowers Canonical Slot IR to RV32 MIR.

#include "toyc/mir/instruction_selector.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"

#include <cassert>
#include <sstream>

namespace toyc {

RV32InstructionSelector::RV32InstructionSelector(DiagnosticEngine& diagnostics)
    : diag_(diagnostics) {}

// ── Emit helpers ────────────────────────────────────────────────────────────

MIRBlock& RV32InstructionSelector::currentBlock() {
  assert(currentFunc_ && !currentFunc_->blocks.empty());
  return currentFunc_->blocks.back();
}

void RV32InstructionSelector::emit(MIROpcode op, std::vector<MIROperand> ops) {
  currentBlock().insts.push_back(MIRInstruction::make(op, std::move(ops)));
}

void RV32InstructionSelector::emitComment(const std::string& text) {
  currentBlock().insts.push_back(MIRInstruction::makeComment(text));
}

VRegId RV32InstructionSelector::newVReg() {
  return currentFunc_->allocVReg();
}

VRegId RV32InstructionSelector::getOrCreateVReg(ValueId val) {
  auto it = valueToVReg_.find(val.value);
  if (it != valueToVReg_.end()) return it->second;
  VRegId vreg = newVReg();
  valueToVReg_[val.value] = vreg;
  return vreg;
}

// ── Label helpers ───────────────────────────────────────────────────────────

std::string RV32InstructionSelector::globalSymbolName(GlobalId id) const {
  return ".Ltoyc.global." + std::to_string(id.value);
}

std::string RV32InstructionSelector::functionLabel(const Function& func) const {
  if (func.name() == "main") return "main";
  if (func.name().rfind(".Ltoyc.", 0) == 0) return func.name();
  return ".Ltoyc.fn." + std::to_string(func.id().value);
}

std::string RV32InstructionSelector::blockLabel(BlockId id) const {
  return ".Lbb." + std::to_string(id.value);
}

// ── Lower a single IR instruction ───────────────────────────────────────────

void RV32InstructionSelector::lowerInst(const Inst& inst) {
  switch (inst.opcode) {
    case Opcode::ConstInt: {
      VRegId dst = getOrCreateVReg(*inst.result);
      emit(MIROpcode::LoadImm, {MIROperand::makeVReg(dst), MIROperand::makeImm(inst.constValue)});
      break;
    }

    case Opcode::SlotLoad: {
      VRegId dst = getOrCreateVReg(*inst.result);
      auto it = slotToFrameObj_.find(inst.slot.value);
      assert(it != slotToFrameObj_.end());
      emit(MIROpcode::LoadFrame, {MIROperand::makeVReg(dst), MIROperand::makeFrameSlot(it->second)});
      break;
    }

    case Opcode::SlotStore: {
      // store.slot $slot, %value — value is in inst.lhs
      VRegId src = getOrCreateVReg(inst.lhs);
      auto it = slotToFrameObj_.find(inst.slot.value);
      assert(it != slotToFrameObj_.end());
      emit(MIROpcode::StoreFrame, {MIROperand::makeFrameSlot(it->second), MIROperand::makeVReg(src)});
      break;
    }

    case Opcode::GlobalLoad: {
      VRegId dst = getOrCreateVReg(*inst.result);
      emit(MIROpcode::LoadGlobal, {MIROperand::makeVReg(dst), MIROperand::makeGlobal(inst.global)});
      break;
    }

    case Opcode::GlobalStore: {
      // store.global @global, %value — value is in inst.lhs
      VRegId src = getOrCreateVReg(inst.lhs);
      emit(MIROpcode::StoreGlobal, {MIROperand::makeGlobal(inst.global), MIROperand::makeVReg(src)});
      break;
    }

    case Opcode::Unary: {
      VRegId dst = getOrCreateVReg(*inst.result);
      VRegId operand = getOrCreateVReg(inst.unaryOperand);
      switch (inst.unaryOp) {
        case UnaryOpcode::Negate:
          emit(MIROpcode::Sub, {MIROperand::makeVReg(dst),
                                MIROperand::makePhysReg(RV32PhysReg::Zero),
                                MIROperand::makeVReg(operand)});
          break;
        case UnaryOpcode::LogicalNot:
          emit(MIROpcode::Sltiu, {MIROperand::makeVReg(dst),
                                  MIROperand::makeVReg(operand),
                                  MIROperand::makeImm(1)});
          break;
      }
      break;
    }

    case Opcode::Binary: {
      VRegId dst = getOrCreateVReg(*inst.result);
      VRegId lhs = getOrCreateVReg(inst.lhs);
      VRegId rhs = getOrCreateVReg(inst.rhs);

      switch (inst.binaryOp) {
        case BinaryOpcode::Add:
          emit(MIROpcode::Add, {MIROperand::makeVReg(dst), MIROperand::makeVReg(lhs), MIROperand::makeVReg(rhs)});
          break;
        case BinaryOpcode::Subtract:
          emit(MIROpcode::Sub, {MIROperand::makeVReg(dst), MIROperand::makeVReg(lhs), MIROperand::makeVReg(rhs)});
          break;
        case BinaryOpcode::Multiply:
        case BinaryOpcode::Divide:
        case BinaryOpcode::Modulo: {
          // Soft arithmetic: call helper via a0/a1/a0 convention.
          emit(MIROpcode::Move, {MIROperand::makePhysReg(RV32PhysReg::A0), MIROperand::makeVReg(lhs)});
          emit(MIROpcode::Move, {MIROperand::makePhysReg(RV32PhysReg::A1), MIROperand::makeVReg(rhs)});

          // Determine helper name.
          const char* helperName = nullptr;
          if (inst.binaryOp == BinaryOpcode::Multiply) {
            helperName = ".Ltoyc.mul_i32";
            usesMul_ = true;
          } else if (inst.binaryOp == BinaryOpcode::Divide) {
            helperName = ".Ltoyc.div_i32";
            usesDiv_ = true;
          } else {
            helperName = ".Ltoyc.rem_i32";
            usesMod_ = true;
          }

          // Emit call. We encode the target as a comment on the Call instruction.
          MIRInstruction callInst = MIRInstruction::make(MIROpcode::Call, {MIROperand::makeImm(2)});
          callInst.comment = helperName;
          currentBlock().insts.push_back(std::move(callInst));

          emit(MIROpcode::Move, {MIROperand::makeVReg(dst), MIROperand::makePhysReg(RV32PhysReg::A0)});
          currentFunc_->hasCall = true;
          break;
        }
      }
      break;
    }

    case Opcode::Compare: {
      VRegId dst = getOrCreateVReg(*inst.result);
      VRegId lhs = getOrCreateVReg(inst.cmpLhs);
      VRegId rhs = getOrCreateVReg(inst.cmpRhs);

      switch (inst.cmpPred) {
        case ComparePredicate::Equal: {
          VRegId temp = newVReg();
          emit(MIROpcode::Xor, {MIROperand::makeVReg(temp), MIROperand::makeVReg(lhs), MIROperand::makeVReg(rhs)});
          emit(MIROpcode::Sltiu, {MIROperand::makeVReg(dst), MIROperand::makeVReg(temp), MIROperand::makeImm(1)});
          break;
        }
        case ComparePredicate::NotEqual: {
          VRegId temp = newVReg();
          emit(MIROpcode::Xor, {MIROperand::makeVReg(temp), MIROperand::makeVReg(lhs), MIROperand::makeVReg(rhs)});
          emit(MIROpcode::Sltu, {MIROperand::makeVReg(dst), MIROperand::makePhysReg(RV32PhysReg::Zero), MIROperand::makeVReg(temp)});
          break;
        }
        case ComparePredicate::Less:
          emit(MIROpcode::Slt, {MIROperand::makeVReg(dst), MIROperand::makeVReg(lhs), MIROperand::makeVReg(rhs)});
          break;
        case ComparePredicate::LessEqual: {
          VRegId temp = newVReg();
          emit(MIROpcode::Slt, {MIROperand::makeVReg(temp), MIROperand::makeVReg(rhs), MIROperand::makeVReg(lhs)});
          emit(MIROpcode::Xori, {MIROperand::makeVReg(dst), MIROperand::makeVReg(temp), MIROperand::makeImm(1)});
          break;
        }
        case ComparePredicate::Greater:
          emit(MIROpcode::Slt, {MIROperand::makeVReg(dst), MIROperand::makeVReg(rhs), MIROperand::makeVReg(lhs)});
          break;
        case ComparePredicate::GreaterEqual: {
          VRegId temp = newVReg();
          emit(MIROpcode::Slt, {MIROperand::makeVReg(temp), MIROperand::makeVReg(lhs), MIROperand::makeVReg(rhs)});
          emit(MIROpcode::Xori, {MIROperand::makeVReg(dst), MIROperand::makeVReg(temp), MIROperand::makeImm(1)});
          break;
        }
      }
      break;
    }

    case Opcode::Call: {
      currentFunc_->hasCall = true;

      const Function* callee = irModule_->findFunction(inst.callee);
      bool returnsInt = callee && callee->returnType().isI32();
      std::string calleeLabel = callee ? functionLabel(*callee) : ".Ltoyc.fn.unknown";

      int argCount = static_cast<int>(inst.arguments.size());
      if (argCount > currentFunc_->maxOutgoingArgCount) {
        currentFunc_->maxOutgoingArgCount = argCount;
      }

      // Load arguments into a0-a7 (first 8) or outgoing stack area (9+).
      for (int i = 0; i < argCount; ++i) {
        VRegId argVreg = getOrCreateVReg(inst.arguments[i]);
        if (i < 8) {
          RV32PhysReg argReg = static_cast<RV32PhysReg>(static_cast<int>(RV32PhysReg::A0) + i);
          emit(MIROpcode::Move, {MIROperand::makePhysReg(argReg), MIROperand::makeVReg(argVreg)});
        } else {
          // Store to outgoing argument area. Create a frame object for it.
          FrameObject outArg;
          outArg.kind = FrameObjectKind::OutgoingArgument;
          outArg.size = 4;
          outArg.paramIndex = i;
          int foIndex = currentFunc_->addFrameObject(std::move(outArg));
          emit(MIROpcode::StoreFrame, {MIROperand::makeFrameSlot(foIndex), MIROperand::makeVReg(argVreg)});
        }
      }

      // Emit call instruction with callee name in comment.
      MIRInstruction callInst = MIRInstruction::make(MIROpcode::Call, {MIROperand::makeImm(argCount)});
      callInst.comment = calleeLabel;
      currentBlock().insts.push_back(std::move(callInst));

      // Capture return value.
      if (returnsInt && inst.result.has_value()) {
        VRegId dst = getOrCreateVReg(*inst.result);
        emit(MIROpcode::Move, {MIROperand::makeVReg(dst), MIROperand::makePhysReg(RV32PhysReg::A0)});
      }
      break;
    }

    default:
      diag_.error(SourceLocation{}, "Instruction selector: unhandled IR opcode");
      hasError_ = true;
      break;
  }
}

// ── Lower a terminator ──────────────────────────────────────────────────────

void RV32InstructionSelector::lowerTerminator(const Terminator& term, const BasicBlock& /*bb*/) {
  switch (term.opcode) {
    case Opcode::Br:
      emit(MIROpcode::Branch, {MIROperand::makeBlockLabel(term.branchTarget)});
      break;

    case Opcode::CondBr: {
      VRegId cond = getOrCreateVReg(term.condCondition);
      emit(MIROpcode::BranchIfNonZero, {MIROperand::makeVReg(cond), MIROperand::makeBlockLabel(term.condTrueTarget)});
      emit(MIROpcode::Branch, {MIROperand::makeBlockLabel(term.condFalseTarget)});
      break;
    }

    case Opcode::Ret: {
      if (term.returnValue.has_value()) {
        VRegId retVal = getOrCreateVReg(*term.returnValue);
        emit(MIROpcode::Move, {MIROperand::makePhysReg(RV32PhysReg::A0), MIROperand::makeVReg(retVal)});
      }
      emit(MIROpcode::Return, {});
      break;
    }

    default:
      diag_.error(SourceLocation{}, "Instruction selector: unhandled terminator opcode");
      hasError_ = true;
      break;
  }
}

// ── Lower a block ───────────────────────────────────────────────────────────

void RV32InstructionSelector::lowerBlock(const BasicBlock& bb) {
  MIRBlock mirBlock;
  mirBlock.id = bb.id();
  mirBlock.label = blockLabel(bb.id());
  currentFunc_->blocks.push_back(std::move(mirBlock));
  blockToIndex_[bb.id().value] = static_cast<int>(currentFunc_->blocks.size() - 1);

  for (const auto& inst : bb.instructions()) {
    lowerInst(*inst);
  }

  if (bb.hasTerminator()) {
    lowerTerminator(*bb.terminator(), bb);
  }
}

// ── Lower a function ────────────────────────────────────────────────────────

void RV32InstructionSelector::lowerFunction(const Function& func) {
  MIRFunction mirFunc;
  mirFunc.sourceFuncId = func.id();
  mirFunc.name = func.name();
  mirFunc.returnType = func.returnType();

  valueToVReg_.clear();
  slotToFrameObj_.clear();
  blockToIndex_.clear();

  // Create frame objects for IR slots.
  for (const auto& slot : func.slots()) {
    FrameObject fo;
    fo.kind = FrameObjectKind::IRSlot;
    fo.size = 4;
    fo.irSlot = slot.id;
    int foIndex = static_cast<int>(mirFunc.frameObjects.size());
    mirFunc.frameObjects.push_back(std::move(fo));
    slotToFrameObj_[slot.id.value] = foIndex;
  }

  for (const auto& param : func.params()) {
    VRegId vreg = mirFunc.allocVReg();
    valueToVReg_[param.valueId.value] = vreg;
    mirFunc.parameterVRegs.push_back(vreg);
  }

  // Add saved ra placeholder (will be used if function has calls).
  FrameObject savedRa;
  savedRa.kind = FrameObjectKind::SavedReturnAddress;
  savedRa.size = 4;
  mirFunc.addFrameObject(std::move(savedRa));

  mirModule_->functions.push_back(std::move(mirFunc));
  currentFunc_ = &mirModule_->functions.back();

  for (const auto& bb : func.blocks()) {
    lowerBlock(*bb);
  }

  // Create VReg homes for all VRegs.
  int vregCount = currentFunc_->vregCount();
  for (int i = 0; i < vregCount; ++i) {
    FrameObject fo;
    fo.kind = FrameObjectKind::VRegHome;
    fo.size = 4;
    fo.vregId = VRegId(static_cast<uint32_t>(i));
    currentFunc_->addFrameObject(std::move(fo));
  }
}

// ── Main entry point ────────────────────────────────────────────────────────

std::optional<MIRModule> RV32InstructionSelector::lower(const Module& module) {
  MIRModule mirModule;
  irModule_ = &module;
  mirModule_ = &mirModule;

  usesMul_ = false;
  usesDiv_ = false;
  usesMod_ = false;

  // Copy globals.
  for (const auto& g : module.globals()) {
    mirModule.globals.push_back(g);
  }

  // Lower each function.
  for (const auto& func : module.functions()) {
    lowerFunction(*func);
    if (hasError_) return std::nullopt;
  }

  return mirModule;
}

} // namespace toyc
