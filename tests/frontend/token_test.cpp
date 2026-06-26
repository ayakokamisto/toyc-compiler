/// Token tests — P1 verification of token utilities.

#include "toyc/frontend/token.h"

#include <gtest/gtest.h>

namespace toyc {

// ── tokenKindName covers all kinds ───────────────────────────────────────

TEST(TokenKindTest, AllKindsHaveNames) {
  // Verify every TokenKind has a non-empty name.
  TokenKind kinds[] = {
    TokenKind::KW_CONST, TokenKind::KW_INT, TokenKind::KW_VOID,
    TokenKind::KW_IF, TokenKind::KW_ELSE, TokenKind::KW_WHILE,
    TokenKind::KW_BREAK, TokenKind::KW_CONTINUE, TokenKind::KW_RETURN,
    TokenKind::IDENT, TokenKind::NUMBER,
    TokenKind::ASSIGN, TokenKind::OR, TokenKind::AND,
    TokenKind::LT, TokenKind::LE, TokenKind::GT, TokenKind::GE,
    TokenKind::EQ, TokenKind::NE,
    TokenKind::PLUS, TokenKind::MINUS, TokenKind::MUL, TokenKind::DIV,
    TokenKind::MOD, TokenKind::NOT,
    TokenKind::SEMICOLON, TokenKind::LBRACE, TokenKind::RBRACE,
    TokenKind::LPAREN, TokenKind::RPAREN, TokenKind::COMMA,
    TokenKind::END_OF_FILE, TokenKind::INVALID,
  };
  for (auto k : kinds) {
    auto name = tokenKindName(k);
    EXPECT_FALSE(name.empty()) << "TokenKind has empty name";
  }
}

TEST(TokenKindTest, NameMatchesEnumName) {
  EXPECT_EQ(tokenKindName(TokenKind::KW_CONST),    "KW_CONST");
  EXPECT_EQ(tokenKindName(TokenKind::KW_INT),      "KW_INT");
  EXPECT_EQ(tokenKindName(TokenKind::KW_VOID),     "KW_VOID");
  EXPECT_EQ(tokenKindName(TokenKind::KW_IF),       "KW_IF");
  EXPECT_EQ(tokenKindName(TokenKind::KW_ELSE),     "KW_ELSE");
  EXPECT_EQ(tokenKindName(TokenKind::KW_WHILE),    "KW_WHILE");
  EXPECT_EQ(tokenKindName(TokenKind::KW_BREAK),    "KW_BREAK");
  EXPECT_EQ(tokenKindName(TokenKind::KW_CONTINUE), "KW_CONTINUE");
  EXPECT_EQ(tokenKindName(TokenKind::KW_RETURN),   "KW_RETURN");
  EXPECT_EQ(tokenKindName(TokenKind::IDENT),        "IDENT");
  EXPECT_EQ(tokenKindName(TokenKind::NUMBER),       "NUMBER");
  EXPECT_EQ(tokenKindName(TokenKind::ASSIGN),       "ASSIGN");
  EXPECT_EQ(tokenKindName(TokenKind::OR),           "OR");
  EXPECT_EQ(tokenKindName(TokenKind::AND),          "AND");
  EXPECT_EQ(tokenKindName(TokenKind::LT),           "LT");
  EXPECT_EQ(tokenKindName(TokenKind::LE),           "LE");
  EXPECT_EQ(tokenKindName(TokenKind::GT),           "GT");
  EXPECT_EQ(tokenKindName(TokenKind::GE),           "GE");
  EXPECT_EQ(tokenKindName(TokenKind::EQ),           "EQ");
  EXPECT_EQ(tokenKindName(TokenKind::NE),           "NE");
  EXPECT_EQ(tokenKindName(TokenKind::PLUS),         "PLUS");
  EXPECT_EQ(tokenKindName(TokenKind::MINUS),        "MINUS");
  EXPECT_EQ(tokenKindName(TokenKind::MUL),          "MUL");
  EXPECT_EQ(tokenKindName(TokenKind::DIV),          "DIV");
  EXPECT_EQ(tokenKindName(TokenKind::MOD),          "MOD");
  EXPECT_EQ(tokenKindName(TokenKind::NOT),          "NOT");
  EXPECT_EQ(tokenKindName(TokenKind::SEMICOLON),    "SEMICOLON");
  EXPECT_EQ(tokenKindName(TokenKind::LBRACE),       "LBRACE");
  EXPECT_EQ(tokenKindName(TokenKind::RBRACE),       "RBRACE");
  EXPECT_EQ(tokenKindName(TokenKind::LPAREN),       "LPAREN");
  EXPECT_EQ(tokenKindName(TokenKind::RPAREN),       "RPAREN");
  EXPECT_EQ(tokenKindName(TokenKind::COMMA),        "COMMA");
  EXPECT_EQ(tokenKindName(TokenKind::END_OF_FILE),  "END_OF_FILE");
  EXPECT_EQ(tokenKindName(TokenKind::INVALID),      "INVALID");
}

TEST(TokenKindTest, AllKindsAreDistinct) {
  // Spot-check that no two kinds map to the same name.
  EXPECT_NE(tokenKindName(TokenKind::KW_INT), tokenKindName(TokenKind::IDENT));
  EXPECT_NE(tokenKindName(TokenKind::NUMBER), tokenKindName(TokenKind::INVALID));
  EXPECT_NE(tokenKindName(TokenKind::PLUS), tokenKindName(TokenKind::MINUS));
  EXPECT_NE(tokenKindName(TokenKind::ASSIGN), tokenKindName(TokenKind::EQ));
}

TEST(TokenKindTest, KeywordClassification) {
  EXPECT_TRUE(isKeyword(TokenKind::KW_CONST));
  EXPECT_TRUE(isKeyword(TokenKind::KW_INT));
  EXPECT_TRUE(isKeyword(TokenKind::KW_VOID));
  EXPECT_TRUE(isKeyword(TokenKind::KW_IF));
  EXPECT_TRUE(isKeyword(TokenKind::KW_ELSE));
  EXPECT_TRUE(isKeyword(TokenKind::KW_WHILE));
  EXPECT_TRUE(isKeyword(TokenKind::KW_BREAK));
  EXPECT_TRUE(isKeyword(TokenKind::KW_CONTINUE));
  EXPECT_TRUE(isKeyword(TokenKind::KW_RETURN));

  EXPECT_FALSE(isKeyword(TokenKind::IDENT));
  EXPECT_FALSE(isKeyword(TokenKind::NUMBER));
  EXPECT_FALSE(isKeyword(TokenKind::PLUS));
  EXPECT_FALSE(isKeyword(TokenKind::END_OF_FILE));
  EXPECT_FALSE(isKeyword(TokenKind::INVALID));
}

// ── Token construction ───────────────────────────────────────────────────

TEST(TokenTest, DefaultConstruction) {
  Token tok;
  EXPECT_EQ(tok.kind, TokenKind::INVALID);
  EXPECT_TRUE(tok.rawLexeme.empty());
}

TEST(TokenTest, ValueConstruction) {
  Token tok(TokenKind::NUMBER, "42", SourceRange{SourceLocation{0, 1, 1},
                                                  SourceLocation{2, 1, 3}});
  EXPECT_EQ(tok.kind, TokenKind::NUMBER);
  EXPECT_EQ(tok.rawLexeme, "42");
  EXPECT_EQ(tok.range.begin.line, 1u);
  EXPECT_EQ(tok.range.begin.column, 1u);
  EXPECT_EQ(tok.range.end.column, 3u);
}

} // namespace toyc
