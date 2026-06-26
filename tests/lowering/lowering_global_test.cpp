/// Global lowering tests — global variables, constants, and runtime init.

#include "toyc/analysis/cfg.h"
#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/ir/verifier.h"
#include "toyc/lowering/ast_to_ir.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace toyc {

static std::optional<Module> compileToIR(const std::string& source, DiagnosticEngine& diag) {
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) return std::nullopt;
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) return std::nullopt;
  SemanticAnalyzer sema(diag);
  auto model = sema.analyze(*ast);
  if (diag.hasErrors()) return std::nullopt;
  ASTToIRLowering lowering(*model, diag);
  auto ir = lowering.lower(*ast);
  if (!ir.has_value()) return std::nullopt;
  rebuildCFG(*ir);
  auto verifyResult = verifyModule(*ir);
  if (!verifyResult.ok) {
    for (const auto& err : verifyResult.errors) {
      diag.error(SourceLocation{}, "IR verification: " + err);
    }
    return std::nullopt;
  }
  return ir;
}

// ── Test: global constant ─────────────────────────────────────────────────

TEST(LoweringGlobalTest, GlobalConstant) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    const int c = 2 + 3;
    int main() {
      return c * 4;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // c should be a constant global with static value 5.
  const IRGlobal* cGlobal = nullptr;
  for (const auto& g : ir->globals()) {
    if (g.name == "c") cGlobal = &g;
  }
  ASSERT_NE(cGlobal, nullptr);
  EXPECT_EQ(cGlobal->kind, GlobalKind::Constant);
  EXPECT_EQ(cGlobal->initKind, IRGlobalInitKind::Static);
  EXPECT_EQ(cGlobal->staticInitialValue, 5);

  // Using c should generate ConstInt(5), not LoadGlobal.
  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  bool hasLoadGlobalC = false;
  bool hasConst5 = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalLoad) {
        // Check if it's loading from c.
        hasLoadGlobalC = true;
      }
      if (inst->opcode == Opcode::ConstInt && inst->constValue == 5) {
        hasConst5 = true;
      }
    }
  }
  EXPECT_TRUE(hasConst5);
  EXPECT_FALSE(hasLoadGlobalC);  // Constants should not be loaded.
}

// ── Test: global variable (static) ───────────────────────────────────────

TEST(LoweringGlobalTest, GlobalVariableStatic) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    const int c = 5;
    int g = c * 4;
    int main() {
      return g + c;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // g should be a variable global with static value 20.
  const IRGlobal* gGlobal = nullptr;
  for (const auto& g : ir->globals()) {
    if (g.name == "g") gGlobal = &g;
  }
  ASSERT_NE(gGlobal, nullptr);
  EXPECT_EQ(gGlobal->kind, GlobalKind::Variable);
  EXPECT_EQ(gGlobal->initKind, IRGlobalInitKind::Static);
  EXPECT_EQ(gGlobal->staticInitialValue, 20);

  // Using g should generate LoadGlobal.
  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  bool hasLoadGlobalG = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalLoad) {
        hasLoadGlobalG = true;
      }
    }
  }
  EXPECT_TRUE(hasLoadGlobalG);
}

// ── Test: global variable (runtime) ──────────────────────────────────────

TEST(LoweringGlobalTest, GlobalVariableRuntime) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int seed() { return 7; }
    int g = seed();
    int main() {
      return g;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // g should be runtime-zero-initialized.
  const IRGlobal* gGlobal = nullptr;
  for (const auto& g : ir->globals()) {
    if (g.name == "g") gGlobal = &g;
  }
  ASSERT_NE(gGlobal, nullptr);
  EXPECT_EQ(gGlobal->kind, GlobalKind::Variable);
  EXPECT_EQ(gGlobal->initKind, IRGlobalInitKind::RuntimeZeroInitialized);

  // Should have .Ltoyc.global_init_guard.
  bool hasGuard = false;
  for (const auto& g : ir->globals()) {
    if (g.name == ".Ltoyc.global_init_guard") hasGuard = true;
  }
  EXPECT_TRUE(hasGuard);

  // Should have .Ltoyc.global_init function.
  bool hasInitFunc = false;
  for (const auto& f : ir->functions()) {
    if (f->name() == ".Ltoyc.global_init") hasInitFunc = true;
  }
  EXPECT_TRUE(hasInitFunc);

  // main should call .Ltoyc.global_init.
  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  bool hasCallInit = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::Call) {
        const auto* callee = ir->findFunction(inst->callee);
        if (callee && callee->name() == ".Ltoyc.global_init") {
          hasCallInit = true;
        }
      }
    }
  }
  EXPECT_TRUE(hasCallInit);
}

// ── Test: global assignment ──────────────────────────────────────────────

TEST(LoweringGlobalTest, GlobalAssignment) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int g = 0;
    int main() {
      g = 42;
      return g;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  bool hasStoreGlobal = false;
  bool hasLoadGlobal = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalStore) hasStoreGlobal = true;
      if (inst->opcode == Opcode::GlobalLoad) hasLoadGlobal = true;
    }
  }
  EXPECT_TRUE(hasStoreGlobal);
  EXPECT_TRUE(hasLoadGlobal);
}

// ── Test: multiple runtime globals ───────────────────────────────────────

TEST(LoweringGlobalTest, MultipleRuntimeGlobals) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int f() { return 1; }
    int g() { return 2; }
    int a = f();
    int b = g();
    int main() {
      return a + b;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // Both a and b should be runtime-zero-initialized.
  int runtimeCount = 0;
  for (const auto& g : ir->globals()) {
    if (g.initKind == IRGlobalInitKind::RuntimeZeroInitialized) runtimeCount++;
  }
  EXPECT_GE(runtimeCount, 2);
}

} // namespace toyc
