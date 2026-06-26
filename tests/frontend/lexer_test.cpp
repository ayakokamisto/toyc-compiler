/// Lexer tests — P1 comprehensive token scanning verification.

#include "toyc/frontend/lexer.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>

namespace toyc {

// Helper: tokenize a source string, assert no errors, return tokens.
static std::vector<Token> expectTokens(const std::string& source,
                                        DiagnosticEngine& diag) {
  Lexer lexer(source, diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_TRUE(ok) << "Lexer reported errors for: " << source;
  EXPECT_FALSE(diag.hasErrors()) << "Unexpected diagnostics for: " << source;
  return tokens;
}

static std::vector<Token> expectTokens(const std::string& source) {
  DiagnosticEngine diag;
  return expectTokens(source, diag);
}

// ── 1. All keywords ──────────────────────────────────────────────────────

TEST(LexerTest, AllKeywords) {
  auto toks = expectTokens(
      "const int void if else while break continue return");
  // Remove EOF for easier checking.
  ASSERT_FALSE(toks.empty());
  EXPECT_EQ(toks.back().kind, TokenKind::END_OF_FILE);
  toks.pop_back();

  ASSERT_EQ(toks.size(), 9u);
  EXPECT_EQ(toks[0].kind, TokenKind::KW_CONST);
  EXPECT_EQ(toks[0].rawLexeme, "const");
  EXPECT_EQ(toks[1].kind, TokenKind::KW_INT);
  EXPECT_EQ(toks[1].rawLexeme, "int");
  EXPECT_EQ(toks[2].kind, TokenKind::KW_VOID);
  EXPECT_EQ(toks[2].rawLexeme, "void");
  EXPECT_EQ(toks[3].kind, TokenKind::KW_IF);
  EXPECT_EQ(toks[3].rawLexeme, "if");
  EXPECT_EQ(toks[4].kind, TokenKind::KW_ELSE);
  EXPECT_EQ(toks[4].rawLexeme, "else");
  EXPECT_EQ(toks[5].kind, TokenKind::KW_WHILE);
  EXPECT_EQ(toks[5].rawLexeme, "while");
  EXPECT_EQ(toks[6].kind, TokenKind::KW_BREAK);
  EXPECT_EQ(toks[6].rawLexeme, "break");
  EXPECT_EQ(toks[7].kind, TokenKind::KW_CONTINUE);
  EXPECT_EQ(toks[7].rawLexeme, "continue");
  EXPECT_EQ(toks[8].kind, TokenKind::KW_RETURN);
  EXPECT_EQ(toks[8].rawLexeme, "return");
}

// ── 2. Identifier boundaries and keyword prefixes ────────────────────────

TEST(LexerTest, IdentifierBasic) {
  auto toks = expectTokens("foo _bar baz123 _");
  ASSERT_EQ(toks.size(), 5u); // 4 idents + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[0].rawLexeme, "foo");
  EXPECT_EQ(toks[1].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[1].rawLexeme, "_bar");
  EXPECT_EQ(toks[2].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[2].rawLexeme, "baz123");
  EXPECT_EQ(toks[3].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[3].rawLexeme, "_");
}

TEST(LexerTest, KeywordPrefixesAreIdentifiers) {
  auto toks = expectTokens("ifx return1 _const intx");
  ASSERT_EQ(toks.size(), 5u); // 4 idents + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[0].rawLexeme, "ifx");
  EXPECT_EQ(toks[1].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[1].rawLexeme, "return1");
  EXPECT_EQ(toks[2].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[2].rawLexeme, "_const");
  EXPECT_EQ(toks[3].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[3].rawLexeme, "intx");
}

// ── 3. Normal integers ───────────────────────────────────────────────────

TEST(LexerTest, NumberZero) {
  auto toks = expectTokens("0");
  ASSERT_EQ(toks.size(), 2u); // NUMBER + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[0].rawLexeme, "0");
}

TEST(LexerTest, NumberPositive) {
  auto toks = expectTokens("42 123 2147483648");
  ASSERT_EQ(toks.size(), 4u); // 3 numbers + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[0].rawLexeme, "42");
  EXPECT_EQ(toks[1].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[1].rawLexeme, "123");
  EXPECT_EQ(toks[2].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[2].rawLexeme, "2147483648");
}

// ── 4. Negative sign tokenization ────────────────────────────────────────

TEST(LexerTest, ReturnNegativeFive) {
  auto toks = expectTokens("return -5;");
  ASSERT_EQ(toks.size(), 5u); // KW_RETURN MINUS NUMBER SEMICOLON EOF
  EXPECT_EQ(toks[0].kind, TokenKind::KW_RETURN);
  EXPECT_EQ(toks[1].kind, TokenKind::MINUS);
  EXPECT_EQ(toks[1].rawLexeme, "-");
  EXPECT_EQ(toks[2].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[2].rawLexeme, "5");
  EXPECT_EQ(toks[3].kind, TokenKind::SEMICOLON);
}

TEST(LexerTest, IdentifierMinusNumber) {
  auto toks = expectTokens("a-5");
  ASSERT_EQ(toks.size(), 4u); // IDENT MINUS NUMBER EOF
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[0].rawLexeme, "a");
  EXPECT_EQ(toks[1].kind, TokenKind::MINUS);
  EXPECT_EQ(toks[2].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[2].rawLexeme, "5");
}

TEST(LexerTest, ParenMinusOne) {
  auto toks = expectTokens("(-1)");
  ASSERT_EQ(toks.size(), 5u); // LPAREN MINUS NUMBER RPAREN EOF
  EXPECT_EQ(toks[0].kind, TokenKind::LPAREN);
  EXPECT_EQ(toks[1].kind, TokenKind::MINUS);
  EXPECT_EQ(toks[2].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[2].rawLexeme, "1");
  EXPECT_EQ(toks[3].kind, TokenKind::RPAREN);
}

// ── 5. All single-char operators and delimiters ──────────────────────────

TEST(LexerTest, SingleCharOperators) {
  auto toks = expectTokens("+ - * / % !");
  ASSERT_EQ(toks.size(), 7u); // 6 ops + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::PLUS);
  EXPECT_EQ(toks[1].kind, TokenKind::MINUS);
  EXPECT_EQ(toks[2].kind, TokenKind::MUL);
  EXPECT_EQ(toks[3].kind, TokenKind::DIV);
  EXPECT_EQ(toks[4].kind, TokenKind::MOD);
  EXPECT_EQ(toks[5].kind, TokenKind::NOT);
}

TEST(LexerTest, Delimiters) {
  auto toks = expectTokens("; { } ( ) ,");
  ASSERT_EQ(toks.size(), 7u); // 6 delims + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::SEMICOLON);
  EXPECT_EQ(toks[1].kind, TokenKind::LBRACE);
  EXPECT_EQ(toks[2].kind, TokenKind::RBRACE);
  EXPECT_EQ(toks[3].kind, TokenKind::LPAREN);
  EXPECT_EQ(toks[4].kind, TokenKind::RPAREN);
  EXPECT_EQ(toks[5].kind, TokenKind::COMMA);
}

TEST(LexerTest, AssignOperator) {
  auto toks = expectTokens("=");
  ASSERT_EQ(toks.size(), 2u);
  EXPECT_EQ(toks[0].kind, TokenKind::ASSIGN);
  EXPECT_EQ(toks[0].rawLexeme, "=");
}

// ── 6. All two-char operators ────────────────────────────────────────────

TEST(LexerTest, TwoCharOperators) {
  auto toks = expectTokens("|| && <= >= == !=");
  ASSERT_EQ(toks.size(), 7u); // 6 ops + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::OR);
  EXPECT_EQ(toks[0].rawLexeme, "||");
  EXPECT_EQ(toks[1].kind, TokenKind::AND);
  EXPECT_EQ(toks[1].rawLexeme, "&&");
  EXPECT_EQ(toks[2].kind, TokenKind::LE);
  EXPECT_EQ(toks[2].rawLexeme, "<=");
  EXPECT_EQ(toks[3].kind, TokenKind::GE);
  EXPECT_EQ(toks[3].rawLexeme, ">=");
  EXPECT_EQ(toks[4].kind, TokenKind::EQ);
  EXPECT_EQ(toks[4].rawLexeme, "==");
  EXPECT_EQ(toks[5].kind, TokenKind::NE);
  EXPECT_EQ(toks[5].rawLexeme, "!=");
}

// ── 7. Two-char vs single-char boundary ──────────────────────────────────

TEST(LexerTest, MixedComparisonOperators) {
  auto toks = expectTokens("! != = == < <= > >= ");
  // ! != = == < <= > >=  → 8 ops
  ASSERT_EQ(toks.size(), 9u); // 8 ops + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::NOT);
  EXPECT_EQ(toks[0].rawLexeme, "!");
  EXPECT_EQ(toks[1].kind, TokenKind::NE);
  EXPECT_EQ(toks[1].rawLexeme, "!=");
  EXPECT_EQ(toks[2].kind, TokenKind::ASSIGN);
  EXPECT_EQ(toks[2].rawLexeme, "=");
  EXPECT_EQ(toks[3].kind, TokenKind::EQ);
  EXPECT_EQ(toks[3].rawLexeme, "==");
  EXPECT_EQ(toks[4].kind, TokenKind::LT);
  EXPECT_EQ(toks[4].rawLexeme, "<");
  EXPECT_EQ(toks[5].kind, TokenKind::LE);
  EXPECT_EQ(toks[5].rawLexeme, "<=");
  EXPECT_EQ(toks[6].kind, TokenKind::GT);
  EXPECT_EQ(toks[6].rawLexeme, ">");
  EXPECT_EQ(toks[7].kind, TokenKind::GE);
  EXPECT_EQ(toks[7].rawLexeme, ">=");
}

// ── 8. Whitespace and source locations ───────────────────────────────────

TEST(LexerTest, SourceLocationLF) {
  // "a\nb" — b should be on line 2, column 1.
  auto toks = expectTokens("a\nb");
  ASSERT_EQ(toks.size(), 3u); // IDENT(a) IDENT(b) EOF
  EXPECT_EQ(toks[0].range.begin.line, 1u);
  EXPECT_EQ(toks[0].range.begin.column, 1u);
  EXPECT_EQ(toks[1].range.begin.line, 2u);
  EXPECT_EQ(toks[1].range.begin.column, 1u);
}

TEST(LexerTest, SourceLocationCRLF) {
  // "a\r\nb" — \r\n treated as one newline.
  auto toks = expectTokens("a\r\nb");
  ASSERT_EQ(toks.size(), 3u);
  EXPECT_EQ(toks[0].range.begin.line, 1u);
  EXPECT_EQ(toks[0].range.begin.column, 1u);
  EXPECT_EQ(toks[1].range.begin.line, 2u);
  EXPECT_EQ(toks[1].range.begin.column, 1u);
}

TEST(LexerTest, SourceLocationCR) {
  // "a\rb" — \r treated as newline.
  auto toks = expectTokens("a\rb");
  ASSERT_EQ(toks.size(), 3u);
  EXPECT_EQ(toks[0].range.begin.line, 1u);
  EXPECT_EQ(toks[0].range.begin.column, 1u);
  EXPECT_EQ(toks[1].range.begin.line, 2u);
  EXPECT_EQ(toks[1].range.begin.column, 1u);
}

TEST(LexerTest, MultiLineSourceLocations) {
  auto toks = expectTokens("x =\n  42;");
  // x = \n 42 ;
  ASSERT_EQ(toks.size(), 5u);
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[0].range.begin.line, 1u);
  EXPECT_EQ(toks[0].range.begin.column, 1u);
  EXPECT_EQ(toks[1].kind, TokenKind::ASSIGN);
  EXPECT_EQ(toks[1].range.begin.line, 1u);
  EXPECT_EQ(toks[1].range.begin.column, 3u);
  EXPECT_EQ(toks[2].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[2].range.begin.line, 2u);
  EXPECT_EQ(toks[2].range.begin.column, 3u);
  EXPECT_EQ(toks[3].kind, TokenKind::SEMICOLON);
  EXPECT_EQ(toks[3].range.begin.line, 2u);
  EXPECT_EQ(toks[3].range.begin.column, 5u);
}

