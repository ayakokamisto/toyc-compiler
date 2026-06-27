#include "toyc/analysis/cfg.h"
#include "toyc/analysis/dominator_tree.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/module.h"

#include <gtest/gtest.h>

namespace toyc {

TEST(DominatorTreeTest, DiamondAndUnreachable) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  auto thenB = b.createBlock("then");
  auto elseB = b.createBlock("else");
  auto merge = b.createBlock("merge");
  auto dead = b.createBlock("dead");

  b.setInsertBlock(entry);
  auto cond = b.emitConstInt(1);
  b.emitCondBranch(cond, thenB, elseB);
  b.setInsertBlock(thenB);
  b.emitBranch(merge);
  b.setInsertBlock(elseB);
  b.emitBranch(merge);
  b.setInsertBlock(merge);
  auto zero = b.emitConstInt(0);
  b.emitReturn(zero);
  b.setInsertBlock(dead);
  b.emitReturn(std::nullopt);
  rebuildCFG(*func);

  DominatorTree dom(*func);
  EXPECT_TRUE(dom.isReachable(entry));
  EXPECT_FALSE(dom.isReachable(dead));
  EXPECT_EQ(dom.immediateDominator(entry), std::nullopt);
  EXPECT_EQ(dom.immediateDominator(thenB), entry);
  EXPECT_EQ(dom.immediateDominator(elseB), entry);
  EXPECT_EQ(dom.immediateDominator(merge), entry);
  EXPECT_TRUE(dom.dominates(entry, merge));
  EXPECT_FALSE(dom.dominates(thenB, merge));
  EXPECT_EQ(dom.reversePostOrder().front(), entry);
}

TEST(DominatorTreeTest, LoopHeaderDominatesBodyAndExit) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  auto header = b.createBlock("header");
  auto body = b.createBlock("body");
  auto exit = b.createBlock("exit");

  b.setInsertBlock(entry);
  b.emitBranch(header);
  b.setInsertBlock(header);
  auto cond = b.emitConstInt(1);
  b.emitCondBranch(cond, body, exit);
  b.setInsertBlock(body);
  b.emitBranch(header);
  b.setInsertBlock(exit);
  auto zero = b.emitConstInt(0);
  b.emitReturn(zero);
  rebuildCFG(*func);

  DominatorTree dom(*func);
  EXPECT_EQ(dom.immediateDominator(header), entry);
  EXPECT_EQ(dom.immediateDominator(body), header);
  EXPECT_EQ(dom.immediateDominator(exit), header);
  EXPECT_TRUE(dom.dominates(header, body));
  EXPECT_TRUE(dom.dominates(header, exit));
}

} // namespace toyc
