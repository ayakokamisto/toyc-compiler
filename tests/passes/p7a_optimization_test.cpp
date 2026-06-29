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
#include "toyc/mir/verifier.h"
#include "toyc/target/riscv32/asm_emitter.h"
#include "toyc/target/riscv32/instruction_selector.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

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
  for (const auto& func : ir->functions()) (void)eliminateSelfTailRecursion(*func);
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
  (void)foldGlobalConstantLoads(*ir);
  std::ostringstream diagnostics;
  for (const auto& func : ir->functions()) {
    EXPECT_TRUE(manager.runToFixedPoint(*func, *ir, 3, diagnostics)) << diagnostics.str();
  }
  (void)foldGlobalConstantLoads(*ir);
  rebuildCFG(*ir);
  EXPECT_TRUE(verifyModule(*ir, VerificationMode::SSA).ok);
  return std::move(*ir);
}

static std::string compileOptimizedAssembly(const std::string& source) {
  auto module = lowerToOptimizedSSA(source);
  OutOfSSAPass outOfSSA;
  for (const auto& func : module.functions()) {
    (void)outOfSSA.run(*func);
    (void)cleanupCanonicalSlots(*func);
  }
  rebuildCFG(module);
  EXPECT_TRUE(verifyModule(module, VerificationMode::CanonicalSlot).ok);

  DiagnosticEngine diag;
  RV32InstructionSelector selector(diag);
  auto mir = selector.lower(module);
  EXPECT_TRUE(mir.has_value());
  EXPECT_FALSE(diag.hasErrors());
  EXPECT_TRUE(verifyMIR(*mir).ok);
  riscv32::SpillAllAllocator allocator;
  auto allocated = allocator.allocate(std::move(*mir));
  return riscv32::emitAssembly(allocated, true);
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

static int countTerminators(const Function& func, Opcode opcode) {
  int count = 0;
  for (const auto& block : func.blocks()) {
    if (block->hasTerminator() && block->terminator()->opcode == opcode) ++count;
  }
  return count;
}

static int countBinaryOp(const Function& func, BinaryOpcode opcode) {
  int count = 0;
  for (const auto& block : func.blocks()) {
    for (const auto& inst : block->instructions()) {
      if (inst->opcode == Opcode::Binary && inst->binaryOp == opcode) ++count;
    }
  }
  return count;
}

static std::string constantAddChainSource(int terms) {
  std::string source = "int main() { return 0";
  for (int i = 0; i < terms; ++i) source += " + 1";
  source += "; }";
  return source;
}

static std::string sideEffectAddChainSource(int terms) {
  std::string source = "int side() { return 7; }\nint main() { return side()";
  for (int i = 0; i < terms; ++i) source += " + 1";
  source += "; }";
  return source;
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

TEST(P7DOptimizationTest, LocalCSEReusesCommonExpressionInBlock) {
  auto module = lowerToOptimizedSSA(R"(
int id(int x) { return x; }
int main() {
  int a = id(1);
  int b = id(2);
  int x = a + b;
  int y = a + b;
  return x * y;
}
)");
  const auto* main = mainFunc(module);
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(countBinaryOp(*main, BinaryOpcode::Add), 1);
}

TEST(P7DOptimizationTest, GlobalConstantLoadFoldsToConstInt) {
  Module module;
  IRGlobal global;
  global.name = "g";
  global.kind = GlobalKind::Constant;
  global.initKind = IRGlobalInitKind::Static;
  global.staticInitialValue = 7;
  auto gid = module.createGlobal(std::move(global));
  auto* func = module.createFunction("main", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  b.setInsertBlock(entry);
  auto value = b.emitLoadGlobal(gid);
  b.emitReturn(value);
  rebuildCFG(module);

  EXPECT_TRUE(foldGlobalConstantLoads(module));
  EXPECT_EQ(countOpcode(*func, Opcode::GlobalLoad), 0);
  EXPECT_EQ(countOpcode(*func, Opcode::ConstInt), 1);
  ASSERT_TRUE(func->entryBlock()->terminator()->returnValue.has_value());
  EXPECT_EQ(constValueOf(*func, *func->entryBlock()->terminator()->returnValue), 7);
}

TEST(P7DOptimizationTest, CanonicalCleanupRemovesLocalTempStoreLoadPair) {
  Module module;
  auto* func = module.createFunction("main", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  b.setInsertBlock(entry);
  auto slot = func->createSlot(SlotKind::Temporary);
  auto value = b.emitConstInt(5);
  b.emitStoreSlot(slot, value);
  auto loaded = b.emitLoadSlot(slot);
  b.emitReturn(loaded);
  rebuildCFG(module);

  EXPECT_TRUE(cleanupCanonicalSlots(*func));
  EXPECT_TRUE(verifyModule(module, VerificationMode::CanonicalSlot).ok);
  EXPECT_EQ(countOpcode(*func, Opcode::SlotLoad), 0);
  EXPECT_EQ(countOpcode(*func, Opcode::SlotStore), 0);
  ASSERT_TRUE(func->entryBlock()->terminator()->returnValue.has_value());
  EXPECT_EQ(*func->entryBlock()->terminator()->returnValue, value);
}

TEST(P7DOptimizationTest, OptCodegenSmokeCoversFunctionalRegressionShapes) {
  const std::vector<std::string> cases = {
      "int main(){ if (1) { return 3; } else { return 4; } }",
      "int main(){ int i=0; while(i<4){ if(i==2){break;} i=i+1; } return i; }",
      "int main(){ int i=0; int s=0; while(i<5){ i=i+1; if(i==2){continue;} s=s+i; } return s; }",
      "int side(){return 1;} int main(){ int x=0; if(x && side()){return 9;} return 2; }",
      "int main(){ int i=0; int s=0; while(i<3){ if(i==1){s=s+2;} else {s=s+1;} i=i+1;} return s; }",
      "int main(){ if(0){return 1;} if(1){return 2;} return 3; }",
      "int main(){ int a0=0; int a1=1; int a2=2; int a3=3; int a4=4; int a5=5; return a0+a1+a2+a3+a4+a5; }",
      "int sum9(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8){return a0+a1+a2+a3+a4+a5+a6+a7+a8;} int main(){return sum9(1,2,3,4,5,6,7,8,9);}",
      "const int c=2+3; int main(){ return c+1; }",
  };
  for (const auto& source : cases) {
    SCOPED_TRACE(source);
    auto asmText = compileOptimizedAssembly(source);
    EXPECT_NE(asmText.find(".section .text"), std::string::npos);
    EXPECT_NE(asmText.find("ret"), std::string::npos);
    EXPECT_EQ(asmText.find("%v"), std::string::npos);
  }
}

TEST(P7DOptimizationTest, SelfTailRecursionRewritesCallToLoop) {
  auto module = lowerToOptimizedSSA(R"(
int fact(int n, int acc) {
  if (n <= 1) { return acc; }
  return fact(n - 1, acc * n);
}
int main() { return fact(5, 1); }
)");
  const Function* fact = nullptr;
  for (const auto& func : module.functions()) {
    if (func->name() == "fact") fact = func.get();
  }
  ASSERT_NE(fact, nullptr);
  EXPECT_EQ(countOpcode(*fact, Opcode::Call), 0);
  EXPECT_GT(countOpcode(*fact, Opcode::Phi), 0);
}

TEST(P7BCompileTimeTest, LongConstantChainFoldsToSingleReturnConstant) {
  auto module = lowerToOptimizedSSA(constantAddChainSource(3000));
  const auto* main = mainFunc(module);
  ASSERT_NE(main, nullptr);
  ASSERT_NE(main->entryBlock(), nullptr);
  auto* terminator = main->entryBlock()->terminator();
  ASSERT_NE(terminator, nullptr);
  ASSERT_TRUE(terminator->returnValue.has_value());
  EXPECT_EQ(constValueOf(*main, *terminator->returnValue), 3000);
  EXPECT_EQ(countOpcode(*main, Opcode::Binary), 0);
}

TEST(P7BCompileTimeTest, LongChainWithCallKeepsSideEffect) {
  auto module = lowerToOptimizedSSA(sideEffectAddChainSource(128));
  const auto* main = mainFunc(module);
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(countOpcode(*main, Opcode::Call), 1);
}

TEST(P7BCompileTimeTest, LoopPhiKeepsExitConditionDynamic) {
  auto module = lowerToOptimizedSSA(R"(
int input(int x) { return x; }
int main() {
  int n = input(10);
  int i = 0;
  int x = 0;
  while (i < n) {
    x = x + i;
    i = i + 1;
  }
  return x;
}
)");
  const auto* main = mainFunc(module);
  ASSERT_NE(main, nullptr);
  EXPECT_GT(countOpcode(*main, Opcode::Phi), 0);
  EXPECT_GT(countTerminators(*main, Opcode::CondBr), 0);
}

TEST(P7AOptimizationTest, SimplifyCFGPrunesPhiIncomingAfterEdgeRemoval) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  auto header = b.createBlock("while.header");
  auto latch = b.createBlock("while.latch");
  auto exit = b.createBlock("while.exit");

  b.setInsertBlock(entry);
  auto zero = b.emitConstInt(0);
  b.emitBranch(header);

  auto* phi = b.createPhi(header, I32Type);
  b.addPhiIncoming(*phi, entry, zero);
  b.setInsertBlock(header);
  b.emitBranch(latch);

  b.setInsertBlock(latch);
  auto one = b.emitConstInt(1);
  auto stop = b.emitConstInt(0);
  b.addPhiIncoming(*phi, latch, one);
  b.emitCondBranch(stop, header, exit);

  b.setInsertBlock(exit);
  b.emitReturn(*phi->result);
  rebuildCFG(*func);
  func->setForm(IRForm::SSA);
  ASSERT_TRUE(verifySSAModule(module).ok);

  SimplifyCFGPass pass;
  EXPECT_TRUE(pass.run(*func).changed);
  rebuildCFG(*func);
  EXPECT_TRUE(verifySSAModule(module).ok);
}

TEST(P7AOptimizationTest, SimplifyCFGRewritesPhiIncomingAfterLinearMerge) {
  Module module;
  auto* func = module.createFunction("f", I32Type);
  IRBuilder b;
  b.setFunction(func);
  auto entry = b.createBlock("entry");
  auto header = b.createBlock("while.header");
  auto body = b.createBlock("while.body");
  auto latch = b.createBlock("while.latch");

  b.setInsertBlock(entry);
  auto zero = b.emitConstInt(0);
  b.emitBranch(header);

  auto* phi = b.createPhi(header, I32Type);
  b.addPhiIncoming(*phi, entry, zero);

  b.setInsertBlock(header);
  b.emitBranch(body);

  b.setInsertBlock(body);
  b.emitBranch(latch);

  b.setInsertBlock(latch);
  auto one = b.emitConstInt(1);
  b.addPhiIncoming(*phi, latch, one);
  b.emitBranch(header);

  rebuildCFG(*func);
  func->setForm(IRForm::SSA);
  ASSERT_TRUE(verifySSAModule(module).ok);

  SimplifyCFGPass pass;
  EXPECT_TRUE(pass.run(*func).changed);
  rebuildCFG(*func);
  EXPECT_TRUE(verifySSAModule(module).ok);
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
