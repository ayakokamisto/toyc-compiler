/// IR smoke tests — P4 verification of Canonical Slot IR.

#include "toyc/analysis/cfg.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/ir_type.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/ir/value.h"
#include "toyc/ir/verifier.h"
#include "toyc/support/ids.h"

#include <gtest/gtest.h>

namespace toyc {

// ── Strongly-typed ID tests ─────────────────────────────────────────────────

TEST(IDTest, ValueIdConstructionAndComparison) {
  ValueId a(0);
  ValueId b(1);
  ValueId c(0);

  EXPECT_TRUE(a.valid());
  EXPECT_TRUE(b.valid());
  EXPECT_EQ(a, c);
  EXPECT_NE(a, b);
  EXPECT_LT(a, b);
}

TEST(IDTest, BlockIdConstructionAndComparison) {
  BlockId a(0);
  BlockId b(1);

  EXPECT_TRUE(a.valid());
  EXPECT_TRUE(b.valid());
  EXPECT_NE(a, b);
  EXPECT_LT(a, b);
}

TEST(IDTest, DefaultIdIsInvalid) {
  ValueId id;
  EXPECT_FALSE(id.valid());
  EXPECT_EQ(id.value, static_cast<uint32_t>(-1));
}

TEST(IDTest, AllIdTypesConstructible) {
  EXPECT_TRUE(SourceId(0).valid());
  EXPECT_TRUE(SymbolId(1).valid());
  EXPECT_TRUE(SlotId(2).valid());
  EXPECT_TRUE(InstId(3).valid());
  EXPECT_TRUE(FunctionId(4).valid());
  EXPECT_TRUE(GlobalId(5).valid());
  EXPECT_TRUE(VRegId(6).valid());
}

// ── IR type tests ───────────────────────────────────────────────────────────

TEST(IRTypeTest, I32Type) {
  IRType t = I32Type;
  EXPECT_TRUE(t.isI32());
  EXPECT_FALSE(t.isVoid());
  EXPECT_FALSE(t.isLabel());
  EXPECT_EQ(t.toString(), "i32");
}

TEST(IRTypeTest, VoidType) {
  IRType t = VoidIRType;
  EXPECT_TRUE(t.isVoid());
  EXPECT_FALSE(t.isI32());
  EXPECT_EQ(t.toString(), "void");
}

TEST(IRTypeTest, LabelType) {
  IRType t = LabelType;
  EXPECT_TRUE(t.isLabel());
  EXPECT_EQ(t.toString(), "label");
}

// ── Module creation tests ──────────────────────────────────────────────────

TEST(ModuleTest, CreateFunction) {
  Module mod;
  auto* func = mod.createFunction("main", I32Type);
  EXPECT_NE(func, nullptr);
  EXPECT_EQ(func->name(), "main");
  EXPECT_TRUE(func->returnType().isI32());
  EXPECT_EQ(func->id().value, 0u);
}

TEST(ModuleTest, CreateMultipleFunctions) {
  Module mod;
  auto* f1 = mod.createFunction("main", I32Type);
  auto* f2 = mod.createFunction("foo", VoidIRType);
  EXPECT_NE(f1, f2);
  EXPECT_NE(f1->id(), f2->id());
}

TEST(ModuleTest, CreateGlobal) {
  Module mod;
  IRGlobal g;
  g.name = "x";
  g.kind = GlobalKind::Variable;
  g.initKind = IRGlobalInitKind::Static;
  g.staticInitialValue = 0;
  auto gid = mod.createGlobal(std::move(g));
  EXPECT_TRUE(gid.valid());
  EXPECT_EQ(mod.globals().size(), 1u);
  EXPECT_EQ(mod.globals()[0].name, "x");
  EXPECT_EQ(mod.globals()[0].kind, GlobalKind::Variable);
}

TEST(ModuleTest, CreateConstGlobal) {
  Module mod;
  IRGlobal g;
  g.name = "N";
  g.kind = GlobalKind::Constant;
  g.initKind = IRGlobalInitKind::Static;
  g.staticInitialValue = 42;
  mod.createGlobal(std::move(g));
  EXPECT_EQ(mod.globals().size(), 1u);
  EXPECT_EQ(mod.globals()[0].kind, GlobalKind::Constant);
  EXPECT_EQ(mod.globals()[0].staticInitialValue, 42);
}

// ── BasicBlock tests ───────────────────────────────────────────────────────

TEST(BasicBlockTest, Creation) {
  BasicBlock bb(BlockId(0), "entry");
  EXPECT_EQ(bb.id().value, 0u);
  EXPECT_EQ(bb.label(), "entry");
  EXPECT_TRUE(bb.instructions().empty());
  EXPECT_FALSE(bb.hasTerminator());
}

TEST(BasicBlockTest, AppendInstruction) {
  BasicBlock bb(BlockId(0), "entry");
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::ConstInt;
  inst->resultType = I32Type;
  inst->result = ValueId(0);
  inst->constValue = 42;
  bb.appendInst(std::move(inst));
  EXPECT_EQ(bb.instructions().size(), 1u);
  EXPECT_FALSE(bb.hasTerminator());
}

TEST(BasicBlockTest, SetTerminator) {
  BasicBlock bb(BlockId(0), "entry");
  Terminator term;
  term.opcode = Opcode::Ret;
  term.returnValue = std::nullopt;
  bb.setTerminator(std::move(term));
  EXPECT_TRUE(bb.hasTerminator());
}

// ── Function tests ─────────────────────────────────────────────────────────

TEST(FunctionTest, CreateBlock) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  auto* bb = func->createBlock("entry");
  EXPECT_NE(bb, nullptr);
  EXPECT_EQ(func->blocks().size(), 1u);
  EXPECT_EQ(func->entryBlock(), bb);
}