TEST(LexerTest, TabAndSpaceWhitespace) {
  auto toks = expectTokens("  \t a \t ");
  ASSERT_EQ(toks.size(), 2u); // IDENT(a) EOF
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[0].rawLexeme, "a");
  EXPECT_EQ(toks[0].range.begin.column, 5u); // after "  \t "
}

// ── 9. Single-line comments ─────────────────────────────────────────────

TEST(LexerTest, SingleLineComment) {
  auto toks = expectTokens("a // this is a comment\nb");
  ASSERT_EQ(toks.size(), 3u); // IDENT(a) IDENT(b) EOF
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[0].rawLexeme, "a");
  EXPECT_EQ(toks[1].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[1].rawLexeme, "b");
  EXPECT_EQ(toks[1].range.begin.line, 2u);
}

TEST(LexerTest, SingleLineCommentAtEOF) {
  auto toks = expectTokens("a // comment at end");
  ASSERT_EQ(toks.size(), 2u); // IDENT(a) EOF
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
}

TEST(LexerTest, CommentDoesNotGenerateToken) {
  // Verify // and /* */ don't appear in token stream.
  auto toks = expectTokens("/* block */ a // line\n b");
  // a b EOF
  ASSERT_EQ(toks.size(), 3u);
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[0].rawLexeme, "a");
  EXPECT_EQ(toks[1].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[1].rawLexeme, "b");
}

