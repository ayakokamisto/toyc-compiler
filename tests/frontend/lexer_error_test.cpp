/// Lexer error tests — P1 verification of lexical error handling.

#include "toyc/frontend/lexer.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>

namespace toyc {

// ── Unterminated block comment ───────────────────────────────────────────

TEST(LexerErrorTest, UnterminatedBlockCommentAtEOF) {
  DiagnosticEngine diag;
  Lexer lexer("a /* never closed", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(diag.hasErrors());
  EXPECT_EQ(diag.errorCount(), 1u);
  // Should have IDENT(a) and EOF.
  ASSERT_GE(tokens.size(), 2u);
  EXPECT_EQ(tokens[0].kind, TokenKind::IDENT);
  EXPECT_EQ(tokens.back().kind, TokenKind::END_OF_FILE);
}

TEST(LexerErrorTest, UnterminatedBlockCommentDiagnosticMessage) {
  DiagnosticEngine diag;
  Lexer lexer("/* no end", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  ASSERT_FALSE(diag.diagnostics().empty());
  EXPECT_NE(diag.diagnostics()[0].message.find("unterminated"),
            std::string::npos);
}

// ── Invalid characters ───────────────────────────────────────────────────

TEST(LexerErrorTest, AtSign) {
  DiagnosticEngine diag;
  Lexer lexer("@", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  EXPECT_EQ(diag.errorCount(), 1u);
  ASSERT_EQ(tokens.size(), 2u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "@");
}

TEST(LexerErrorTest, DollarSign) {
  DiagnosticEngine diag;
  Lexer lexer("$", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "$");
}

TEST(LexerErrorTest, OpenBracket) {
  DiagnosticEngine diag;
  Lexer lexer("[", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "[");
}

TEST(LexerErrorTest, CloseBracket) {
  DiagnosticEngine diag;
  Lexer lexer("]", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "]");
}

TEST(LexerErrorTest, DoubleQuote) {
  DiagnosticEngine diag;
  Lexer lexer("\"", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
}

TEST(LexerErrorTest, SingleQuote) {
  DiagnosticEngine diag;
  Lexer lexer("'", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
}

TEST(LexerErrorTest, SinglePipe) {
  DiagnosticEngine diag;
  Lexer lexer("|", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "|");
}

TEST(LexerErrorTest, SingleAmpersand) {
  DiagnosticEngine diag;
  Lexer lexer("&", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "&");
}

TEST(LexerErrorTest, InvalidCharacterDiagnosticContainsLocation) {
  DiagnosticEngine diag;
  Lexer lexer("a\n @", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  ASSERT_FALSE(diag.diagnostics().empty());
  // Location should be line 2, column 2.
  EXPECT_EQ(diag.diagnostics()[0].location.line, 2u);
  EXPECT_EQ(diag.diagnostics()[0].location.column, 2u);
}

TEST(LexerErrorTest, InvalidCharacterDiagnosticMentionsCharacter) {
  DiagnosticEngine diag;
  Lexer lexer("@", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  ASSERT_FALSE(diag.diagnostics().empty());
  EXPECT_NE(diag.diagnostics()[0].message.find("@"),
            std::string::npos);
}

// ── Leading zero ─────────────────────────────────────────────────────────

TEST(LexerErrorTest, LeadingZeroSimple) {
  DiagnosticEngine diag;
  Lexer lexer("012", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "012");
}

TEST(LexerErrorTest, LeadingZeroDoubleZero) {
  DiagnosticEngine diag;
  Lexer lexer("00", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "00");
}

TEST(LexerErrorTest, LeadingZeroDiagnosticMentionsLeadingZero) {
  DiagnosticEngine diag;
  Lexer lexer("007", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  ASSERT_FALSE(diag.diagnostics().empty());
  auto& msg = diag.diagnostics()[0].message;
  // Should mention "leading zero" or similar.
  bool mentionsLeading = msg.find("leading") != std::string::npos;
  bool mentionsZero = msg.find("0") != std::string::npos;
  EXPECT_TRUE(mentionsLeading || mentionsZero)
      << "Diagnostic message: " << msg;
}

TEST(LexerErrorTest, LeadingZeroDoesNotHang) {
  // Ensure the lexer doesn't get stuck on leading-zero input.
  DiagnosticEngine diag;
  Lexer lexer("000000000000000000000000000000", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  // Should produce exactly one INVALID token and EOF.
  ASSERT_EQ(tokens.size(), 2u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[1].kind, TokenKind::END_OF_FILE);
}

// ── EOF always appended ──────────────────────────────────────────────────

TEST(LexerErrorTest, EOFAppendedAfterErrors) {
  DiagnosticEngine diag;
  Lexer lexer("@$#", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back().kind, TokenKind::END_OF_FILE);
}

TEST(LexerErrorTest, EOFAppendedAfterUnterminatedComment) {
  DiagnosticEngine diag;
  Lexer lexer("/* unterminated", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back().kind, TokenKind::END_OF_FILE);
}

// ── Errors don't prevent scanning of subsequent valid tokens ─────────────

TEST(LexerErrorTest, InvalidCharThenValidTokens) {
  DiagnosticEngine diag;
  Lexer lexer("@ a + b", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  // INVALID(@) IDENT(a) PLUS IDENT(b) EOF
  ASSERT_EQ(tokens.size(), 5u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[1].kind, TokenKind::IDENT);
  EXPECT_EQ(tokens[1].rawLexeme, "a");
  EXPECT_EQ(tokens[2].kind, TokenKind::PLUS);
  EXPECT_EQ(tokens[3].kind, TokenKind::IDENT);
  EXPECT_EQ(tokens[3].rawLexeme, "b");
  EXPECT_EQ(tokens[4].kind, TokenKind::END_OF_FILE);
}

TEST(LexerErrorTest, LeadingZeroThenValidTokens) {
  DiagnosticEngine diag;
  Lexer lexer("012 + 5", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  // INVALID(012) PLUS NUMBER(5) EOF
  ASSERT_EQ(tokens.size(), 4u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "012");
  EXPECT_EQ(tokens[1].kind, TokenKind::PLUS);
  EXPECT_EQ(tokens[2].kind, TokenKind::NUMBER);
  EXPECT_EQ(tokens[2].rawLexeme, "5");
}

// ── No false errors on valid input ───────────────────────────────────────

TEST(LexerErrorTest, ValidInputNoErrors) {
  DiagnosticEngine diag;
  Lexer lexer("int x = 42;", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_TRUE(ok);
  EXPECT_FALSE(diag.hasErrors());
  EXPECT_EQ(diag.errorCount(), 0u);
}

} // namespace toyc
