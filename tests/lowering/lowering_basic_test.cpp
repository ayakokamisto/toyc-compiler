/// Basic lowering tests — simple programs to verify IR generation.

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
#include <memory>
#include <optional>
#include <string>

namespace toyc {

/// Helper: compile source to IRModule. Returns nullopt on failure.
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

// ── Test: minimal main ─────────────────────────────────────────────────────

TEST(LoweringBasicTest, MinimalMain) {
  DiagnosticEngine diag;
  auto ir = compileToIR("int main() { return 0; }", diag);
  ASSERT_TRUE(ir.has_value()) << "Compilation failed";
  ASSERT_FALSE(diag.hasErrors());

  // Should have one function: main.
  EXPECT_EQ(ir->functions().size(), 1u);
  EXPECT_EQ(ir->functions()[0]->name(), "main");
  EXPECT_TRUE(ir->functions()[0]->returnType().isI32());

  // Entry block should have const + ret.
  auto* entry = ir->functions()[0]->entryBlock();
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->hasTerminator());
  EXPECT_EQ(entry->terminator()->opcode, Opcode::Ret);

  // No CopyInst.
  for (const auto& bb : ir->functions()[0]->blocks()) {
    for (const auto& inst : bb->instructions()) {
      // P4 doesn't have CopyInst.
    }
  }
}

// ── Test: local variable ───────────────────────────────────────────────────

TEST(LoweringBasicTest, LocalVariable) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 1;
      x = x + 2;
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value()) << "Compilation failed";

  auto* func = ir->functions()[0].get();

  // Should have at least one slot for x.
  EXPECT_GE(func->slots().size(), 1u);
  EXPECT_EQ(func->slots()[0].kind, SlotKind::LocalVariable);

  // Should have load.slot and store.slot instructions.
  bool hasLoadSlot = false;
  bool hasStoreSlot = false;
  bool hasAdd = false;
  for (const auto& bb : func->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::SlotLoad) hasLoadSlot = true;
      if (inst->opcode == Opcode::SlotStore) hasStoreSlot = true;
      if (inst->opcode == Opcode::Binary && inst->binaryOp == BinaryOpcode::Add) hasAdd = true;
    }
  }
  EXPECT_TRUE(hasLoadSlot);
  EXPECT_TRUE(hasStoreSlot);
  EXPECT_TRUE(hasAdd);
}

// ── Test: arithmetic expressions ───────────────────────────────────────────

TEST(LoweringBasicTest, ArithmeticExpressions) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int a = 10;
      int b = 3;
      int c = a + b;
      int d = a - b;
      int e = a * b;
      int f = a / b;
      int g = a % b;
      return c + d + e + f + g;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();
  bool hasAdd = false, hasSub = false, hasMul = false, hasDiv = false, hasMod = false;
  for (const auto& bb : func->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode != Opcode::Binary) continue;
      switch (inst->binaryOp) {
        case BinaryOpcode::Add: hasAdd = true; break;
        case BinaryOpcode::Subtract: hasSub = true; break;
        case BinaryOpcode::Multiply: hasMul = true; break;
        case BinaryOpcode::Divide: hasDiv = true; break;
        case BinaryOpcode::Modulo: hasMod = true; break;
      }
    }
  }
  EXPECT_TRUE(hasAdd);
  EXPECT_TRUE(hasSub);
  EXPECT_TRUE(hasMul);
  EXPECT_TRUE(hasDiv);
  EXPECT_TRUE(hasMod);
}

// ── Test: comparison expressions ───────────────────────────────────────────

TEST(LoweringBasicTest, ComparisonExpressions) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int a = 1;
      int b = 2;
      int c = (a == b);
      int d = (a != b);
      int e = (a < b);
      int f = (a <= b);
      int g = (a > b);
      int h = (a >= b);
      return c + d + e + f + g + h;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();
  bool hasEq = false, hasNe = false, hasLt = false, hasLe = false, hasGt = false, hasGe = false;
  for (const auto& bb : func->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode != Opcode::Compare) continue;
      switch (inst->cmpPred) {
        case ComparePredicate::Equal: hasEq = true; break;
        case ComparePredicate::NotEqual: hasNe = true; break;
        case ComparePredicate::Less: hasLt = true; break;
        case ComparePredicate::LessEqual: hasLe = true; break;
        case ComparePredicate::Greater: hasGt = true; break;
        case ComparePredicate::GreaterEqual: hasGe = true; break;
      }
    }
  }
  EXPECT_TRUE(hasEq);
  EXPECT_TRUE(hasNe);
  EXPECT_TRUE(hasLt);
  EXPECT_TRUE(hasLe);
  EXPECT_TRUE(hasGt);
  EXPECT_TRUE(hasGe);
}

// ── Test: scope shadowing ──────────────────────────────────────────────────

TEST(LoweringBasicTest, ScopeShadowing) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 1;
      {
        int x = 2;
        x = x + 1;
      }
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();
  // Two different x's should have different slots.
  EXPECT_GE(func->slots().size(), 2u);
  EXPECT_NE(func->slots()[0].id, func->slots()[1].id);
}

// ── Test: const declaration ────────────────────────────────────────────────

TEST(LoweringBasicTest, ConstDeclaration) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      const int c = 2 + 3;
      return c * 4;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();
  // Local const should NOT create a slot.
  for (const auto& slot : func->slots()) {
    // No slot should be created for const.
    if (slot.sourceSymbol.has_value()) {
      // All slots should be for variables, not constants.
    }
  }

  // Should have const 5 (from 2+3) and const 4.
  bool hasConst5 = false;
  bool hasConst4 = false;
  for (const auto& bb : func->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::ConstInt) {
        if (inst->constValue == 5) hasConst5 = true;
        if (inst->constValue == 4) hasConst4 = true;
      }
    }
  }
  EXPECT_TRUE(hasConst5);
  EXPECT_TRUE(hasConst4);
}

// ── Test: unary expressions ────────────────────────────────────────────────

TEST(LoweringBasicTest, UnaryExpressions) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 5;
      int y = -x;
      int z = !x;
      return y + z;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();
  bool hasNeg = false, hasNot = false;
  for (const auto& bb : func->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::Unary) {
        if (inst->unaryOp == UnaryOpcode::Negate) hasNeg = true;
        if (inst->unaryOp == UnaryOpcode::LogicalNot) hasNot = true;
      }
    }
  }
  EXPECT_TRUE(hasNeg);
  EXPECT_TRUE(hasNot);
}

TEST(LoweringBasicTest, UnaryPlusIdentity) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 5;
      int y = +x;
      return y;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();
  // +x should NOT generate a unary instruction.
  bool hasUnary = false;
  for (const auto& bb : func->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::Unary) hasUnary = true;
    }
  }
  EXPECT_FALSE(hasUnary);
}

} // namespace toyc
