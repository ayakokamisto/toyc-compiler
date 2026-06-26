/// Control flow tests — P3 verification of return-path checking.

#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>

namespace toyc {

static DiagnosticEngine analyzeExpectOk(const std::string& source) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) return diag;
  SemanticAnalyzer sema(diag);
  sema.analyze(*ast);
  return diag;
}

static DiagnosticEngine analyzeExpectErrors(const std::string& source) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) return diag;
  SemanticAnalyzer sema(diag);
  sema.analyze(*ast);
  return diag;
}

// ── Valid return paths ───────────────────────────────────────────────────

TEST(ControlFlowTest, IntFuncDirectReturn) {
  auto diag = analyzeExpectOk("int f() { return 0; }\nint main() { return 0; }");
  EXPECT_FALSE(diag.hasErrors());
}

TEST(ControlFlowTest, IfElseBothReturn) {
  auto diag = analyzeExpectOk(
      "int f(int x) {\n"
      "  if (x) { return 1; } else { return 0; }\n"
      "}\n"
      "int main() { return 0; }");
  EXPECT_FALSE(diag.hasErrors());
}

TEST(ControlFlowTest, WhileInfiniteNoBreak) {
  // while(1) with no break — function never falls through.
  auto diag = analyzeExpectOk(
      "int f() {\n"
      "  while (1) { }\n"
      "}\n"
      "int main() { return 0; }");
  EXPECT_FALSE(diag.hasErrors());
}

TEST(ControlFlowTest, WhileInfiniteReturn) {
  // while(1) { return 7; } — function never falls through.
  auto diag = analyzeExpectOk(
      "int f() {\n"
      "  while (1) { return 7; }\n"
      "}\n"
      "int main() { return f(); }");
  EXPECT_FALSE(diag.hasErrors());
}

TEST(ControlFlowTest, WhileZeroBodyNeverRuns) {
  // while(0) body never executes — falls through past the loop.
  auto diag = analyzeExpectErrors(
      "int f() {\n"
      "  while (0) { return 7; }\n"
      "}\n"
      "int main() { return f(); }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(ControlFlowTest, WhileBreakExitsLoop) {
  // while(1) { break; } — break makes the loop exit, function falls through.
  auto diag = analyzeExpectErrors(
      "int f() {\n"
      "  while (1) { break; }\n"
      "}\n"
      "int main() { return f(); }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(ControlFlowTest, NestedLoopBreakConsumed) {
  // Inner while(1) { break; } only affects inner loop.
  // Outer while(1) body: while(1){break}; return 7; — outer never exits.
  auto diag = analyzeExpectOk(
      "int f() {\n"
      "  while (1) {\n"
      "    while (1) { break; }\n"
      "    return 7;\n"
      "  }\n"
      "}\n"
      "int main() { return f(); }");
  EXPECT_FALSE(diag.hasErrors());
}

TEST(ControlFlowTest, WhileContinueIsInfinite) {
  // while(1) { continue; } — never exits, never returns.
  auto diag = analyzeExpectOk(
      "int f() {\n"
      "  while (1) { continue; }\n"
      "}\n"
      "int main() { return f(); }");
  EXPECT_FALSE(diag.hasErrors());
}

TEST(ControlFlowTest, WhileUnknownConditionAlwaysFallsThrough) {
  // while(x) — condition not constant, always fallsThrough=true.
  auto diag = analyzeExpectErrors(
      "int f(int x) {\n"
      "  while (x) { return 7; }\n"
      "}\n"
      "int main() { return f(1); }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(ControlFlowTest, WhileWithBreak) {
  auto diag = analyzeExpectOk(
      "int f() {\n"
      "  while (1) { break; }\n"
      "  return 0;\n"
      "}\n"
      "int main() { return 0; }");
  EXPECT_FALSE(diag.hasErrors());
}

// ── Missing return ───────────────────────────────────────────────────────

TEST(ControlFlowTest, IntFuncMissingReturn) {
  auto diag = analyzeExpectErrors(
      "int f() { }\n"
      "int main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(ControlFlowTest, IfOnlyThenReturn) {
  auto diag = analyzeExpectErrors(
      "int f(int x) {\n"
      "  if (x) { return 1; }\n"
      "}\n"
      "int main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

// ── break / continue in loop ─────────────────────────────────────────────

TEST(ControlFlowTest, BreakInLoop) {
  auto diag = analyzeExpectOk(
      "int main() {\n"
      "  while (1) { break; }\n"
      "  return 0;\n"
      "}");
  EXPECT_FALSE(diag.hasErrors());
}

TEST(ControlFlowTest, ContinueInLoop) {
  auto diag = analyzeExpectOk(
      "int main() {\n"
      "  while (1) { continue; }\n"
      "  return 0;\n"
      "}");
  EXPECT_FALSE(diag.hasErrors());
}

TEST(ControlFlowTest, BreakOutsideLoop) {
  auto diag = analyzeExpectErrors(
      "int main() {\n"
      "  break;\n"
      "  return 0;\n"
      "}");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(ControlFlowTest, ContinueOutsideLoop) {
  auto diag = analyzeExpectErrors(
      "int main() {\n"
      "  continue;\n"
      "  return 0;\n"
      "}");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(ControlFlowTest, BreakInNestedLoop) {
  auto diag = analyzeExpectOk(
      "int main() {\n"
      "  while (1) {\n"
      "    while (1) { break; }\n"
      "    break;\n"
      "  }\n"
      "  return 0;\n"
      "}");
  EXPECT_FALSE(diag.hasErrors());
}

// ── Return type mismatch ─────────────────────────────────────────────────

TEST(ControlFlowTest, IntFuncReturnVoid) {
  auto diag = analyzeExpectErrors(
      "void g() { }\n"
      "int f() { return g(); }\n"
      "int main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(ControlFlowTest, VoidFuncReturnExpr) {
  auto diag = analyzeExpectErrors(
      "void f() { return 1; }\n"
      "int main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(ControlFlowTest, IntFuncEmptyReturn) {
  auto diag = analyzeExpectErrors(
      "int f() { return; }\n"
      "int main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

} // namespace toyc
