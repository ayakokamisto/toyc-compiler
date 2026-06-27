#include "toyc/analysis/cfg.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/ir/verifier.h"
#include "toyc/lowering/ast_to_ir.h"
#include "toyc/passes/mem2reg.h"
#include "toyc/sema/semantic_analyzer.h"

#include <gtest/gtest.h>

namespace toyc {

static Module lowerToSSA(const std::string& source) {
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
  Mem2RegPass pass;
  for (const auto& func : ir->functions()) {
    (void)pass.run(*func);
  }
  rebuildCFG(*ir);
  EXPECT_TRUE(verifySSAModule(*ir).ok);
  return std::move(*ir);
}

static const Function* findFunc(const Module& module, const std::string& name) {
  for (const auto& func : module.functions()) {
    if (func->name() == name) return func.get();
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

TEST(Mem2RegTest, LinearLocalHasNoPhiOrSlotAccess) {
  auto module = lowerToSSA("int main() { int x = 1; x = x + 2; return x; }");
  const auto* main = findFunc(module, "main");
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(main->form(), IRForm::SSA);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotLoad), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotStore), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::Phi), 0);
}

TEST(Mem2RegTest, IfElseInsertsPhiAtMerge) {
  auto module = lowerToSSA(R"(
int main() {
  int x = 0;
  if (x) { x = 1; } else { x = 2; }
  return x;
}
)");
  const auto* main = findFunc(module, "main");
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotLoad), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotStore), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::Phi), 1);
}

TEST(Mem2RegTest, WhileInsertsLoopHeaderPhis) {
  auto module = lowerToSSA(R"(
int main() {
  int i = 0;
  int sum = 0;
  while (i < 5) {
    sum = sum + i;
    i = i + 1;
  }
  return sum;
}
)");
  const auto* main = findFunc(module, "main");
  ASSERT_NE(main, nullptr);
  EXPECT_GE(countOpcode(*main, Opcode::Phi), 2);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotLoad), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotStore), 0);
}

TEST(Mem2RegTest, WriteOnlySlotGetsRemovedWithoutPhi) {
  auto module = lowerToSSA("int main() { int x = 0; x = 1; return 0; }");
  const auto* main = findFunc(module, "main");
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(countOpcode(*main, Opcode::Phi), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotLoad), 0);
  EXPECT_EQ(countOpcode(*main, Opcode::SlotStore), 0);
}

} // namespace toyc
