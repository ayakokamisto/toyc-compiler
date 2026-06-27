#include "toyc/analysis/cfg.h"
#include "toyc/analysis/dominance_frontier.h"
#include "toyc/analysis/dominator_tree.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/module.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace toyc {

static bool containsBlock(const std::vector<BlockId>& blocks, BlockId block) {
  return std::find(blocks.begin(), blocks.end(), block) != blocks.end();
}

TEST(DominanceFrontierTest, DiamondFrontierContainsMerge) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  auto thenB = b.createBlock("then");
  auto elseB = b.createBlock("else");
  auto merge = b.createBlock("merge");
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
  rebuildCFG(*func);

  DominatorTree dom(*func);
  DominanceFrontier df(*func, dom);
  EXPECT_TRUE(containsBlock(df.frontier(thenB), merge));
  EXPECT_TRUE(containsBlock(df.frontier(elseB), merge));
  EXPECT_TRUE(df.frontier(merge).empty());
}

TEST(DominanceFrontierTest, LoopLatchFrontierContainsHeader) {
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
  DominanceFrontier df(*func, dom);
  EXPECT_TRUE(containsBlock(df.frontier(body), header));
}

} // namespace toyc
