/// IRBuilder implementation for ToyC Canonical Slot IR.

#include "toyc/ir/builder.h"

#include <cassert>
#include <stdexcept>

namespace toyc {

void IRBuilder::setFunction(Function* func) {
  func_ = func;
  currentBlock_ = BlockId{};
}

void IRBuilder::setInsertBlock(BlockId block) {
  currentBlock_ = block;
}

BlockId IRBuilder::createBlock(std::string label) {
  assert(func_ && "No function set");
  auto* bb = func_->createBlock(std::move(label));
  return bb->id();
}

BasicBlock* IRBuilder::insertBlock() const {
  if (!func_) return nullptr;
  for (auto& bb : func_->blocks()) {
    if (bb->id() == currentBlock_) return bb.get();
  }
  return nullptr;
}

ValueId IRBuilder::allocValue() {
  assert(func_ && "No function set");
  return func_->createInstValue();
}

ValueId IRBuilder::emitConstInt(int32_t value) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  ValueId vid = allocValue();
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::ConstInt;
  inst->resultType = I32Type;
  inst->result = vid;
  inst->constValue = value;
  bb->appendInst(std::move(inst));
  return vid;
}

ValueId IRBuilder::emitLoadSlot(SlotId slot) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  ValueId vid = allocValue();
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::SlotLoad;
  inst->resultType = I32Type;
  inst->result = vid;
  inst->slot = slot;
  bb->appendInst(std::move(inst));
  return vid;
}

void IRBuilder::emitStoreSlot(SlotId slot, ValueId value) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::SlotStore;
  inst->resultType = VoidIRType;
  inst->result = std::nullopt;
  inst->slot = slot;
  inst->lhs = value;  // Reuse lhs field for store value.
  bb->appendInst(std::move(inst));
}

ValueId IRBuilder::emitLoadGlobal(GlobalId global) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  ValueId vid = allocValue();
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::GlobalLoad;
  inst->resultType = I32Type;
  inst->result = vid;
  inst->global = global;
  bb->appendInst(std::move(inst));
  return vid;
}

void IRBuilder::emitStoreGlobal(GlobalId global, ValueId value) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::GlobalStore;
  inst->resultType = VoidIRType;
  inst->result = std::nullopt;
  inst->global = global;
  inst->lhs = value;  // Reuse lhs field for store value.
  bb->appendInst(std::move(inst));
}

ValueId IRBuilder::emitUnary(UnaryOpcode op, ValueId operand) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  ValueId vid = allocValue();
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::Unary;
  inst->resultType = I32Type;
  inst->result = vid;
  inst->unaryOp = op;
  inst->unaryOperand = operand;
  bb->appendInst(std::move(inst));
  return vid;
}

ValueId IRBuilder::emitBinary(BinaryOpcode op, ValueId left, ValueId right) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  ValueId vid = allocValue();
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::Binary;
  inst->resultType = I32Type;
  inst->result = vid;
  inst->binaryOp = op;
  inst->lhs = left;
  inst->rhs = right;
  bb->appendInst(std::move(inst));
  return vid;
}

ValueId IRBuilder::emitCompare(ComparePredicate pred, ValueId left, ValueId right) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  ValueId vid = allocValue();
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::Compare;
  inst->resultType = I32Type;
  inst->result = vid;
  inst->cmpPred = pred;
  inst->cmpLhs = left;
  inst->cmpRhs = right;
  bb->appendInst(std::move(inst));
  return vid;
}

std::optional<ValueId> IRBuilder::emitCall(FunctionId callee, std::span<const ValueId> arguments, bool returnsValue) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");

  std::optional<ValueId> result;
  if (returnsValue) {
    result = allocValue();
  }

  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::Call;
  inst->resultType = returnsValue ? I32Type : VoidIRType;
  inst->result = result;
  inst->callee = callee;
  inst->arguments.assign(arguments.begin(), arguments.end());
  bb->appendInst(std::move(inst));
  return result;
}

Inst* IRBuilder::createPhi(BlockId block, IRType type) {
  assert(func_ && "No function set");
  BasicBlock* target = nullptr;
  for (auto& bb : func_->blocks()) {
    if (bb->id() == block) {
      target = bb.get();
      break;
    }
  }
  if (!target) {
    throw std::runtime_error("createPhi target block does not belong to function");
  }
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::Phi;
  inst->resultType = type;
  inst->result = allocValue();
  return target->prependPhi(std::move(inst));
}

void IRBuilder::addPhiIncoming(Inst& phi, BlockId predecessor, ValueId value) {
  if (phi.opcode != Opcode::Phi) {
    throw std::runtime_error("addPhiIncoming expects phi");
  }
  for (const auto& incoming : phi.phiIncoming) {
    if (incoming.predecessor == predecessor) {
      throw std::runtime_error("duplicate phi incoming predecessor");
    }
  }
  phi.phiIncoming.push_back(PhiIncoming{predecessor, value});
}

void IRBuilder::emitBranch(BlockId target) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  Terminator term;
  term.opcode = Opcode::Br;
  term.branchTarget = target;
  bb->setTerminator(std::move(term));
}

void IRBuilder::emitCondBranch(ValueId condition, BlockId trueTarget, BlockId falseTarget) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  Terminator term;
  term.opcode = Opcode::CondBr;
  term.condCondition = condition;
  term.condTrueTarget = trueTarget;
  term.condFalseTarget = falseTarget;
  bb->setTerminator(std::move(term));
}

void IRBuilder::emitReturn(std::optional<ValueId> value) {
  auto* bb = insertBlock();
  assert(bb && "No insert block");
  Terminator term;
  term.opcode = Opcode::Ret;
  term.returnValue = value;
  bb->setTerminator(std::move(term));
}

} // namespace toyc
