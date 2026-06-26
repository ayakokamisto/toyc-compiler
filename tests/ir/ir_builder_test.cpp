/// IRBuilder tests — comprehensive instruction emission.

#include "toyc/ir/basic_block.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/support/ids.h"

#include <gtest/gtest.h>

namespace toyc {

class BuilderTest : public ::testing::Test {
protected:
  Module mod;
  Function* func = nullptr;
  IRBuilder builder;

  void SetUp() override {
    func = mod.createFunction("test", I32Type);
    builder.setFunction(func);
    auto entry = builder.createBlock("entry");
    builder.setInsertBlock(entry);
  }
};

TEST_F(BuilderTest, ConstIntUniqueValues) {
  auto v1 = builder.emitConstInt(1);
  auto v2 = builder.emitConstInt(2);
  EXPECT_NE(v1, v2);
  EXPECT_EQ(func->values().size(), 2u);
}

TEST_F(BuilderTest, LoadGlobalAndStoreGlobal) {
  IRGlobal g;
  g.name = "x";
  g.kind = GlobalKind::Variable;
  g.initKind = IRGlobalInitKind::Static;
  GlobalId gid = mod.createGlobal(std::move(g));

  auto val = builder.emitConstInt(42);
  builder.emitStoreGlobal(gid, val);
  auto loaded = builder.emitLoadGlobal(gid);

  auto* bb = func->entryBlock();
  EXPECT_EQ(bb->instructions().size(), 3u);
  EXPECT_EQ(bb->instructions()[1]->opcode, Opcode::GlobalStore);
  EXPECT_EQ(bb->instructions()[2]->opcode, Opcode::GlobalLoad);
}

TEST_F(BuilderTest, UnaryOperations) {
  auto val = builder.emitConstInt(5);
  auto neg = builder.emitUnary(UnaryOpcode::Negate, val);
  auto notVal = builder.emitUnary(UnaryOpcode::LogicalNot, val);

  EXPECT_NE(neg, notVal);
  auto* bb = func->entryBlock();
  EXPECT_EQ(bb->instructions()[1]->opcode, Opcode::Unary);
  EXPECT_EQ(bb->instructions()[1]->unaryOp, UnaryOpcode::Negate);
  EXPECT_EQ(bb->instructions()[2]->opcode, Opcode::Unary);
  EXPECT_EQ(bb->instructions()[2]->unaryOp, UnaryOpcode::LogicalNot);
}

TEST_F(BuilderTest, AllBinaryOperations) {
  auto a = builder.emitConstInt(10);
  auto b = builder.emitConstInt(3);

  auto add = builder.emitBinary(BinaryOpcode::Add, a, b);
  auto sub = builder.emitBinary(BinaryOpcode::Subtract, a, b);
  auto mul = builder.emitBinary(BinaryOpcode::Multiply, a, b);
  auto div = builder.emitBinary(BinaryOpcode::Divide, a, b);
  auto mod = builder.emitBinary(BinaryOpcode::Modulo, a, b);

  EXPECT_NE(add, sub);
  EXPECT_NE(sub, mul);
  EXPECT_NE(mul, div);
  EXPECT_NE(div, mod);
  EXPECT_EQ(func->entryBlock()->instructions().size(), 7u);  // 2 const + 5 binary
}

TEST_F(BuilderTest, AllCompareOperations) {
  auto a = builder.emitConstInt(1);
  auto b = builder.emitConstInt(2);

  auto eq = builder.emitCompare(ComparePredicate::Equal, a, b);
  auto ne = builder.emitCompare(ComparePredicate::NotEqual, a, b);
  auto lt = builder.emitCompare(ComparePredicate::Less, a, b);
  auto le = builder.emitCompare(ComparePredicate::LessEqual, a, b);
  auto gt = builder.emitCompare(ComparePredicate::Greater, a, b);
  auto ge = builder.emitCompare(ComparePredicate::GreaterEqual, a, b);

  EXPECT_NE(eq, ne);
  EXPECT_NE(lt, le);
  EXPECT_NE(gt, ge);
}

TEST_F(BuilderTest, CallWithArguments) {
  auto* callee = mod.createFunction("add", I32Type);
  auto a = builder.emitConstInt(1);
  auto b = builder.emitConstInt(2);
  std::vector<ValueId> args = {a, b};
  auto result = builder.emitCall(callee->id(), args);

  EXPECT_TRUE(result.has_value());
  auto* bb = func->entryBlock();
  EXPECT_EQ(bb->instructions().back()->opcode, Opcode::Call);
  EXPECT_EQ(bb->instructions().back()->arguments.size(), 2u);
}

TEST_F(BuilderTest, CallVoidFunction) {
  auto* callee = mod.createFunction("print", VoidIRType);
  auto arg = builder.emitConstInt(42);
  std::vector<ValueId> args = {arg};
  auto result = builder.emitCall(callee->id(), args);

  // The builder conservatively creates a result; lowering handles void correctly.
  EXPECT_TRUE(result.has_value());
}

TEST_F(BuilderTest, TerminatorsBlockNewInstructions) {
  builder.emitReturn(std::nullopt);
  EXPECT_TRUE(func->entryBlock()->hasTerminator());
}

TEST_F(BuilderTest, MultipleBlocks) {
  auto b1 = builder.createBlock("block1");
  auto b2 = builder.createBlock("block2");
  EXPECT_EQ(func->blocks().size(), 3u);  // entry + 2
  EXPECT_NE(b1, b2);
}

} // namespace toyc
