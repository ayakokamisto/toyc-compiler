/// Driver / dump-tokens tests — P1 verification of --dump-tokens behavior.

#include "toyc/driver/options.h"
#include "toyc/frontend/lexer.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>
#include <sstream>

namespace toyc {

// ── CompilerOptions parsing ──────────────────────────────────────────────

TEST(DriverOptionsTest, DumpTokensFlag) {
  const char* argv[] = {"toycc", "--dump-tokens"};
  auto opts = CompilerOptions::parse(2, const_cast<char**>(argv));
  EXPECT_TRUE(opts.dumpTokens);
  EXPECT_FALSE(opts.help);
  EXPECT_FALSE(opts.hasCommandLineError);
}

TEST(DriverOptionsTest, OptAndDumpTokensCombined) {
  const char* argv[] = {"toycc", "-opt", "--dump-tokens"};
  auto opts = CompilerOptions::parse(3, const_cast<char**>(argv));
  EXPECT_TRUE(opts.optimize);
  EXPECT_TRUE(opts.dumpTokens);
  EXPECT_FALSE(opts.help);
  EXPECT_FALSE(opts.hasCommandLineError);
}

TEST(DriverOptionsTest, DumpTokensAndOptCombined) {
  const char* argv[] = {"toycc", "--dump-tokens", "-opt"};
  auto opts = CompilerOptions::parse(3, const_cast<char**>(argv));
  EXPECT_TRUE(opts.dumpTokens);
  EXPECT_TRUE(opts.optimize);
}

TEST(DriverOptionsTest, UnknownArgSetsParseError) {
  const char* argv[] = {"toycc", "--unknown-flag"};
  auto opts = CompilerOptions::parse(2, const_cast<char**>(argv));
  EXPECT_TRUE(opts.hasCommandLineError);
}

TEST(DriverOptionsTest, HelpFlag) {
  const char* argv[] = {"toycc", "--help"};
  auto opts = CompilerOptions::parse(2, const_cast<char**>(argv));
  EXPECT_TRUE(opts.help);
  EXPECT_FALSE(opts.hasCommandLineError);
}

TEST(DriverOptionsTest, AllFlagsDefaultOff) {
  const char* argv[] = {"toycc"};
  auto opts = CompilerOptions::parse(1, const_cast<char**>(argv));
  EXPECT_FALSE(opts.help);
  EXPECT_FALSE(opts.optimize);
  EXPECT_FALSE(opts.dumpTokens);
  EXPECT_FALSE(opts.verbose);
  EXPECT_FALSE(opts.hasCommandLineError);
}

// ── dumpTokens output format ─────────────────────────────────────────────

TEST(DumpTokensTest, FormatLineColumnKindLexeme) {
  DiagnosticEngine diag;
  Lexer lexer("int x;", diag);
  auto tokens = lexer.tokenize();

  std::ostringstream out;
  dumpTokens(tokens, out);
  std::string output = out.str();

  // Expected:
  // 1:1 KW_INT 'int'
  // 1:5 IDENT 'x'
  // 1:6 SEMICOLON ';'
  // 1:7 END_OF_FILE ''
  EXPECT_NE(output.find("1:1 KW_INT 'int'"), std::string::npos);
  EXPECT_NE(output.find("1:5 IDENT 'x'"), std::string::npos);
  EXPECT_NE(output.find("1:6 SEMICOLON ';'"), std::string::npos);
  EXPECT_NE(output.find("1:7 END_OF_FILE ''"), std::string::npos);
}

TEST(DumpTokensTest, FormatEscapesNewline) {
  DiagnosticEngine diag;
  // Source with newline in a comment (but tokens don't contain newlines normally).
  // Let's test the escape function indirectly through a token with a newline lexeme.
  // We'll construct such a token manually.
  Token tok(TokenKind::INVALID, "a\nb",
            SourceRange{SourceLocation{0, 1, 1}, SourceLocation{3, 1, 4}});
  std::vector<Token> tokens = {tok,
    Token(TokenKind::END_OF_FILE, "", SourceRange{SourceLocation{3, 1, 4},
                                                    SourceLocation{3, 1, 4}})};

  std::ostringstream out;
  dumpTokens(tokens, out);
  std::string output = out.str();

  EXPECT_NE(output.find("a\\nb"), std::string::npos);
}

TEST(DumpTokensTest, FormatEscapesBackslash) {
  Token tok(TokenKind::IDENT, "a\\b",
            SourceRange{SourceLocation{0, 1, 1}, SourceLocation{3, 1, 4}});
  std::vector<Token> tokens = {tok,
    Token(TokenKind::END_OF_FILE, "", SourceRange{})};

  std::ostringstream out;
  dumpTokens(tokens, out);
  std::string output = out.str();

  EXPECT_NE(output.find("a\\\\b"), std::string::npos);
}

TEST(DumpTokensTest, FormatEscapesSingleQuote) {
  Token tok(TokenKind::IDENT, "a'b",
            SourceRange{SourceLocation{0, 1, 1}, SourceLocation{3, 1, 4}});
  std::vector<Token> tokens = {tok,
    Token(TokenKind::END_OF_FILE, "", SourceRange{})};

  std::ostringstream out;
  dumpTokens(tokens, out);
  std::string output = out.str();

  EXPECT_NE(output.find("a\\'b"), std::string::npos);
}

TEST(DumpTokensTest, FormatMultiLineTokens) {
  DiagnosticEngine diag;
  Lexer lexer("int\nx = 1;", diag);
  auto tokens = lexer.tokenize();

  std::ostringstream out;
  dumpTokens(tokens, out);
  std::string output = out.str();

  // int at 1:1, x at 2:1, = at 2:3, 1 at 2:5, ; at 2:6
  EXPECT_NE(output.find("1:1 KW_INT 'int'"), std::string::npos);
  EXPECT_NE(output.find("2:1 IDENT 'x'"), std::string::npos);
  EXPECT_NE(output.find("2:3 ASSIGN '='"), std::string::npos);
  EXPECT_NE(output.find("2:5 NUMBER '1'"), std::string::npos);
  EXPECT_NE(output.find("2:6 SEMICOLON ';'"), std::string::npos);
}

TEST(DumpTokensTest, EmptySourceDumpsEOF) {
  DiagnosticEngine diag;
  Lexer lexer("", diag);
  auto tokens = lexer.tokenize();

  std::ostringstream out;
  dumpTokens(tokens, out);
  std::string output = out.str();

  EXPECT_NE(output.find("END_OF_FILE ''"), std::string::npos);
}

TEST(DumpTokensTest, MinusTokenFormat) {
  DiagnosticEngine diag;
  Lexer lexer("-5", diag);
  auto tokens = lexer.tokenize();

  std::ostringstream out;
  dumpTokens(tokens, out);
  std::string output = out.str();

  EXPECT_NE(output.find("1:1 MINUS '-'"), std::string::npos);
  EXPECT_NE(output.find("1:2 NUMBER '5'"), std::string::npos);
}

// ── dump-tokens stderr/stdout behavior (tested via component integration) ─

TEST(DumpTokensTest, DumpToStreamNotStdout) {
  // Verify dumpTokens writes to the provided stream, not stdout.
  DiagnosticEngine diag;
  Lexer lexer("int x;", diag);
  auto tokens = lexer.tokenize();

  std::ostringstream stderr_sim;
  dumpTokens(tokens, stderr_sim);

  // The stream should have content.
  EXPECT_FALSE(stderr_sim.str().empty());
  EXPECT_NE(stderr_sim.str().find("KW_INT"), std::string::npos);
}

// ── Lexer + DiagnosticEngine integration for dump-tokens mode ────────────

TEST(DumpTokensTest, SuccessfulTokenizeReturnsTrue) {
  DiagnosticEngine diag;
  Lexer lexer("int main() { return 0; }", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_TRUE(ok);
  EXPECT_FALSE(diag.hasErrors());
}

TEST(DumpTokensTest, FailedTokenizeReturnsFalse) {
  DiagnosticEngine diag;
  Lexer lexer("@invalid", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(diag.hasErrors());
}

TEST(DumpTokensTest, DiagnosticFormatLineColumn) {
  DiagnosticEngine diag;
  Lexer lexer("a\n  @", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);

  ASSERT_FALSE(diag.diagnostics().empty());
  auto& d = diag.diagnostics()[0];
  // @ is at line 2, column 3.
  EXPECT_EQ(d.location.line, 2u);
  EXPECT_EQ(d.location.column, 3u);
}

} // namespace toyc