// ── 10. Multi-line and cross-line comments ───────────────────────────────

TEST(LexerTest, BlockCommentMultiLine) {
  auto toks = expectTokens("a /* comment\nspanning\nlines */ b");
  ASSERT_EQ(toks.size(), 3u); // IDENT(a) IDENT(b) EOF
  EXPECT_EQ(toks[0].rawLexeme, "a");
  EXPECT_EQ(toks[1].rawLexeme, "b");
  EXPECT_EQ(toks[1].range.begin.line, 3u);
}

TEST(LexerTest, BlockCommentNestedStars) {
  // /* * * */ should be valid.
  auto toks = expectTokens("a /* * * */ b");
  ASSERT_EQ(toks.size(), 3u);
  EXPECT_EQ(toks[0].rawLexeme, "a");
  EXPECT_EQ(toks[1].rawLexeme, "b");
}

// ── 11. Unterminated block comment ───────────────────────────────────────

TEST(LexerTest, UnterminatedBlockComment) {
  DiagnosticEngine diag;
  Lexer lexer("a /* unterminated", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(diag.hasErrors());
  EXPECT_GT(diag.errorCount(), 0u);
  // Should still have EOF token.
  EXPECT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back().kind, TokenKind::END_OF_FILE);
}

// ── 12. Invalid characters ───────────────────────────────────────────────

