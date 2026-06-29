#pragma once

#include "common/token.h"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace toyc {

class ParseError : public std::runtime_error {
public:
    explicit ParseError(std::string message) : std::runtime_error(std::move(message)) {}
};

class TokenStream {
public:
    explicit TokenStream(std::vector<Token> tokens) : tokens_(std::move(tokens)) {
        if (tokens_.empty() || tokens_.back().kind != TokenKind::Eof) {
            tokens_.push_back(Token{TokenKind::Eof, "", {}});
        }
    }

    [[nodiscard]] const Token& peek(std::size_t offset = 0) const {
        const std::size_t index = position_ + offset;
        if (index < tokens_.size()) {
            return tokens_[index];
        }
        return tokens_.back();
    }

    const Token& consume() {
        const Token& token = peek();
        if (position_ + 1 < tokens_.size()) {
            ++position_;
        }
        return token;
    }

    bool match(TokenKind kind) {
        if (peek().kind != kind) {
            return false;
        }
        consume();
        return true;
    }

    const Token& expect(TokenKind kind, std::string_view message) {
        if (peek().kind == kind) {
            return consume();
        }

        const Token& token = peek();
        std::ostringstream out;
        out << token.location.line << ':' << token.location.column << ": " << message
            << "; expected " << tokenKindToString(kind) << ", got "
            << tokenKindToString(token.kind) << " '" << token.lexeme << "'";
        throw ParseError(out.str());
    }

private:
    std::vector<Token> tokens_;
    std::size_t position_ = 0;
};

} // namespace toyc
