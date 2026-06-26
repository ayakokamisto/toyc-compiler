/// Semantic error tests — P3 verification of error diagnostics.

#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>

namespace toyc {

static DiagnosticEngine analyzeExpectErrors(const std::string& source) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) return diag;

  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) return diag;

  SemanticAnalyzer sema(diag);
  sema.analyze(*ast);
  return diag;
}

// ── Undefined identifiers ────────────────────────────────────────────────

TEST(SemaErrorTest, UndefinedVar) {
  auto diag = analyzeExpectErrors("int main() { return x; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaErrorTest, UndefinedFunction) {
  auto diag = analyzeExpectErrors("int main() { return f(); }");
  EXPECT_TRUE(diag.hasErrors());
}

// ── Redefinition ─────────────────────────────────────────────────────────

TEST(SemaErrorTest, DuplicateGlobalVar) {
  auto diag = analyzeExpectErrors(
      "int x = 1;\n"
      "int x = 2;\n"
      "int main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaErrorTest, DuplicateLocalVar) {
  auto diag = analyzeExpectErrors(
      "int main() {\n"
      "  int x = 1;\n"
      "  int x = 2;\n"
      "  return 0;\n"
      "}");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaErrorTest, VarShadowsFunction) {
  auto diag = analyzeExpectErrors(
      "int f() { return 0; }\n"
      "int main() {\n"
      "  int f = 1;\n"
      "  return f;\n"
      "}");
  // This should be OK — local var shadows function.
  // Actually, in ToyC, functions and vars share the same namespace.
  // So this would be a redefinition error in the same scope?
  // No — f() is in global scope, f is in local scope. Different scopes.
  // This should be fine.
}

TEST(SemaErrorTest, DuplicateParam) {
  auto diag = analyzeExpectErrors(
      "int f(int a, int a) { return a; }\n"
      "int main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

// ── Void misuse ──────────────────────────────────────────────────────────

TEST(SemaErrorTest, VoidVarInit) {
  auto diag = analyzeExpectErrors(
      "void g() { }\n"
      "int main() { int x = g(); return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaErrorTest, VoidUnaryOperand) {
  auto diag = analyzeExpectErrors(
      "void g() { }\n"
      "int main() { int x = -g(); return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaErrorTest, VoidBinaryOperand) {
  auto diag = analyzeExpectErrors(
      "void g() { }\n"
      "int main() { int x = g() + 1; return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

// ── Const assignment ─────────────────────────────────────────────────────

TEST(SemaErrorTest, AssignToLocalConst) {
  auto diag = analyzeExpectErrors(
      "int main() {\n"
      "  const int c = 1;\n"
      "  c = 2;\n"
      "  return 0;\n"
      "}");
  EXPECT_TRUE(diag.hasErrors());
}

// ── Function misuse ──────────────────────────────────────────────────────

TEST(SemaErrorTest, NonFunctionCall) {
  auto diag = analyzeExpectErrors(
      "int main() {\n"
      "  int x = 1;\n"
      "  x();\n"
      "  return 0;\n"
      "}");
  EXPECT_TRUE(diag.hasErrors());
}

// ── No crash on errors ───────────────────────────────────────────────────

TEST(SemaErrorTest, NoCrashOnGarbage) {
  // This should not crash even with many errors.
  analyzeExpectErrors(
      "int f() {\n"
      "  const int c = x + 1;\n"
      "  break;\n"
      "  return;\n"
      "}\n"
      "\n"
      "int main(int x) {\n"
      "  return f();\n"
      "}\n");
}

} // namespace toyc
