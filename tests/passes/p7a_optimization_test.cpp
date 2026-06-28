#include "toyc/analysis/cfg.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/ir/verifier.h"
#include "toyc/lowering/ast_to_ir.h"
#include "toyc/passes/dce.h"
#include "toyc/passes/inst_combine.h"
#include "toyc/passes/ir_utils.h"
#include "toyc/passes/mem2reg.h"
#include "toyc/passes/out_of_ssa.h"
#include "toyc/passes/pass_manager.h"
#include "toyc/passes/sccp.h"
#include "toyc/passes/simplify_cfg.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/target/riscv32/instruction_selector.h"

#include <gtest/gtest.h>

#include <memory>
#include <sstream>

namespace toyc {

static Module lowerToOptimizedSSA(const std::string& source) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  SemanticAnalyzer sema(diag);
  auto model = sema.analyze(*ast);
  ASTToIRLowering lowering(*model, diag);
  auto ir = lowering.lower(*ast);
  EXPECT_TRUE(ir.has_value());
  rebuildCFG(*ir);
  EXPECT_TRUE(verifyModule(*ir, VerificationMode::CanonicalSlot).ok);
  for (const auto& func : ir->functions()) (void)removeUnreachableBlocks(*func);
  Mem2RegPass mem2reg;
  for (const auto& func : ir->functions()) (void)mem2reg.run(*func);
  rebuildCFG(*ir);
  EXPECT_TRUE(verifySSAModule(*ir).ok);

  FunctionPassManager manager;
  manager.add(std::make_unique<InstCombineLitePass>());
  manager.add(std::make_unique<SCCPPass>());
  manager.add(std::make_unique<SimplifyCFGPass>());
  manager.add(std::make_unique<DCEPass>());
  std::ostringstream diagnostics;
  for (const auto& func : ir->functions()) {
    EXPECT_TRUE(manager.runToFixedPoint(*func, *ir, 8, diagnostics)) << diagnostics.str();
  }
  rebuildCFG(*ir);
  EXPECT_TRUE(verifyModule(*ir, VerificationMode::SSA).ok);
  return std::move(*ir);
}

static const Function* mainFunc(const Module& module) {
  for (const auto& func : module.functions()) {
    if (func->name() == "main") return func.get();
  }
  return nullptr;
}

static int countOpcode(const Function& func, Opcode opcode) {
  int count = 0;
  for (const auto& block : func.blocks()) {
    for (const auto& inst : block->instructions()) {
      if (inst->opcode == opcode) ++count;
    }
  }
  return count;
}

TEST(P7AOptimizationTest, VerifyModuleSSAMatchesFullSSAVerifier) {
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
  auto one = b.emitConstInt(1);
  b.emitBranch(merge);
  b.setInsertBlock(elseB);
  b.emitBranch(merge);
  auto* phi = b.createPhi(merge, I32Type);
  b.addPhiIncoming(*phi, thenB, one);
  b.setInsertBlock(merge);
  b.emitReturn(*phi->result);
  rebuildCFG(*func);
  func->setForm(IRForm::SSA);

  auto generic = verifyModule(module, VerificationMode::SSA);
  auto full = verifySSAModule(module);
  EXPECT_FALSE(generic.ok);
  EXPECT_FALSE(full.ok);
  ASSERT_FALSE(generic.errors.empty());
  EXPECT_NE(generic.errors.front().find("Phi incoming predecessor set mismatch"), std::string::npos);
}

TEST(P7AOptimizationTest, RemoveUnreachableBeforeMem2RegDropsDeadSlotAccess) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  auto slot = func->createSlot(SlotKind::LocalVariable);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  auto dead = b.createBlock("dead");
  b.setInsertBlock(entry);
  auto zero = b.emitConstInt(0);
  b.emitReturn(zero);
  b.setInsertBlock(dead);
  b.emitStoreSlot(slot, zero);
  auto loaded = b.emitLoadSlot(slot);
  b.emitReturn(loaded);
  rebuildCFG(*func);

  EXPECT_TRUE(removeUnreachableBlocks(*func));
  Mem2RegPass mem2reg;
  (void)mem2reg.run(*func);
  rebuildCFG(*func);
  EXPECT_TRUE(verifySSAModule(module).ok);
  EXPECT_EQ(func->blocks().size(), 1u);
  EXPECT_EQ(countOpcode(*func, Opcode::SlotLoad), 0);
  EXPECT_EQ(countOpcode(*func, Opcode::SlotStore), 0);
}

