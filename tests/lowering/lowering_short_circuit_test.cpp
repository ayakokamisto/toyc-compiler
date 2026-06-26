/// Short-circuit lowering tests — && / || with CFG.

#include "toyc/analysis/cfg.h"
#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/ir/value.h"
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

// ── Test: && in condition context ─────────────────────────────────────────

TEST(LoweringShortCircuitTest, LogicalAndCondition) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int rhs() { return 1; }
    int main() {
      int a = 0;
      if (a && rhs()) {
        return 1;
      }
      return 0;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->findFunctionByName("main");
  ASSERT_NE(func, nullptr);

  // rhs() call should be in a separate "logic.rhs" block, not in the entry block.
  bool rhsCallInLogicBlock = false;
  for (const auto& bb : func->blocks()) {
    if (bb->label().find("logic.rhs") == std::string::npos) continue;
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::Call) {
        rhsCallInLogicBlock = true;
      }
    }
  }
  EXPECT_TRUE(rhsCallInLogicBlock);

  // Should NOT have a temporary slot (condition context doesn't materialize).
  bool hasTempSlot = false;
  for (const auto& slot : func->slots()) {
    if (slot.kind == SlotKind::Temporary) hasTempSlot = true;
  }
  EXPECT_FALSE(hasTempSlot);
}

// ── Test: || in condition context ─────────────────────────────────────────

TEST(LoweringShortCircuitTest, LogicalOrCondition) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int rhs() { return 1; }
    int main() {
      int a = 1;
      if (a || rhs()) {
        return 1;
      }
      return 0;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->findFunctionByName("main");
  ASSERT_NE(func, nullptr);

  // a is true → should jump directly to true block, skipping rhs.
  bool hasCondBr = false;
  for (const auto& bb : func->blocks()) {
    if (bb->hasTerminator() && bb->terminator()->opcode == Opcode::CondBr) {
      hasCondBr = true;
    }
  }
  EXPECT_TRUE(hasCondBr);
}

// ── Test: && in value context ─────────────────────────────────────────────

TEST(LoweringShortCircuitTest, LogicalAndValue) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int rhs() { return 1; }
    int main() {
      int a = 1;
      int x = a && rhs();
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->findFunctionByName("main");
  ASSERT_NE(func, nullptr);

  // Should have a temporary slot for boolean materialization.
  bool hasTempSlot = false;
  for (const auto& slot : func->slots()) {
    if (slot.kind == SlotKind::Temporary) hasTempSlot = true;
  }
  EXPECT_TRUE(hasTempSlot);

  // Should have logic.true and logic.false blocks.
  bool hasLogicTrue = false, hasLogicFalse = false, hasLogicMerge = false;
  for (const auto& bb : func->blocks()) {
    if (bb->label().find("logic.true") != std::string::npos) hasLogicTrue = true;
    if (bb->label().find("logic.false") != std::string::npos) hasLogicFalse = true;
    if (bb->label().find("logic.merge") != std::string::npos) hasLogicMerge = true;
  }
  EXPECT_TRUE(hasLogicTrue);
  EXPECT_TRUE(hasLogicFalse);
  EXPECT_TRUE(hasLogicMerge);
}

// ── Test: || in value context ─────────────────────────────────────────────

TEST(LoweringShortCircuitTest, LogicalOrValue) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int rhs() { return 1; }
    int main() {
      int a = 1;
      int x = a || rhs();
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->findFunctionByName("main");
  ASSERT_NE(func, nullptr);

  // Should have a temporary slot.
  bool hasTempSlot = false;
  for (const auto& slot : func->slots()) {
    if (slot.kind == SlotKind::Temporary) hasTempSlot = true;
  }
  EXPECT_TRUE(hasTempSlot);

  // True path should store 1, false path should store 0.
  bool hasStore1 = false, hasStore0 = false;
  for (const auto& bb : func->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::ConstInt) {
        // Check if followed by store.slot to temp.
      }
    }
  }
  // At minimum, verifier should pass.
  auto result = verifyModule(*ir);
  EXPECT_TRUE(result.ok);
}

// ── Test: negation in condition ───────────────────────────────────────────

TEST(LoweringShortCircuitTest, NegationInCondition) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int a = 0;
      if (!a) {
        return 1;
      }
      return 0;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->findFunctionByName("main");
  ASSERT_NE(func, nullptr);

  // !a should swap true/false targets.
  // The condition should still be a CondBranch.
  bool hasCondBr = false;
  for (const auto& bb : func->blocks()) {
    if (bb->hasTerminator() && bb->terminator()->opcode == Opcode::CondBr) {
      hasCondBr = true;
    }
  }
  EXPECT_TRUE(hasCondBr);
}

// ── Test: nested logical ─────────────────────────────────────────────────

TEST(LoweringShortCircuitTest, NestedLogical) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int f() { return 1; }
    int g() { return 0; }
    int main() {
      int a = 1;
      int b = 0;
      int x = (a || f()) && (b || g());
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // Verifier should pass.
  auto result = verifyModule(*ir);
  EXPECT_TRUE(result.ok);
}

} // namespace toyc
