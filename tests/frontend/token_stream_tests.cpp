#include "common/token_stream.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void fail(std::string_view message) {
    std::cerr << "token stream test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

std::vector<toyc::Token> sampleTokens() {
    return {
        {toyc::TokenKind::Identifier, "a", {1, 1}},
        {toyc::TokenKind::Plus, "+", {1, 3}},
        {toyc::TokenKind::IntegerLiteral, "2", {1, 5}},
        {toyc::TokenKind::Eof, "", {1, 6}},
    };
}

void testPeekAndLookahead() {
    toyc::TokenStream stream(sampleTokens());
    require(stream.peek().kind == toyc::TokenKind::Identifier, "peek current token");
    require(stream.peek(1).kind == toyc::TokenKind::Plus, "peek one token ahead");
    require(stream.peek(2).kind == toyc::TokenKind::IntegerLiteral,
            "peek two tokens ahead");
    require(stream.peek(100).kind == toyc::TokenKind::Eof, "peek beyond end returns EOF");
    require(stream.peek().kind == toyc::TokenKind::Identifier, "peek does not consume");
}

void testConsumeAtEof() {
    toyc::TokenStream stream(sampleTokens());
    (void)stream.consume();
    (void)stream.consume();
    (void)stream.consume();
    require(stream.consume().kind == toyc::TokenKind::Eof, "consume reaches EOF");
    require(stream.consume().kind == toyc::TokenKind::Eof, "consume remains at EOF");
    require(stream.peek().kind == toyc::TokenKind::Eof, "peek remains at EOF");
}

void testMatch() {
    toyc::TokenStream stream(sampleTokens());
    require(stream.match(toyc::TokenKind::Identifier), "successful match");
    require(stream.peek().kind == toyc::TokenKind::Plus, "successful match consumes");
    require(!stream.match(toyc::TokenKind::IntegerLiteral), "failed match");
    require(stream.peek().kind == toyc::TokenKind::Plus, "failed match preserves position");
}

void testExpectSuccess() {
    toyc::TokenStream stream(sampleTokens());
    const toyc::Token& token = stream.expect(toyc::TokenKind::Identifier, "expected name");
    require(token.lexeme == "a", "expect returns consumed token");
    require(stream.peek().kind == toyc::TokenKind::Plus, "expect success consumes");
}

void testExpectFailure() {
    toyc::TokenStream stream(sampleTokens());
    try {
        (void)stream.expect(toyc::TokenKind::KwInt, "expected declaration");
    } catch (const toyc::ParseError& error) {
        const std::string message = error.what();
        require(message.find("expected int") != std::string::npos,
                "expect failure reports expected token");
        require(message.find("got Identifier 'a'") != std::string::npos,
                "expect failure reports actual token");
        require(stream.peek().kind == toyc::TokenKind::Identifier,
                "expect failure preserves position");
        return;
    }
    fail("expect failure throws ParseError");
}

void testSyntheticEof() {
    toyc::TokenStream stream({{toyc::TokenKind::Identifier, "a", {1, 1}}});
    require(stream.peek(1).kind == toyc::TokenKind::Eof, "constructor appends EOF");
}

} // namespace

int main() {
    try {
        testPeekAndLookahead();
        testConsumeAtEof();
        testMatch();
        testExpectSuccess();
        testExpectFailure();
        testSyntheticEof();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