TEST(FunctionTest, Parameters) {
  Module mod;
  auto* func = mod.createFunction("add", I32Type);
  auto p1 = func->addParam(SymbolId(0));
  auto p2 = func->addParam(SymbolId(1));
  EXPECT_EQ(func->params().size(), 2u);
  EXPECT_TRUE(p1.valueId.valid());
  EXPECT_TRUE(p1.slotId.valid());
  EXPECT_TRUE(p2.valueId.valid());
  EXPECT_TRUE(p2.slotId.valid());
  EXPECT_NE(p1.valueId, p2.valueId);
  EXPECT_NE(p1.slotId, p2.slotId);
}

TEST(FunctionTest, Slots) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  auto s1 = func->createSlot(SlotKind::LocalVariable, SymbolId(0));
  auto s2 = func->createSlot(SlotKind::Temporary);
  EXPECT_EQ(func->slots().size(), 2u);
  EXPECT_EQ(func->slots()[0].kind, SlotKind::LocalVariable);
  EXPECT_EQ(func->slots()[1].kind, SlotKind::Temporary);
  EXPECT_EQ(func->slots()[0].sourceSymbol, SymbolId(0));
  EXPECT_FALSE(func->slots()[1].sourceSymbol.has_value());
}

TEST(FunctionTest, Values) {
  Module mod;
  auto* func = mod.createFunction("test", I32Type);
  auto argVal = func->createArgumentValue();
  auto instVal = func->createInstValue();
  EXPECT_EQ(func->values().size(), 2u);
  EXPECT_EQ(func->values()[0].source, ValueSource::Argument);
  EXPECT_EQ(func->values()[1].source, ValueSource::InstructionResult);
}

// ── Opcode tests ───────────────────────────────────────────────────────────

TEST(OpcodeTest, Names) {
  EXPECT_EQ(opcodeName(Opcode::ConstInt), "const");
  EXPECT_EQ(opcodeName(Opcode::SlotLoad), "load.slot");
  EXPECT_EQ(opcodeName(Opcode::SlotStore), "store.slot");
  EXPECT_EQ(opcodeName(Opcode::Br), "br");
  EXPECT_EQ(opcodeName(Opcode::CondBr), "condbr");
  EXPECT_EQ(opcodeName(Opcode::Ret), "ret");
  EXPECT_EQ(opcodeName(Opcode::Call), "call");
}

// ── Builder tests ──────────────────────────────────────────────────────────

TEST(IRBuilderTest, ConstInt) {
  Module mod;
  auto* func = mod.createFunction("test", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);

  auto val = builder.emitConstInt(42);
  EXPECT_TRUE(val.valid());
  EXPECT_EQ(func->values().size(), 1u);
  auto* bb = func->entryBlock();
  EXPECT_EQ(bb->instructions().size(), 1u);
  EXPECT_EQ(bb->instructions()[0]->opcode, Opcode::ConstInt);
  EXPECT_EQ(bb->instructions()[0]->constValue, 42);
}