TEST(P7AOptimizationTest, ConstantDeadBranchAndArithmeticFoldToReturnNine) {
  auto module = lowerToOptimizedSSA(R"(
int main() {
  int a = 1;
  int b = 2;
  int c = a + b;
  int d = c * 3;
  if (0) { return 99; }
  return d;
}
)");
  const auto* main = mainFunc(module);
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(countOpcode(*main, Opcode::CondBr), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::Binary), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotLoad), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotStore), 0);
  EXPECT_EQ(constValueOf(*main, *main->entryBlock()->terminator()->returnValue), 9);
}

TEST(P7AOptimizationTest, DCEKeepsCallsAndRemovesDeadArithmetic) {
  auto module = lowerToOptimizedSSA(R"(
int side() { return 3; }
int main() {
  int x = 1 + 2;
  side();
  return 4;
}
)");
  const auto* main = mainFunc(module);
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(countOpcode(*main, Opcode::Binary), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::Call), 1);
}

TEST(P7AOptimizationTest, OutOfSSALowersPhiAndP5AcceptsResult) {
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
  auto one = b.emitConstInt(1);
  b.emitBranch(merge);
  b.setInsertBlock(elseB);
  auto two = b.emitConstInt(2);
  b.emitBranch(merge);
  auto* phi = b.createPhi(merge, I32Type);
  b.addPhiIncoming(*phi, thenB, one);
  b.addPhiIncoming(*phi, elseB, two);
  b.setInsertBlock(merge);
  b.emitReturn(*phi->result);
  rebuildCFG(*func);
  func->setForm(IRForm::SSA);
  ASSERT_TRUE(verifySSAModule(module).ok);

  OutOfSSAPass pass;
  auto result = pass.run(*func);
  EXPECT_TRUE(result.changed);
  EXPECT_EQ(func->form(), IRForm::CanonicalSlot);
  EXPECT_EQ(countOpcode(*func, Opcode::Phi), 0);
  EXPECT_GT(countOpcode(*func, Opcode::SlotLoad), 0);
  EXPECT_GT(countOpcode(*func, Opcode::SlotStore), 0);
  EXPECT_TRUE(verifyModule(module, VerificationMode::CanonicalSlot).ok);

  DiagnosticEngine diag;
  RV32InstructionSelector selector(diag);
  auto mir = selector.lower(module);
  EXPECT_TRUE(mir.has_value());
  EXPECT_FALSE(diag.hasErrors());
}

TEST(P7AOptimizationTest, OutOfSSASplitsCriticalEdgeForPhiStore) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  auto left = b.createBlock("left");
  auto right = b.createBlock("right");
  auto other = b.createBlock("other");
  auto merge = b.createBlock("merge");
  b.setInsertBlock(entry);
  auto cond = b.emitConstInt(1);
  b.emitCondBranch(cond, left, right);
  b.setInsertBlock(left);
  auto one = b.emitConstInt(1);
  auto leftCond = b.emitConstInt(0);
  b.emitCondBranch(leftCond, merge, other);
  b.setInsertBlock(right);
  auto two = b.emitConstInt(2);
  b.emitBranch(merge);
  b.setInsertBlock(other);
  auto zero = b.emitConstInt(0);
  b.emitReturn(zero);
  auto* phi = b.createPhi(merge, I32Type);
  b.addPhiIncoming(*phi, left, one);
  b.addPhiIncoming(*phi, right, two);
  b.setInsertBlock(merge);
  b.emitReturn(*phi->result);
  rebuildCFG(*func);
  func->setForm(IRForm::SSA);
  ASSERT_TRUE(verifySSAModule(module).ok);

  OutOfSSAPass pass;
  auto result = pass.run(*func);
  EXPECT_EQ(result.splitEdgeCount, 1u);
  EXPECT_TRUE(verifyModule(module, VerificationMode::CanonicalSlot).ok);
  bool foundEdgeBlockStore = false;
  for (const auto& block : func->blocks()) {
    if (block->label().find("phi.edge.") == std::string::npos) continue;
    EXPECT_EQ(block->instructions().size(), 1u);
    EXPECT_EQ(block->instructions().front()->opcode, Opcode::SlotStore);
    foundEdgeBlockStore = true;
  }
  EXPECT_TRUE(foundEdgeBlockStore);
}

} // namespace toyc
