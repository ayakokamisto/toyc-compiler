#include "common/token.h"
#include "lexer/lexer.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct ExpectedToken {
    toyc::TokenKind kind;
    std::string_view lexeme;
};

void fail(std::string_view message) {
    std::cerr << "lexer test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void expectTokens(std::string_view source, const std::vector<ExpectedToken>& expected) {
    const std::vector<toyc::Token> tokens = toyc::lex(source);
    require(tokens.size() == expected.size() + 1, "unexpected token count");
    require(tokens.back().kind == toyc::TokenKind::Eof, "missing EOF token");

    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (tokens[i].kind != expected[i].kind || tokens[i].lexeme != expected[i].lexeme) {
            std::cerr << "token " << i << ": got " << toyc::tokenKindToString(tokens[i].kind)
                      << " '" << tokens[i].lexeme << "', expected "
                      << toyc::tokenKindToString(expected[i].kind) << " '" << expected[i].lexeme
                      << "'\n";
            fail("token mismatch");
        }
    }
}

void expectLexError(std::string_view source, std::string_view text) {
    try {
        (void)toyc::lex(source);
    } catch (const toyc::LexError& error) {
        const std::string message = error.what();
        require(message.find(text) != std::string::npos, "diagnostic text mismatch");
        return;
    }
    fail("expected lexical error");
}

void testKeywordsVersusIdentifiers() {
    expectTokens("int void const if else while break continue return integer ifx _while",
                 {{toyc::TokenKind::KwInt, "int"},
                  {toyc::TokenKind::KwVoid, "void"},
                  {toyc::TokenKind::KwConst, "const"},
                  {toyc::TokenKind::KwIf, "if"},
                  {toyc::TokenKind::KwElse, "else"},
                  {toyc::TokenKind::KwWhile, "while"},
                  {toyc::TokenKind::KwBreak, "break"},
                  {toyc::TokenKind::KwContinue, "continue"},
                  {toyc::TokenKind::KwReturn, "return"},
                  {toyc::TokenKind::Identifier, "integer"},
                  {toyc::TokenKind::Identifier, "ifx"},
                  {toyc::TokenKind::Identifier, "_while"}});
}

void testAllOperators() {
    expectTokens("+ - * / % ! < > = <= >= == != && ||",
                 {{toyc::TokenKind::Plus, "+"},
                  {toyc::TokenKind::Minus, "-"},
                  {toyc::TokenKind::Star, "*"},
                  {toyc::TokenKind::Slash, "/"},
                  {toyc::TokenKind::Percent, "%"},
                  {toyc::TokenKind::Bang, "!"},
                  {toyc::TokenKind::Less, "<"},
                  {toyc::TokenKind::Greater, ">"},
                  {toyc::TokenKind::Equal, "="},
                  {toyc::TokenKind::LessEqual, "<="},
                  {toyc::TokenKind::GreaterEqual, ">="},
                  {toyc::TokenKind::EqualEqual, "=="},
                  {toyc::TokenKind::BangEqual, "!="},
                  {toyc::TokenKind::AmpAmp, "&&"},
                  {toyc::TokenKind::PipePipe, "||"}});
}

void testNestedLookingCommentContent() {
    expectTokens("a /* outer /* nested-looking */ b */ c",
                 {{toyc::TokenKind::Identifier, "a"},
                  {toyc::TokenKind::Identifier, "b"},
                  {toyc::TokenKind::Star, "*"},
                  {toyc::TokenKind::Slash, "/"},
                  {toyc::TokenKind::Identifier, "c"}});
}

void testLineAndColumnTracking() {
    const std::vector<toyc::Token> tokens = toyc::lex("int\n  value\n\treturn");
    require(tokens[0].location.line == 1 && tokens[0].location.column == 1, "int location");
    require(tokens[1].location.line == 2 && tokens[1].location.column == 3, "identifier location");
    require(tokens[2].location.line == 3 && tokens[2].location.column == 2, "return location");
}

void testLineComments() {
    expectTokens("a // + - ignored\nb",
                 {{toyc::TokenKind::Identifier, "a"}, {toyc::TokenKind::Identifier, "b"}});
}

void testBlockComments() {
    expectTokens("a /* + - \n ignored */ b",
                 {{toyc::TokenKind::Identifier, "a"}, {toyc::TokenKind::Identifier, "b"}});
}

void testMinusTokenization() {
    expectTokens("a-1",
                 {{toyc::TokenKind::Identifier, "a"},
                  {toyc::TokenKind::Minus, "-"},
                  {toyc::TokenKind::IntegerLiteral, "1"}});
    expectTokens("a+-1",
                 {{toyc::TokenKind::Identifier, "a"},
                  {toyc::TokenKind::Plus, "+"},
                  {toyc::TokenKind::Minus, "-"},
                  {toyc::TokenKind::IntegerLiteral, "1"}});
    expectTokens("-42",
                 {{toyc::TokenKind::Minus, "-"}, {toyc::TokenKind::IntegerLiteral, "42"}});
}

void testFunctionCallPunctuation() {
    expectTokens("foo(a, 1);",
                 {{toyc::TokenKind::Identifier, "foo"},
                  {toyc::TokenKind::LParen, "("},
                  {toyc::TokenKind::Identifier, "a"},
                  {toyc::TokenKind::Comma, ","},
                  {toyc::TokenKind::IntegerLiteral, "1"},
                  {toyc::TokenKind::RParen, ")"},
                  {toyc::TokenKind::Semicolon, ";"}});
}

void testRejections() {
    expectLexError("012", "leading-zero");
    expectLexError("/* unfinished", "unterminated block comment");
}

} // namespace

int main() {
    try {
        testKeywordsVersusIdentifiers();
        testAllOperators();
        testNestedLookingCommentContent();
        testLineAndColumnTracking();
        testLineComments();
        testBlockComments();
        testMinusTokenization();
        testFunctionCallPunctuation();
        testRejections();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
