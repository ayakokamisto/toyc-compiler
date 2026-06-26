/// IR scaffold — P0 stub implementations.

#include "toyc/ir/value.h"
#include "toyc/ir/use.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/module.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/printer.h"
#include "toyc/ir/verifier.h"
#include "toyc/ir/ir_type.h"

#include <sstream>

namespace toyc {

// ── IRType ──────────────────────────────────────────────────────────────────

std::string IRType::toString() const {
  switch (kind) {
    case IRTypeKind::I32:   return "i32";
    case IRTypeKind::Void:  return "void";
    case IRTypeKind::Label: return "label";
    case IRTypeKind::Error: return "<error>";
  }
  return "<unknown>";
}

// ── Value ───────────────────────────────────────────────────────────────────

void Value::addUse(Use* u) {
  u->nextUse = useList_;
  useList_ = u;
}

void Value::removeUse(Use* u) {
  // Simple O(n) removal — fine for P0.
  if (useList_ == u) {
    useList_ = u->nextUse;
    return;
  }
  for (Use* cur = useList_; cur; cur = cur->nextUse) {
    if (cur->nextUse == u) {
      cur->nextUse = u->nextUse;
      return;
    }
  }
}

std::string Value::toString() const {
  return "%" + std::to_string(id_.value);
}

// ── Instruction ─────────────────────────────────────────────────────────────

void Instruction::addOperand(Value* v) {
  operands_.emplace_back(v, this);
  v->addUse(&operands_.back());
}

// ── BasicBlock ──────────────────────────────────────────────────────────────

void BasicBlock::appendInst(Instruction* inst) {
  inst->setParentBlock(id_);
  insts_.push_back(inst);
}

void BasicBlock::addSuccessor(BlockId b) {
  succs_.push_back(b);
}

void BasicBlock::addPredecessor(BlockId b) {
  preds_.push_back(b);
}

Instruction* BasicBlock::terminator() const {
  if (insts_.empty()) return nullptr;
  return insts_.back();
}

// ── Function ────────────────────────────────────────────────────────────────

BasicBlock* Function::createBlock() {
  BlockId bid(static_cast<uint32_t>(blocks_.size()));
  auto bb = std::make_unique<BasicBlock>(bid);
  bb->setParentFunction(id_);
  auto* ptr = bb.get();
  blocks_.push_back(std::move(bb));
  return ptr;
}

BasicBlock* Function::entryBlock() const {
  return blocks_.empty() ? nullptr : blocks_.front().get();
}

// ── Module ──────────────────────────────────────────────────────────────────

Function* Module::createFunction(std::string name, IRType returnType) {
  FunctionId fid(nextFuncId_++);
  auto func = std::make_unique<Function>(fid, std::move(name), returnType);
  auto* ptr = func.get();
  funcs_.push_back(std::move(func));
  return ptr;
}

GlobalId Module::createGlobal(std::string name, IRType type, bool isConst, int64_t initValue) {
  GlobalId gid(nextGlobalId_++);
  globals_.push_back({gid, std::move(name), type, isConst, initValue});
  return gid;
}

// ── IRBuilder (P0 stubs) ───────────────────────────────────────────────────

void IRBuilder::setInsertPoint(BasicBlock* block) {
  insertBB_ = block;
}

SlotLoadInst* IRBuilder::createSlotLoad(SlotId /*slot*/) {
  return nullptr;  // P0 stub
}

SlotStoreInst* IRBuilder::createSlotStore(SlotId /*slot*/, Value* /*val*/) {
  return nullptr;  // P0 stub
}

BinaryInst* IRBuilder::createBinary(BinaryOp /*op*/, Value* /*lhs*/, Value* /*rhs*/) {
  return nullptr;  // P0 stub
}

UnaryInst* IRBuilder::createUnary(UnaryOp /*op*/, Value* /*operand*/) {
  return nullptr;  // P0 stub
}

CmpInst* IRBuilder::createCmp(CmpPred /*pred*/, Value* /*lhs*/, Value* /*rhs*/) {
  return nullptr;  // P0 stub
}

RetInst* IRBuilder::createRet(Value* /*val*/) {
  return nullptr;  // P0 stub
}

RetInst* IRBuilder::createVoidRet() {
  return nullptr;  // P0 stub
}

BrInst* IRBuilder::createBr(BlockId /*target*/) {
  return nullptr;  // P0 stub
}

CondBrInst* IRBuilder::createCondBr(Value* /*cond*/, BlockId /*trueBB*/, BlockId /*falseBB*/) {
  return nullptr;  // P0 stub
}

// ── Printer (P0 stub) ──────────────────────────────────────────────────────

void printModule(const Module& /*module*/, std::ostream& out) {
  out << "; IR Printer: P0 stub — no IR to print\n";
}

// ── Verifier (P0 stub) ─────────────────────────────────────────────────────

std::string verifyModule(const Module& /*module*/) {
  return "";  // P0: always succeeds
}

} // namespace toyc
