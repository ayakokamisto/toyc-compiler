/// Driver --dump-sema tests — P3 verification.

#include "toyc/driver/options.h"
#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/sema/semantic_model.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>
#include <sstream>

namespace toyc {

// Helper: analyze and dump sema to string.
static std::string dumpSemaFromSource(const std::string& source, bool& ok) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) { ok = false; return ""; }

  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) { ok = false; return ""; }

  SemanticAnalyzer sema(diag);
  auto model = sema.analyze(*ast);
  if (diag.hasErrors()) { ok = false; return ""; }

  ok = true;
  std::ostringstream out;
  dumpSema(*model, *ast, out);
  return out.str();
}

// ── 1. Minimal main dump ────────────────────────────────────────────────

TEST(DumpSemaTest, MinimalMainDump) {
  bool ok = false;
  auto result = dumpSemaFromSource("int main() { return 0; }", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("SemanticModel"), std::string::npos);
  EXPECT_NE(result.find("Function name=main return=int"), std::string::npos);
}

// ── 2. Global const, var, function ──────────────────────────────────────

TEST(DumpSemaTest, GlobalSymbolsDump) {
  bool ok = false;
  auto result = dumpSemaFromSource(
      "const int c = 42;\n"
      "int g = 10;\n"
      "int f(int a) { return a; }\n"
      "int main() { return f(g); }", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("GlobalConst name=c"), std::string::npos);
  EXPECT_NE(result.find("GlobalVar name=g"), std::string::npos);
  EXPECT_NE(result.find("Function name=f"), std::string::npos);
}

// ── 3. Dump only to stderr (component test) ─────────────────────────────

TEST(DumpSemaTest, DumpWritesToStream) {
  bool ok = false;
  auto result = dumpSemaFromSource("int main() { return 0; }", ok);
  EXPECT_TRUE(ok);
  EXPECT_FALSE(result.empty());
}

// ── 4. Sema error → no dump ─────────────────────────────────────────────

TEST(DumpSemaTest, SemaErrorNoDump) {
  bool ok = false;
  auto result = dumpSemaFromSource("int main() { return x; }", ok);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(result.empty());
}

// ── 5. -opt --dump-sema can be parsed ────────────────────────────────────

TEST(DumpSemaTest, OptAndDumpSemaCombined) {
  const char* argv[] = {"toycc", "-opt", "--dump-sema"};
  auto opts = CompilerOptions::parse(3, const_cast<char**>(argv));
  EXPECT_TRUE(opts.optimize);
  EXPECT_TRUE(opts.dumpSema);
  EXPECT_FALSE(opts.hasCommandLineError);
}

// ── 6. Three dump modes conflict ─────────────────────────────────────────

TEST(DumpSemaTest, DumpTokensAndSemaConflict) {
  const char* argv[] = {"toycc", "--dump-tokens", "--dump-sema"};
  auto opts = CompilerOptions::parse(3, const_cast<char**>(argv));
  EXPECT_TRUE(opts.hasCommandLineError);
}

TEST(DumpSemaTest, DumpAstAndSemaConflict) {
  const char* argv[] = {"toycc", "--dump-ast", "--dump-sema"};
  auto opts = CompilerOptions::parse(3, const_cast<char**>(argv));
  EXPECT_TRUE(opts.hasCommandLineError);
}

TEST(DumpSemaTest, AllThreeConflict) {
  const char* argv[] = {"toycc", "--dump-tokens", "--dump-ast", "--dump-sema"};
  auto opts = CompilerOptions::parse(4, const_cast<char**>(argv));
  EXPECT_TRUE(opts.hasCommandLineError);
}

// ── 7. Unknown option ───────────────────────────────────────────────────

TEST(DumpSemaTest, UnknownOption) {
  const char* argv[] = {"toycc", "--unknown"};
  auto opts = CompilerOptions::parse(2, const_cast<char**>(argv));
  EXPECT_TRUE(opts.hasCommandLineError);
}

// ── 8. Dump format stability ────────────────────────────────────────────

TEST(DumpSemaTest, FormatStable) {
  bool ok = false;
  auto result = dumpSemaFromSource(
      "const int c = 5;\n"
      "int main() { return c; }", ok);
  EXPECT_TRUE(ok);
  EXPECT_NE(result.find("GlobalConst name=c type=int value=5"), std::string::npos);
}

} // namespace toyc
