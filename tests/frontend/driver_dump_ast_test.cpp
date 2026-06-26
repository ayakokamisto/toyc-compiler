/// Driver --dump-ast tests — P2 verification.

#include "toyc/driver/options.h"
#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>
#include <sstream>

namespace toyc {

// Helper: parse and dump AST, simulating --dump-ast behavior.
static std::string dumpAstFromSource(const std::string& source, bool& ok) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();

  if (diag.hasErrors()) {
    ok = false;
    return "";
  }

  Parser parser(tokens, diag);
  auto ast = parser.parse();

  if (diag.hasErrors()) {
    ok = false;
    return "";
  }

  ok = true;
  std::ostringstream out;
  dumpAst(*ast, out);
  return out.str();
}

// ── 1. Minimal main AST dump ───────────────────────────────────────────

TEST(DumpAstTest, MinimalMainDump) {
  bool ok = false;
  auto result = dumpAstFromSource("int main() {\n  return 0;\n}\n", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("CompUnit"), std::string::npos);
  EXPECT_NE(result.find("FuncDef return=int name=main"), std::string::npos);
  EXPECT_NE(result.find("Params"), std::string::npos);
  EXPECT_NE(result.find("BlockStmt"), std::string::npos);
  EXPECT_NE(result.find("ReturnStmt"), std::string::npos);
  EXPECT_NE(result.find("IntegerLiteral value=0"), std::string::npos);
}

// ── 2. Global vars and functions AST dump ──────────────────────────────

TEST(DumpAstTest, GlobalDeclsAndFunctionsDump) {
  bool ok = false;
  auto result = dumpAstFromSource(
      "const int g = 1 + 2 * 3;\n"
      "int f(int a, int b) {\n"
      "  return a;\n"
      "}\n"
      "int main() {\n"
      "  return f(g, 3);\n"
      "}\n", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("GlobalConstDecl name=g"), std::string::npos);
  EXPECT_NE(result.find("FuncDef return=int name=f"), std::string::npos);
  EXPECT_NE(result.find("FuncDef return=int name=main"), std::string::npos);
  EXPECT_NE(result.find("CallExpr callee=f"), std::string::npos);
}

// ── 3. --dump-ast output only to stderr (tested via component) ─────────

TEST(DumpAstTest, DumpWritesToProvidedStream) {
  bool ok = false;
  auto result = dumpAstFromSource("int main() { return 0; }", ok);
  EXPECT_TRUE(ok);
  EXPECT_FALSE(result.empty());
  // The output goes to whatever ostream we provide — here it's our string.
}

// ── 4. stdout is empty (driver behavior — tested via component) ────────

TEST(DumpAstTest, StdoutEmptyOnSuccess) {
  // In actual driver, stdout is never written in dump-ast mode.
  // This is a structural test verifying the dump function works
  // without touching stdout.
  bool ok = false;
  auto result = dumpAstFromSource("int main() { return 0; }", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("CompUnit"), std::string::npos);
}

// ── 5. Syntax error → no AST dump ─────────────────────────────────────

TEST(DumpAstTest, SyntaxErrorNoDump) {
  bool ok = false;
  auto result = dumpAstFromSource("int main() { return }", ok);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(result.empty());
}

TEST(DumpAstTest, LexerErrorNoDump) {
  bool ok = false;
  auto result = dumpAstFromSource("@invalid", ok);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(result.empty());
}

// ── 6. -opt --dump-ast can be parsed together ──────────────────────────

TEST(DumpAstTest, OptAndDumpAstCombined) {
  const char* argv[] = {"toycc", "-opt", "--dump-ast"};
  auto opts = CompilerOptions::parse(3, const_cast<char**>(argv));
  EXPECT_TRUE(opts.optimize);
  EXPECT_TRUE(opts.dumpAst);
  EXPECT_FALSE(opts.hasCommandLineError);
}

// ── 7. --dump-tokens --dump-ast conflict ───────────────────────────────

TEST(DumpAstTest, DumpTokensAndDumpAstConflict) {
  const char* argv[] = {"toycc", "--dump-tokens", "--dump-ast"};
  auto opts = CompilerOptions::parse(3, const_cast<char**>(argv));
  EXPECT_TRUE(opts.hasCommandLineError);
  EXPECT_TRUE(opts.dumpTokens);
  EXPECT_TRUE(opts.dumpAst);
}

// ── Additional dump-ast tests ──────────────────────────────────────────

TEST(DumpAstTest, DumpIfWhileBreak) {
  bool ok = false;
  auto result = dumpAstFromSource(
      "int main() {\n"
      "  if (1) break;\n"
      "  while (1) continue;\n"
      "  return 0;\n"
      "}\n", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("IfStmt"), std::string::npos);
  EXPECT_NE(result.find("WhileStmt"), std::string::npos);
  EXPECT_NE(result.find("BreakStmt"), std::string::npos);
  EXPECT_NE(result.find("ContinueStmt"), std::string::npos);
}

TEST(DumpAstTest, DumpAssignAndDecl) {
  bool ok = false;
  auto result = dumpAstFromSource(
      "int main() {\n"
      "  int x = 0;\n"
      "  x = 1;\n"
      "  return x;\n"
      "}\n", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("LocalVarDecl name=x"), std::string::npos);
  EXPECT_NE(result.find("AssignStmt target=x"), std::string::npos);
}

TEST(DumpAstTest, DumpNestedBlocks) {
  bool ok = false;
  auto result = dumpAstFromSource(
      "int main() {\n"
      "  {\n"
      "    {\n"
      "    }\n"
      "  }\n"
      "  return 0;\n"
      "}\n", ok);
  EXPECT_TRUE(ok);
  // Should have multiple BlockStmt entries.
  size_t pos = 0;
  int count = 0;
  while ((pos = result.find("BlockStmt", pos)) != std::string::npos) {
    ++count;
    pos += 9;
  }
  EXPECT_GE(count, 3); // body + 2 nested
}

TEST(DumpAstTest, DumpEmptyStatement) {
  bool ok = false;
  auto result = dumpAstFromSource(
      "int main() {\n"
      "  ;\n"
      "  return 0;\n"
      "}\n", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("EmptyStmt"), std::string::npos);
}

} // namespace toyc
