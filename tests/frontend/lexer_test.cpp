#include <gtest/gtest.h>

#include "toyc/frontend/lexer.h"

#include <sstream>

// =============================================================================
// Lexer Tests
// =============================================================================

TEST(LexerTest, SimpleInteger) {
    auto tokens = Lexer("42").tokenize();
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(tokens[0].int_value, 42u);
    EXPECT_EQ(tokens[0].lexeme, "42");
}

TEST(LexerTest, KeywordInt) {
    auto tokens = Lexer("int").tokenize();
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwInt);
}

TEST(LexerTest, KeywordReturn) {
    auto tokens = Lexer("return").tokenize();
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwReturn);
}

TEST(LexerTest, FullProgram) {
    auto tokens = Lexer("int main() { return 42; }").tokenize();
    ASSERT_GE(tokens.size(), 8u);

    EXPECT_EQ(tokens[0].kind, TokenKind::KwInt);
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "main");
    EXPECT_EQ(tokens[2].kind, TokenKind::LParen);
    EXPECT_EQ(tokens[3].kind, TokenKind::RParen);
    EXPECT_EQ(tokens[4].kind, TokenKind::LBrace);
    EXPECT_EQ(tokens[5].kind, TokenKind::KwReturn);
    EXPECT_EQ(tokens[6].kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(tokens[6].int_value, 42u);
    EXPECT_EQ(tokens[7].kind, TokenKind::Semicolon);
    EXPECT_EQ(tokens[8].kind, TokenKind::RBrace);
    EXPECT_EQ(tokens[9].kind, TokenKind::Eof);
}

TEST(LexerTest, Operators) {
    auto tokens = Lexer("+ - * / % ! < > <= >= == != && ||").tokenize();
    ASSERT_GE(tokens.size(), 13u);
    EXPECT_EQ(tokens[0].kind, TokenKind::Plus);
    EXPECT_EQ(tokens[1].kind, TokenKind::Minus);
    EXPECT_EQ(tokens[2].kind, TokenKind::Star);
    EXPECT_EQ(tokens[3].kind, TokenKind::Slash);
    EXPECT_EQ(tokens[4].kind, TokenKind::Percent);
    EXPECT_EQ(tokens[5].kind, TokenKind::Bang);
    EXPECT_EQ(tokens[6].kind, TokenKind::Less);
    EXPECT_EQ(tokens[7].kind, TokenKind::Greater);
    EXPECT_EQ(tokens[8].kind, TokenKind::LessEqual);
    EXPECT_EQ(tokens[9].kind, TokenKind::GreaterEqual);
    EXPECT_EQ(tokens[10].kind, TokenKind::EqualEqual);
    EXPECT_EQ(tokens[11].kind, TokenKind::BangEqual);
    EXPECT_EQ(tokens[12].kind, TokenKind::AmpAmp);
    EXPECT_EQ(tokens[13].kind, TokenKind::PipePipe);
}

TEST(LexerTest, LineComment) {
    auto tokens = Lexer("// comment\n42").tokenize();
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(tokens[0].int_value, 42u);
}

TEST(LexerTest, BlockComment) {
    auto tokens = Lexer("/* comment */ 42").tokenize();
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(tokens[0].int_value, 42u);
}

TEST(LexerTest, MultiLineBlockComment) {
    auto tokens = Lexer("/* line1\nline2 */ 99").tokenize();
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(tokens[0].int_value, 99u);
}

TEST(LexerTest, ParenthesesAndBraces) {
    auto tokens = Lexer("( ) { }").tokenize();
    ASSERT_GE(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].kind, TokenKind::LParen);
    EXPECT_EQ(tokens[1].kind, TokenKind::RParen);
    EXPECT_EQ(tokens[2].kind, TokenKind::LBrace);
    EXPECT_EQ(tokens[3].kind, TokenKind::RBrace);
}

TEST(LexerTest, ErrorUnexpectedChar) {
    EXPECT_THROW(Lexer("@").tokenize(), LexError);
}

TEST(LexerTest, SingleEqNowValid) {
    // Single '=' is now a valid Equal token
    auto tokens = Lexer("=").tokenize();
    EXPECT_EQ(tokens[0].kind, TokenKind::Equal);
}
