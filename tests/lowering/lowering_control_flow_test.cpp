/// Control flow lowering tests — if/while/break/continue.

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

// ── Test: if statement ─────────────────────────────────────────────────────

TEST(LoweringControlFlowTest, IfStatement) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 0;
      if (x) {
        x = 1;
      } else {
        x = 2;
      }
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();

  // Should have CondBranch.
  bool hasCondBr = false;
  for (const auto& bb : func->blocks()) {
    if (bb->hasTerminator() && bb->terminator()->opcode == Opcode::CondBr) {
      hasCondBr = true;
    }
  }
  EXPECT_TRUE(hasCondBr);

  // Should have at least 3 blocks (entry, then, else, merge).
  EXPECT_GE(func->blocks().size(), 3u);

  // CFG should be consistent.
  rebuildCFG(*func);
  auto result = verifyFunction(*func, *ir);
  EXPECT_TRUE(result.ok);
}

// ── Test: while loop ──────────────────────────────────────────────────────

TEST(LoweringControlFlowTest, WhileLoop) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 3;
      while (x) {
        x = x - 1;
      }
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();

  // Should have while.header, while.body, while.exit blocks.
  bool hasHeader = false, hasBody = false, hasExit = false;
  for (const auto& bb : func->blocks()) {
    if (bb->label().find("while.header") != std::string::npos) hasHeader = true;
    if (bb->label().find("while.body") != std::string::npos) hasBody = true;
    if (bb->label().find("while.exit") != std::string::npos) hasExit = true;
  }
  EXPECT_TRUE(hasHeader);
  EXPECT_TRUE(hasBody);
  EXPECT_TRUE(hasExit);
}

// ── Test: break and continue ──────────────────────────────────────────────

TEST(LoweringControlFlowTest, BreakAndContinue) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 3;
      while (x) {
        if (x == 2) {
          break;
        }
        x = x - 1;
        continue;
      }
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();

  // break should jump to while.exit.
  // continue should jump to while.header.
  // Find the break and continue blocks by checking terminators.
  bool hasBreakJump = false;
  bool hasContinueJump = false;
  for (const auto& bb : func->blocks()) {
    if (!bb->hasTerminator()) continue;
    if (bb->terminator()->opcode != Opcode::Br) continue;

    // Check if this block is the "break" or "continue" block.
    BlockId target = bb->terminator()->branchTarget;
    for (const auto& targetBB : func->blocks()) {
      if (targetBB->id() == target) {
        if (targetBB->label().find("while.exit") != std::string::npos) {
          hasBreakJump = true;
        }
        if (targetBB->label().find("while.header") != std::string::npos) {
          hasContinueJump = true;
        }
      }
    }
  }
  EXPECT_TRUE(hasBreakJump);
  EXPECT_TRUE(hasContinueJump);
}

// ── Test: nested while ────────────────────────────────────────────────────

TEST(LoweringControlFlowTest, NestedWhile) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 3;
      while (x) {
        int y = 2;
        while (y) {
          y = y - 1;
        }
        x = x - 1;
      }
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();

  // Should have multiple while.header blocks.
  int headerCount = 0;
  for (const auto& bb : func->blocks()) {
    if (bb->label().find("while.header") != std::string::npos) headerCount++;
  }
  EXPECT_GE(headerCount, 2);
}

// ── Test: if without else ─────────────────────────────────────────────────

TEST(LoweringControlFlowTest, IfWithoutElse) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      int x = 0;
      if (x) {
        x = 1;
      }
      return x;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* func = ir->functions()[0].get();

  // Should have CondBranch but only 2 successors from entry.
  bool hasCondBr = false;
  for (const auto& bb : func->blocks()) {
    if (bb->hasTerminator() && bb->terminator()->opcode == Opcode::CondBr) {
      hasCondBr = true;
    }
  }
  EXPECT_TRUE(hasCondBr);
}

// ── Test: empty main ──────────────────────────────────────────────────────

TEST(LoweringControlFlowTest, EmptyMain) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int main() {
      return 0;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());
  EXPECT_EQ(ir->functions().size(), 1u);
}

// ── Test: void function ──────────────────────────────────────────────────

TEST(LoweringControlFlowTest, VoidFunction) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    void noop() {
      return;
    }
    int main() {
      noop();
      return 0;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // Find noop function.
  const Function* noop = nullptr;
  for (const auto& f : ir->functions()) {
    if (f->name() == "noop") noop = f.get();
  }
  ASSERT_NE(noop, nullptr);
  EXPECT_TRUE(noop->returnType().isVoid());

  // noop should end with void return.
  auto* lastBlock = noop->blocks().back().get();
  EXPECT_TRUE(lastBlock->hasTerminator());
  EXPECT_EQ(lastBlock->terminator()->opcode, Opcode::Ret);
  EXPECT_FALSE(lastBlock->terminator()->returnValue.has_value());
}

} // namespace toyc
