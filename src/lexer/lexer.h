#pragma once

#include "common/token.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace toyc {

class LexError : public std::runtime_error {
public:
    LexError(SourceLocation location, std::string message);

    [[nodiscard]] SourceLocation location() const noexcept { return location_; }

private:
    SourceLocation location_;
};

class Lexer {
public:
    explicit Lexer(std::string_view source);

    [[nodiscard]] std::vector<Token> tokenize();

private:
    [[nodiscard]] bool isAtEnd() const noexcept;
    [[nodiscard]] char peek(std::size_t offset = 0) const noexcept;
    [[nodiscard]] SourceLocation currentLocation() const noexcept;

    char advance() noexcept;
    bool match(char expected) noexcept;
    void addToken(TokenKind kind, std::string_view lexeme, SourceLocation location);
    void skipWhitespaceAndComments();
    void lexIdentifierOrKeyword();
    void lexIntegerLiteral();
    void lexOperatorOrDelimiter();

    std::string source_;
    std::size_t current_ = 0;
    int line_ = 1;
    int column_ = 1;
    std::vector<Token> tokens_;
};

[[nodiscard]] std::vector<Token> lex(std::string_view source);

} // namespace toyc
