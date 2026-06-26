/// CFG tests — predecessor/successor edge construction.

#include "toyc/analysis/cfg.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/function.h"
#include "toyc/ir/module.h"

#include <gtest/gtest.h>

namespace toyc {

TEST(CFGTest, BranchSingleSuccessor) {
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

TEST(CFGTest, CondBranchDualSuccessor) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  auto t = builder.createBlock("true");
  auto f = builder.createBlock("false");

  builder.setInsertBlock(entry);
  auto cond = builder.emitConstInt(1);
  builder.emitCondBranch(cond, t, f);
  builder.setInsertBlock(t);
  builder.emitReturn(std::nullopt);
  builder.setInsertBlock(f);
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);

  EXPECT_EQ(func->blocks()[0]->successors().size(), 2u);
}

TEST(CFGTest, CondBranchSameTargetDedup) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  auto target = builder.createBlock("target");

  builder.setInsertBlock(entry);
  auto cond = builder.emitConstInt(1);
  builder.emitCondBranch(cond, target, target);
  builder.setInsertBlock(target);
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);

  // Same target should be deduplicated.
  EXPECT_EQ(func->blocks()[0]->successors().size(), 1u);
  EXPECT_EQ(func->blocks()[1]->predecessors().size(), 1u);
}

TEST(CFGTest, PredecessorSuccessorBidirectional) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  auto a = builder.createBlock("a");
  auto b = builder.createBlock("b");
  auto merge = builder.createBlock("merge");

  builder.setInsertBlock(entry);
  auto cond = builder.emitConstInt(1);
  builder.emitCondBranch(cond, a, b);

  builder.setInsertBlock(a);
  builder.emitBranch(merge);

  builder.setInsertBlock(b);
  builder.emitBranch(merge);

  builder.setInsertBlock(merge);
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);

  // merge has two predecessors.
  EXPECT_EQ(func->blocks()[3]->predecessors().size(), 2u);
  // a and b each have one predecessor (entry).
  EXPECT_EQ(func->blocks()[1]->predecessors().size(), 1u);
  EXPECT_EQ(func->blocks()[2]->predecessors().size(), 1u);
}

TEST(CFGTest, ReturnNoSuccessor) {
  Module mod;
  auto* func = mod.createFunction("test", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  auto val = builder.emitConstInt(0);
  builder.emitReturn(val);

  rebuildCFG(*func);

  EXPECT_TRUE(func->blocks()[0]->successors().empty());
}

TEST(CFGTest, IfElseCFG) {
  Module mod;
  auto* func = mod.createFunction("test", I32Type);
  IRBuilder builder;
  builder.setFunction(func);

  auto entry = builder.createBlock("entry");
  auto thenB = builder.createBlock("if.then");
  auto elseB = builder.createBlock("if.else");
  auto merge = builder.createBlock("if.merge");

  builder.setInsertBlock(entry);
  auto cond = builder.emitConstInt(1);
  builder.emitCondBranch(cond, thenB, elseB);

  builder.setInsertBlock(thenB);
  auto v1 = builder.emitConstInt(1);
  builder.emitBranch(merge);

  builder.setInsertBlock(elseB);
  auto v2 = builder.emitConstInt(2);
  builder.emitBranch(merge);

  builder.setInsertBlock(merge);
  builder.emitReturn(v1);

  rebuildCFG(*func);

  EXPECT_EQ(func->blocks()[0]->successors().size(), 2u);
  EXPECT_EQ(func->blocks()[1]->successors().size(), 1u);
  EXPECT_EQ(func->blocks()[2]->successors().size(), 1u);
  EXPECT_EQ(func->blocks()[3]->predecessors().size(), 2u);
}

TEST(CFGTest, WhileCFG) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);

  auto entry = builder.createBlock("entry");
  auto header = builder.createBlock("while.header");
  auto body = builder.createBlock("while.body");
  auto exit = builder.createBlock("while.exit");

  builder.setInsertBlock(entry);
  builder.emitBranch(header);

  builder.setInsertBlock(header);
  auto cond = builder.emitConstInt(1);
  builder.emitCondBranch(cond, body, exit);

  builder.setInsertBlock(body);
  builder.emitBranch(header);

  builder.setInsertBlock(exit);
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);

  // header has 2 predecessors: entry and body.
  EXPECT_EQ(func->blocks()[1]->predecessors().size(), 2u);
  // header has 2 successors: body and exit.
  EXPECT_EQ(func->blocks()[1]->successors().size(), 2u);
  // body loops back to header.
  EXPECT_EQ(func->blocks()[2]->successors().size(), 1u);
  EXPECT_EQ(func->blocks()[2]->successors()[0], header);
}

TEST(CFGTest, NestedWhileCFG) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);

  auto entry = builder.createBlock("entry");
  auto outerHeader = builder.createBlock("outer.header");
  auto outerBody = builder.createBlock("outer.body");
  auto outerExit = builder.createBlock("outer.exit");
  auto innerHeader = builder.createBlock("inner.header");
  auto innerBody = builder.createBlock("inner.body");
  auto innerExit = builder.createBlock("inner.exit");

  builder.setInsertBlock(entry);
  builder.emitBranch(outerHeader);

  builder.setInsertBlock(outerHeader);
  auto c1 = builder.emitConstInt(1);
  builder.emitCondBranch(c1, outerBody, outerExit);

  builder.setInsertBlock(outerBody);
  builder.emitBranch(innerHeader);

  builder.setInsertBlock(innerHeader);
  auto c2 = builder.emitConstInt(1);
  builder.emitCondBranch(c2, innerBody, innerExit);

  builder.setInsertBlock(innerBody);
  builder.emitBranch(innerHeader);

  builder.setInsertBlock(innerExit);
  builder.emitBranch(outerHeader);

  builder.setInsertBlock(outerExit);
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);

  // innerHeader has 2 predecessors: outerBody and innerBody.
  EXPECT_EQ(innerHeader, func->blocks()[4]->id());
  EXPECT_EQ(func->blocks()[4]->predecessors().size(), 2u);
}

} // namespace toyc
