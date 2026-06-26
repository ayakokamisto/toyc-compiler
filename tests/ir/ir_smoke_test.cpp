/// IR smoke tests — P0 verification.

#include "toyc/support/ids.h"
#include "toyc/ir/ir_type.h"
#include "toyc/ir/value.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/module.h"
#include "toyc/ir/instruction.h"

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
  auto gid = mod.createGlobal("x", I32Type, false, 0);
  EXPECT_TRUE(gid.valid());
  EXPECT_EQ(mod.globals().size(), 1u);
  EXPECT_EQ(mod.globals()[0].name, "x");
  EXPECT_FALSE(mod.globals()[0].isConst);
}

TEST(ModuleTest, CreateConstGlobal) {
  Module mod;
  mod.createGlobal("N", I32Type, true, 42);
  EXPECT_EQ(mod.globals().size(), 1u);
  EXPECT_TRUE(mod.globals()[0].isConst);
  EXPECT_EQ(mod.globals()[0].initValue, 42);
}

// ── BasicBlock tests ───────────────────────────────────────────────────────

TEST(BasicBlockTest, Creation) {
  BasicBlock bb(BlockId(0));
  EXPECT_EQ(bb.id().value, 0u);
  EXPECT_TRUE(bb.instructions().empty());
  EXPECT_EQ(bb.terminator(), nullptr);
}

TEST(BasicBlockTest, AppendInstruction) {
  BasicBlock bb(BlockId(0));
  auto* ret = new RetInst();
  bb.appendInst(ret);
  EXPECT_EQ(bb.instructions().size(), 1u);
  EXPECT_EQ(bb.terminator(), ret);
}

TEST(BasicBlockTest, CFGEdges) {
  BasicBlock bb(BlockId(0));
  bb.addSuccessor(BlockId(1));
  bb.addPredecessor(BlockId(2));
  EXPECT_EQ(bb.successors().size(), 1u);
  EXPECT_EQ(bb.predecessors().size(), 1u);
  EXPECT_EQ(bb.successors()[0].value, 1u);
}

// ── Function tests ─────────────────────────────────────────────────────────

TEST(FunctionTest, CreateBlock) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  auto* bb = func->createBlock();
  EXPECT_NE(bb, nullptr);
  EXPECT_EQ(func->blocks().size(), 1u);
  EXPECT_EQ(func->entryBlock(), bb);
}

TEST(FunctionTest, Parameters) {
  Module mod;
  auto* func = mod.createFunction("add", I32Type);
  func->addParam(SymbolId(0));
  func->addParam(SymbolId(1));
  EXPECT_EQ(func->params().size(), 2u);
}

// ── Instruction tests ──────────────────────────────────────────────────────

TEST(InstructionTest, OpcodeValues) {
  EXPECT_NE(Opcode::SlotLoad, Opcode::SlotStore);
  EXPECT_NE(Opcode::Br, Opcode::CondBr);
  EXPECT_NE(Opcode::Binary, Opcode::Unary);
  EXPECT_NE(Opcode::Phi, Opcode::Ret);
}

TEST(PhiTest, IncomingValues) {
  PhiInst phi;
  phi.addIncoming(nullptr, BlockId(0));
  phi.addIncoming(nullptr, BlockId(1));
  EXPECT_EQ(phi.incoming().size(), 2u);
  EXPECT_EQ(phi.opcode(), Opcode::Phi);
}

} // namespace toyc
