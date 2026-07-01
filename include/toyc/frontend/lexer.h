#pragma once

#include "token.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// =============================================================================
// Lexer — tokenizer for the P1 Toy-C subset.
//
// Throws LexError on invalid input.
// =============================================================================

class LexError : public std::runtime_error {
public:
    int line;
    int column;
    LexError(int line, int column, const std::string& msg)
        : std::runtime_error(msg), line(line), column(column) {}
};

class Lexer {
public:
    explicit Lexer(std::string_view source);
    std::vector<Token> tokenize();

private:
    char peek() const;
    char advance();
    bool match(char c);
    bool is_at_end() const;
    void skip_whitespace_and_comments();
    Token lex_identifier_or_keyword();
    Token lex_integer_literal();
    Token lex_operator_or_delimiter();

    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    std::vector<Token> tokens_;
};