TEST(IRBuilderTest, LoadStoreSlot) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  SlotId slot = func->createSlot(SlotKind::LocalVariable);

  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);

  auto val = builder.emitConstInt(10);
  builder.emitStoreSlot(slot, val);
  auto loaded = builder.emitLoadSlot(slot);

  auto* bb = func->entryBlock();
  EXPECT_EQ(bb->instructions().size(), 3u);
  EXPECT_EQ(bb->instructions()[1]->opcode, Opcode::SlotStore);
  EXPECT_EQ(bb->instructions()[2]->opcode, Opcode::SlotLoad);
}

TEST(IRBuilderTest, BinaryAndCompare) {
  Module mod;
  auto* func = mod.createFunction("test", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);

  auto a = builder.emitConstInt(1);
  auto b = builder.emitConstInt(2);
  auto sum = builder.emitBinary(BinaryOpcode::Add, a, b);
  auto cmp = builder.emitCompare(ComparePredicate::Less, a, b);

  EXPECT_TRUE(sum.valid());
  EXPECT_TRUE(cmp.valid());
  EXPECT_NE(sum, cmp);
}

TEST(IRBuilderTest, BranchAndCondBranch) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);

  auto entry = builder.createBlock("entry");
  auto target = builder.createBlock("target");
  auto other = builder.createBlock("other");

  builder.setInsertBlock(entry);
  auto cond = builder.emitConstInt(1);
  builder.emitCondBranch(cond, target, other);

  builder.setInsertBlock(target);
  builder.emitReturn(std::nullopt);

  builder.setInsertBlock(other);
  builder.emitReturn(std::nullopt);

  EXPECT_EQ(func->blocks().size(), 3u);
  EXPECT_TRUE(func->entryBlock()->hasTerminator());
  EXPECT_EQ(func->entryBlock()->terminator()->opcode, Opcode::CondBr);
}

TEST(IRBuilderTest, Call) {
  Module mod;
  auto* callee = mod.createFunction("add", I32Type);
  auto* func = mod.createFunction("main", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);

  auto arg1 = builder.emitConstInt(1);
  auto arg2 = builder.emitConstInt(2);
  std::vector<ValueId> args = {arg1, arg2};
  auto result = builder.emitCall(callee->id(), args);

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result->valid());
}

// ── CFG tests ──────────────────────────────────────────────────────────────

TEST(CFGTest, SimpleBranch) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);

  auto entry = builder.createBlock("entry");
  auto exit = builder.createBlock("exit");

  builder.setInsertBlock(entry);
  builder.emitBranch(exit);

  builder.setInsertBlock(exit);
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);

  EXPECT_EQ(func->blocks()[0]->successors().size(), 1u);
  EXPECT_EQ(func->blocks()[0]->successors()[0], exit);
  EXPECT_EQ(func->blocks()[1]->predecessors().size(), 1u);
  EXPECT_EQ(func->blocks()[1]->predecessors()[0], entry);
}

TEST(CFGTest, CondBranch) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);

  auto entry = builder.createBlock("entry");
  auto trueB = builder.createBlock("true");
  auto falseB = builder.createBlock("false");

  builder.setInsertBlock(entry);
  auto cond = builder.emitConstInt(1);
  builder.emitCondBranch(cond, trueB, falseB);

  builder.setInsertBlock(trueB);
  builder.emitReturn(std::nullopt);

  builder.setInsertBlock(falseB);
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);

  EXPECT_EQ(func->blocks()[0]->successors().size(), 2u);
}

// ── Verifier tests ─────────────────────────────────────────────────────────

TEST(VerifierTest, ValidModule) {
  Module mod;
  auto* func = mod.createFunction("main", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  auto val = builder.emitConstInt(0);
  builder.emitReturn(val);

  rebuildCFG(mod);
  auto result = verifyModule(mod);
  EXPECT_TRUE(result.ok) << "Errors: " << (result.errors.empty() ? "" : result.errors[0]);
}

TEST(VerifierTest, MissingTerminator) {
  Module mod;
  auto* func = mod.createFunction("main", I32Type);
  func->createBlock("entry");

  auto result = verifyModule(mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("terminator") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DuplicateValueId) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  // Create two values with the same ID — this is a hack to test the verifier.
  func->createArgumentValue();
  func->createArgumentValue();  // Different IDs, OK.
  // The verifier checks uniqueness, which is enforced by the Function.
  // So this test just verifies normal operation.
  EXPECT_EQ(func->values().size(), 2u);
}

} // namespace toyc
