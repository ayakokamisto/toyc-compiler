#include "toyc/analysis/cfg.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/module.h"
#include "toyc/ir/verifier.h"
#include "toyc/target/riscv32/instruction_selector.h"

#include <gtest/gtest.h>

namespace toyc {

TEST(SSAVerifierTest, RejectsPhiMissingIncomingPredecessor) {
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

  auto result = verifySSAFunction(*func, module);
  EXPECT_FALSE(result.ok);
}

TEST(SSAVerifierTest, RejectsSlotAccessInSSA) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  auto slot = func->createSlot(SlotKind::LocalVariable);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  b.setInsertBlock(entry);
  auto value = b.emitLoadSlot(slot);
  b.emitReturn(value);
  rebuildCFG(*func);
  func->setForm(IRForm::SSA);

  auto result = verifySSAFunction(*func, module);
  EXPECT_FALSE(result.ok);
}

TEST(SSAVerifierTest, P5SelectorRejectsSSAForm) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  b.setInsertBlock(entry);
  auto zero = b.emitConstInt(0);
  b.emitReturn(zero);
  rebuildCFG(*func);
  func->setForm(IRForm::SSA);

  DiagnosticEngine diag;
  RV32InstructionSelector selector(diag);
  auto mir = selector.lower(module);
  EXPECT_FALSE(mir.has_value());
  EXPECT_TRUE(diag.hasErrors());
}

} // namespace toyc
