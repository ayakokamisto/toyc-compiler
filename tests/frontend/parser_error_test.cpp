/// Parser error recovery tests — P2 verification.

#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>

namespace toyc {

// Helper: parse source, expect errors, verify no crash.
static std::unique_ptr<CompUnit> expectErrors(const std::string& source,
                                               size_t minErrors = 1) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();

  // If lexer has errors, that's also fine for these tests.
  if (!diag.hasErrors()) {
    Parser parser(tokens, diag);
    auto ast = parser.parse();
    EXPECT_TRUE(diag.hasErrors()) << "Expected parser errors for: " << source;
    EXPECT_GE(diag.errorCount(), minErrors);
    EXPECT_NE(ast, nullptr) << "Parser should return partial AST";
    return ast;
  }

  EXPECT_GE(diag.errorCount(), minErrors);
  return nullptr;
}

// ── 1. Empty file ───────────────────────────────────────────────────────

TEST(ParserErrorTest, EmptyFile) {
  DiagnosticEngine diag;
  Lexer lexer("", diag);
  auto tokens = lexer.tokenize();
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  // Empty file is syntactically valid (empty CompUnit).
  EXPECT_FALSE(diag.hasErrors());
  EXPECT_NE(ast, nullptr);
  EXPECT_TRUE(ast->items().empty());
}

// ── 2. Missing semicolon ───────────────────────────────────────────────

TEST(ParserErrorTest, MissingSemicolonAfterVarDecl) {
  auto ast = expectErrors("int x = 5\nint main() { return 0; }");
  // Should still parse something.
  EXPECT_NE(ast, nullptr);
}

TEST(ParserErrorTest, MissingSemicolonAfterReturn) {
  auto ast = expectErrors("int main() { return 0 }");
  EXPECT_NE(ast, nullptr);
}

// ── 3. Missing right parenthesis ───────────────────────────────────────

TEST(ParserErrorTest, MissingRParenInFuncDef) {
  auto ast = expectErrors("int main( { return 0; }");
  EXPECT_NE(ast, nullptr);
}

TEST(ParserErrorTest, MissingRParenInCall) {
  auto ast = expectErrors("int main() { return foo(1, 2; }");
  EXPECT_NE(ast, nullptr);
}

TEST(ParserErrorTest, MissingRParenInIf) {
  auto ast = expectErrors("int main() { if (1 return 0; }");
  EXPECT_NE(ast, nullptr);
}

// ── 4. Missing right brace ─────────────────────────────────────────────

TEST(ParserErrorTest, MissingRBrace) {
  auto ast = expectErrors("int main() { return 0;");
  EXPECT_NE(ast, nullptr);
}

// ── 5. If condition missing ────────────────────────────────────────────

TEST(ParserErrorTest, IfConditionMissing) {
  auto ast = expectErrors("int main() { if return 0; }");
  EXPECT_NE(ast, nullptr);
}

// ── 6. Function param missing comma ────────────────────────────────────

TEST(ParserErrorTest, ParamMissingComma) {
  auto ast = expectErrors("int f(int a int b) { return 0; }");
  EXPECT_NE(ast, nullptr);
}

// ── 7. Call missing right paren ────────────────────────────────────────

TEST(ParserErrorTest, CallMissingRParen) {
  auto ast = expectErrors("int main() { return foo(1, 2; }");
  EXPECT_NE(ast, nullptr);
}

// ── 8. Invalid top-level token ─────────────────────────────────────────

TEST(ParserErrorTest, InvalidTopLevelToken) {
  auto ast = expectErrors("; int main() { return 0; }");
  // The leading ; is invalid at top level.
  EXPECT_NE(ast, nullptr);
}

// ── 9. Multiple consecutive errors ─────────────────────────────────────

TEST(ParserErrorTest, MultipleConsecutiveErrors) {
  auto ast = expectErrors(
      "int main() {\n"
      "  if return;\n"
      "  while ;\n"
      "  return\n"
      "}\n", 3);
  EXPECT_NE(ast, nullptr);
}

// ── 10. EOF errors ─────────────────────────────────────────────────────

TEST(ParserErrorTest, UnexpectedEOFInExpr) {
  auto ast = expectErrors("int main() { return");
  EXPECT_NE(ast, nullptr);
}

TEST(ParserErrorTest, UnexpectedEOFInFuncDef) {
  auto ast = expectErrors("int main(");
  EXPECT_NE(ast, nullptr);
}

// ── No crash / no infinite loop ────────────────────────────────────────

TEST(ParserErrorTest, NoCrashOnGarbage) {
  DiagnosticEngine diag;
  Lexer lexer("{{{{{ ;;; ;;; }}}}}", diag);
  auto tokens = lexer.tokenize();
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  // May or may not have errors, but should not crash.
  EXPECT_NE(ast, nullptr);
}

TEST(ParserErrorTest, NoCrashOnRandomTokens) {
  DiagnosticEngine diag;
  Lexer lexer("return return return return return", diag);
  auto tokens = lexer.tokenize();
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  EXPECT_NE(ast, nullptr);
}

TEST(ParserErrorTest, NoInfiniteLoopOnMissingSemicolon) {
  DiagnosticEngine diag;
  Lexer lexer("int main() { int x = 1 int y = 2; return 0; }", diag);
  auto tokens = lexer.tokenize();
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  // Should complete (no hang) and have errors.
  EXPECT_TRUE(diag.hasErrors());
  EXPECT_NE(ast, nullptr);
}

} // namespace toyc