TEST(LexerTest, InvalidCharacterAt) {
  DiagnosticEngine diag;
  Lexer lexer("a @ b", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(diag.hasErrors());
  // Should have IDENT(a) INVALID(@) IDENT(b) EOF
  ASSERT_EQ(tokens.size(), 4u);
  EXPECT_EQ(tokens[0].kind, TokenKind::IDENT);
  EXPECT_EQ(tokens[0].rawLexeme, "a");
  EXPECT_EQ(tokens[1].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[1].rawLexeme, "@");
  EXPECT_EQ(tokens[2].kind, TokenKind::IDENT);
  EXPECT_EQ(tokens[2].rawLexeme, "b");
}

TEST(LexerTest, InvalidCharacterDollar) {
  DiagnosticEngine diag;
  Lexer lexer("$", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "$");
}

TEST(LexerTest, InvalidCharacterBrackets) {
  DiagnosticEngine diag;
  Lexer lexer("[ ]", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  // INVALID([) INVALID(]) EOF
  ASSERT_EQ(tokens.size(), 3u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "[");
  EXPECT_EQ(tokens[1].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[1].rawLexeme, "]");
}

TEST(LexerTest, InvalidCharacterQuotes) {
  DiagnosticEngine diag;
  Lexer lexer("\" '", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_EQ(tokens.size(), 3u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[1].kind, TokenKind::INVALID);
}

TEST(LexerTest, InvalidCharacterPipe) {
  // Single | is invalid (not ||).
  DiagnosticEngine diag;
  Lexer lexer("|", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
}

TEST(LexerTest, InvalidCharacterAmpersand) {
  // Single & is invalid (not &&).
  DiagnosticEngine diag;
  Lexer lexer("&", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
}

TEST(LexerTest, MultipleInvalidCharacters) {
  DiagnosticEngine diag;
  Lexer lexer("@$#", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(diag.hasErrors());
  EXPECT_EQ(diag.errorCount(), 3u);
  ASSERT_EQ(tokens.size(), 4u); // 3 INVALID + EOF
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[1].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[2].kind, TokenKind::INVALID);
}

// ── 13. Leading-zero integers ────────────────────────────────────────────

TEST(LexerTest, LeadingZeroSingle) {
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

TEST(LexerTest, LeadingZeroDouble) {
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

TEST(LexerTest, LeadingZeroLong) {
  DiagnosticEngine diag;
  Lexer lexer("00123", diag);
  std::vector<Token> tokens;
  bool ok = lexer.tokenize(tokens);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(diag.hasErrors());
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::INVALID);
  EXPECT_EQ(tokens[0].rawLexeme, "00123");
}

TEST(LexerTest, ZeroAloneIsValid) {
  auto toks = expectTokens("0");
  ASSERT_EQ(toks.size(), 2u);
  EXPECT_EQ(toks[0].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[0].rawLexeme, "0");
}

// ── 14. EOF token ────────────────────────────────────────────────────────

TEST(LexerTest, EmptyInputProducesEOF) {
  auto toks = expectTokens("");
  ASSERT_EQ(toks.size(), 1u);
  EXPECT_EQ(toks[0].kind, TokenKind::END_OF_FILE);
  EXPECT_TRUE(toks[0].rawLexeme.empty());
}

TEST(LexerTest, EOFLocationAtEnd) {
  auto toks = expectTokens("a");
  ASSERT_EQ(toks.size(), 2u);
  EXPECT_EQ(toks[1].kind, TokenKind::END_OF_FILE);
  // EOF should be at position after 'a'.
  EXPECT_EQ(toks[1].range.begin.offset, 1u);
}

TEST(LexerTest, EOFAfterNewline) {
  auto toks = expectTokens("a\n");
  ASSERT_EQ(toks.size(), 2u);
  EXPECT_EQ(toks[1].kind, TokenKind::END_OF_FILE);
  EXPECT_EQ(toks[1].range.begin.line, 2u);
}

// ── Additional comprehensive tests ───────────────────────────────────────

TEST(LexerTest, FullMinimalMain) {
  auto toks = expectTokens("int main() {\n  return 0;\n}\n");
  // int main ( ) { return 0 ; } EOF
  ASSERT_EQ(toks.size(), 10u);
  EXPECT_EQ(toks[0].kind, TokenKind::KW_INT);
  EXPECT_EQ(toks[1].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[1].rawLexeme, "main");
  EXPECT_EQ(toks[2].kind, TokenKind::LPAREN);
  EXPECT_EQ(toks[3].kind, TokenKind::RPAREN);
  EXPECT_EQ(toks[4].kind, TokenKind::LBRACE);
  EXPECT_EQ(toks[5].kind, TokenKind::KW_RETURN);
  EXPECT_EQ(toks[6].kind, TokenKind::NUMBER);
  EXPECT_EQ(toks[6].rawLexeme, "0");
  EXPECT_EQ(toks[7].kind, TokenKind::SEMICOLON);
  EXPECT_EQ(toks[8].kind, TokenKind::RBRACE);
  EXPECT_EQ(toks[9].kind, TokenKind::END_OF_FILE);
}

TEST(LexerTest, ComplexExpression) {
  auto toks = expectTokens("a<=b && c!=d || e>=f == g");
  ASSERT_EQ(toks.size(), 14u); // 13 tokens + EOF
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);   // a
  EXPECT_EQ(toks[1].kind, TokenKind::LE);       // <=
  EXPECT_EQ(toks[2].kind, TokenKind::IDENT);   // b
  EXPECT_EQ(toks[3].kind, TokenKind::AND);      // &&
  EXPECT_EQ(toks[4].kind, TokenKind::IDENT);   // c
  EXPECT_EQ(toks[5].kind, TokenKind::NE);       // !=
  EXPECT_EQ(toks[6].kind, TokenKind::IDENT);   // d
  EXPECT_EQ(toks[7].kind, TokenKind::OR);       // ||
  EXPECT_EQ(toks[8].kind, TokenKind::IDENT);   // e
  EXPECT_EQ(toks[9].kind, TokenKind::GE);       // >=
  EXPECT_EQ(toks[10].kind, TokenKind::IDENT);  // f
  EXPECT_EQ(toks[11].kind, TokenKind::EQ);      // ==
  EXPECT_EQ(toks[12].kind, TokenKind::IDENT);  // g
}

TEST(LexerTest, LexemeRangesAreCorrect) {
  auto toks = expectTokens("ab + cd");
  ASSERT_EQ(toks.size(), 4u);
  // "ab" at 1:1..1:3
  EXPECT_EQ(toks[0].range.begin.line, 1u);
  EXPECT_EQ(toks[0].range.begin.column, 1u);
  EXPECT_EQ(toks[0].range.end.column, 3u);
  // "+" at 1:4..1:5
  EXPECT_EQ(toks[1].range.begin.column, 4u);
  EXPECT_EQ(toks[1].range.end.column, 5u);
  // "cd" at 1:6..1:8
  EXPECT_EQ(toks[2].range.begin.column, 6u);
  EXPECT_EQ(toks[2].range.end.column, 8u);
}

TEST(LexerTest, LexerHasErrorInitiallyFalse) {
  DiagnosticEngine diag;
  Lexer lexer("hello", diag);
  EXPECT_FALSE(lexer.hasError());
}

TEST(LexerTest, LexerHasErrorAfterInvalidChar) {
  DiagnosticEngine diag;
  Lexer lexer("@", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(lexer.hasError());
}

TEST(LexerTest, LexerHasErrorAfterLeadingZero) {
  DiagnosticEngine diag;
  Lexer lexer("007", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(lexer.hasError());
}

TEST(LexerTest, LexerHasErrorAfterUnterminatedComment) {
  DiagnosticEngine diag;
  Lexer lexer("/* unterminated", diag);
  std::vector<Token> tokens;
  lexer.tokenize(tokens);
  EXPECT_TRUE(lexer.hasError());
}

TEST(LexerTest, DivisionNotComment) {
  auto toks = expectTokens("a / b");
  ASSERT_EQ(toks.size(), 4u);
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[1].kind, TokenKind::DIV);
  EXPECT_EQ(toks[1].rawLexeme, "/");
  EXPECT_EQ(toks[2].kind, TokenKind::IDENT);
}

TEST(LexerTest, StarNotCommentStart) {
  auto toks = expectTokens("a * b");
  ASSERT_EQ(toks.size(), 4u);
  EXPECT_EQ(toks[0].kind, TokenKind::IDENT);
  EXPECT_EQ(toks[1].kind, TokenKind::MUL);
  EXPECT_EQ(toks[1].rawLexeme, "*");
  EXPECT_EQ(toks[2].kind, TokenKind::IDENT);
}

} // namespace toyc
