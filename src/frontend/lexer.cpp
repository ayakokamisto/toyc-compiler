#include "toyc/frontend/lexer.h"

#include <cctype>
#include <sstream>

Lexer::Lexer(std::string_view source)
    : source_(source) {}

std::vector<Token> Lexer::tokenize() {
    skip_whitespace_and_comments();
    while (!is_at_end()) {
        char c = peek();
        if (std::isalpha(c) || c == '_') {
            tokens_.push_back(lex_identifier_or_keyword());
        } else if (std::isdigit(c)) {
            tokens_.push_back(lex_integer_literal());
        } else {
            tokens_.push_back(lex_operator_or_delimiter());
        }
        skip_whitespace_and_comments();
    }
    Token eof;
    eof.kind = TokenKind::Eof;
    eof.location = {line_, column_};
    tokens_.push_back(eof);
    return tokens_;
}

// --- Helpers ---

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[pos_];
}

char Lexer::advance() {
    char c = source_[pos_++];
    column_++;
    return c;
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[pos_] != expected) return false;
    pos_++;
    column_++;
    return true;
}

bool Lexer::is_at_end() const {
    return pos_ >= source_.size();
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            advance();
            line_++;
            column_ = 1;
        } else if (c == '/') {
            if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
                // Single-line comment: skip to end of line
                while (!is_at_end() && peek() != '\n') advance();
            } else if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '*') {
                // Block comment
                advance(); advance(); // skip /*
                while (!is_at_end()) {
                    if (peek() == '\n') {
                        advance();
                        line_++;
                        column_ = 1;
                    } else if (peek() == '*' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
                        advance(); advance(); // skip */
                        break;
                    } else {
                        advance();
                    }
                }
            } else {
                break; // not a comment, treat as operator
            }
        } else {
            break;
        }
    }
}

Token Lexer::lex_identifier_or_keyword() {
    Token tok;
    tok.location = {line_, column_};
    size_t start = pos_;
    while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }
    tok.lexeme = source_.substr(start, pos_ - start);

    if (tok.lexeme == "int") {
        tok.kind = TokenKind::KwInt;
    } else if (tok.lexeme == "return") {
        tok.kind = TokenKind::KwReturn;
    } else if (tok.lexeme == "if") {
        tok.kind = TokenKind::KwIf;
    } else if (tok.lexeme == "else") {
        tok.kind = TokenKind::KwElse;
    } else if (tok.lexeme == "while") {
        tok.kind = TokenKind::KwWhile;
    } else if (tok.lexeme == "break") {
        tok.kind = TokenKind::KwBreak;
    } else if (tok.lexeme == "void") {
        tok.kind = TokenKind::KwVoid;
    } else if (tok.lexeme == "continue") {
        tok.kind = TokenKind::KwContinue;
    } else {
        tok.kind = TokenKind::Identifier;
    }
    return tok;
}

Token Lexer::lex_integer_literal() {
    Token tok;
    tok.location = {line_, column_};
    size_t start = pos_;

    uint64_t value = 0;
    while (!is_at_end() && std::isdigit(peek())) {
        uint64_t digit = peek() - '0';
        // Check for overflow before multiplying
        if (value > (UINT64_MAX - digit) / 10) {
            std::ostringstream msg;
            msg << "integer literal exceeds uint64_t maximum at column " << column_;
            throw LexError(line_, column_, msg.str());
        }
        value = value * 10 + digit;
        advance();
    }

    tok.lexeme = source_.substr(start, pos_ - start);
    tok.kind = TokenKind::IntegerLiteral;
    tok.int_value = value;
    return tok;
}

Token Lexer::lex_operator_or_delimiter() {
    Token tok;
    tok.location = {line_, column_};
    char c = advance();

    switch (c) {
    case '(': tok.kind = TokenKind::LParen; break;
    case ')': tok.kind = TokenKind::RParen; break;
    case '{': tok.kind = TokenKind::LBrace; break;
    case '}': tok.kind = TokenKind::RBrace; break;
    case ';': tok.kind = TokenKind::Semicolon; break;
    case '+': tok.kind = TokenKind::Plus; break;
    case '-': tok.kind = TokenKind::Minus; break;
    case '*': tok.kind = TokenKind::Star; break;
    case '%': tok.kind = TokenKind::Percent; break;

    case '/':
        // Should not reach here if comments are pre-skipped.
        tok.kind = TokenKind::Slash;
        break;

    case '!':
        if (match('=')) tok.kind = TokenKind::BangEqual;
        else tok.kind = TokenKind::Bang;
        break;

    case '<':
        if (match('=')) tok.kind = TokenKind::LessEqual;
        else tok.kind = TokenKind::Less;
        break;

    case '>':
        if (match('=')) tok.kind = TokenKind::GreaterEqual;
        else tok.kind = TokenKind::Greater;
        break;

    case '=':
        if (match('=')) tok.kind = TokenKind::EqualEqual;
        else tok.kind = TokenKind::Equal;
        break;

    case '&':
        if (match('&')) tok.kind = TokenKind::AmpAmp;
        else {
            throw LexError(line_, column_ - 1,
                "unexpected character '&' (single '&' is not a valid operator)");
        }
        break;

    case '|':
        if (match('|')) tok.kind = TokenKind::PipePipe;
        else {
            throw LexError(line_, column_ - 1,
                "unexpected character '|' (single '|' is not a valid operator)");
        }
        break;

    case ',': tok.kind = TokenKind::Comma; break;
    default: {
        std::string c1(1, c);
        throw LexError(line_, column_ - 1,
            "unexpected character '" + c1 + "'");
    }
    }

    tok.lexeme = source_.substr(pos_ - 1 - (tok.kind == TokenKind::BangEqual ||
                                             tok.kind == TokenKind::LessEqual ||
                                             tok.kind == TokenKind::GreaterEqual ||
                                             tok.kind == TokenKind::EqualEqual ||
                                             tok.kind == TokenKind::AmpAmp ||
                                             tok.kind == TokenKind::PipePipe ? 1 : 0),
                                pos_ - (tok.location.column - 1));
    return tok;
}
