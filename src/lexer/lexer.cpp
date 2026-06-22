#include "lexer/lexer.h"

#include <cctype>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace toyc {
namespace {

bool isIdentifierStart(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return ch == '_' || std::isalpha(value) != 0;
}

bool isIdentifierContinue(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return ch == '_' || std::isalnum(value) != 0;
}

bool isDigit(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return std::isdigit(value) != 0;
}

TokenKind keywordKind(std::string_view text) {
    static const std::unordered_map<std::string_view, TokenKind> keywords = {
        {"int", TokenKind::KwInt},       {"void", TokenKind::KwVoid},
        {"const", TokenKind::KwConst},   {"if", TokenKind::KwIf},
        {"else", TokenKind::KwElse},     {"while", TokenKind::KwWhile},
        {"break", TokenKind::KwBreak},   {"continue", TokenKind::KwContinue},
        {"return", TokenKind::KwReturn},
    };

    const auto found = keywords.find(text);
    if (found == keywords.end()) {
        return TokenKind::Identifier;
    }
    return found->second;
}

std::string formatLexError(SourceLocation location, std::string_view message) {
    std::ostringstream out;
    out << location.line << ':' << location.column << ": lexical error: " << message;
    return out.str();
}

} // namespace

LexError::LexError(SourceLocation location, std::string message)
    : std::runtime_error(formatLexError(location, message)), location_(location) {}

Lexer::Lexer(std::string_view source) : source_(source) {}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        skipWhitespaceAndComments();
        if (isAtEnd()) {
            break;
        }

        const char ch = peek();
        if (isIdentifierStart(ch)) {
            lexIdentifierOrKeyword();
        } else if (isDigit(ch)) {
            lexIntegerLiteral();
        } else {
            lexOperatorOrDelimiter();
        }
    }

    tokens_.push_back(Token{TokenKind::Eof, "", currentLocation()});
    return tokens_;
}

bool Lexer::isAtEnd() const noexcept {
    return current_ >= source_.size();
}

char Lexer::peek(std::size_t offset) const noexcept {
    const std::size_t index = current_ + offset;
    if (index >= source_.size()) {
        return '\0';
    }
    return source_[index];
}

SourceLocation Lexer::currentLocation() const noexcept {
    return SourceLocation{line_, column_};
}

char Lexer::advance() noexcept {
    const char ch = source_[current_++];
    if (ch == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return ch;
}

bool Lexer::match(char expected) noexcept {
    if (isAtEnd() || peek() != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::addToken(TokenKind kind, std::string_view lexeme, SourceLocation location) {
    tokens_.push_back(Token{kind, std::string(lexeme), location});
}

void Lexer::skipWhitespaceAndComments() {
    bool consumed = true;
    while (consumed && !isAtEnd()) {
        consumed = false;

        while (!isAtEnd()) {
            const char ch = peek();
            if (ch == ' ' || ch == '\r' || ch == '\t' || ch == '\n') {
                advance();
                consumed = true;
            } else {
                break;
            }
        }

        if (peek() == '/' && peek(1) == '/') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            consumed = true;
            continue;
        }

        if (peek() == '/' && peek(1) == '*') {
            const SourceLocation commentStart = currentLocation();
            advance();
            advance();
            while (!isAtEnd()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance();
                    advance();
                    consumed = true;
                    break;
                }
                advance();
            }
            if (!consumed) {
                throw LexError(commentStart, "unterminated block comment");
            }
        }
    }
}

void Lexer::lexIdentifierOrKeyword() {
    const SourceLocation startLocation = currentLocation();
    const std::size_t start = current_;
    advance();
    while (isIdentifierContinue(peek())) {
        advance();
    }

    const std::string_view text(source_.data() + start, current_ - start);
    addToken(keywordKind(text), text, startLocation);
}

void Lexer::lexIntegerLiteral() {
    const SourceLocation startLocation = currentLocation();
    const std::size_t start = current_;
    advance();
    while (isDigit(peek())) {
        advance();
    }

    const std::string_view text(source_.data() + start, current_ - start);
    if (text.size() > 1 && text.front() == '0') {
        throw LexError(startLocation, "leading-zero decimal integer literal '" + std::string(text) + "'");
    }

    addToken(TokenKind::IntegerLiteral, text, startLocation);
}

void Lexer::lexOperatorOrDelimiter() {
    const SourceLocation startLocation = currentLocation();
    const std::size_t start = current_;
    const char ch = advance();

    switch (ch) {
    case '(':
        addToken(TokenKind::LParen, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case ')':
        addToken(TokenKind::RParen, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case '{':
        addToken(TokenKind::LBrace, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case '}':
        addToken(TokenKind::RBrace, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case ',':
        addToken(TokenKind::Comma, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case ';':
        addToken(TokenKind::Semicolon, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case '+':
        addToken(TokenKind::Plus, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case '-':
        addToken(TokenKind::Minus, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case '*':
        addToken(TokenKind::Star, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case '/':
        addToken(TokenKind::Slash, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case '%':
        addToken(TokenKind::Percent, std::string_view(source_.data() + start, 1), startLocation);
        return;
    case '!':
        if (match('=')) {
            addToken(TokenKind::BangEqual, std::string_view(source_.data() + start, 2), startLocation);
        } else {
            addToken(TokenKind::Bang, std::string_view(source_.data() + start, 1), startLocation);
        }
        return;
    case '<':
        if (match('=')) {
            addToken(TokenKind::LessEqual, std::string_view(source_.data() + start, 2), startLocation);
        } else {
            addToken(TokenKind::Less, std::string_view(source_.data() + start, 1), startLocation);
        }
        return;
    case '>':
        if (match('=')) {
            addToken(TokenKind::GreaterEqual, std::string_view(source_.data() + start, 2), startLocation);
        } else {
            addToken(TokenKind::Greater, std::string_view(source_.data() + start, 1), startLocation);
        }
        return;
    case '=':
        if (match('=')) {
            addToken(TokenKind::EqualEqual, std::string_view(source_.data() + start, 2), startLocation);
        } else {
            addToken(TokenKind::Equal, std::string_view(source_.data() + start, 1), startLocation);
        }
        return;
    case '&':
        if (match('&')) {
            addToken(TokenKind::AmpAmp, std::string_view(source_.data() + start, 2), startLocation);
            return;
        }
        break;
    case '|':
        if (match('|')) {
            addToken(TokenKind::PipePipe, std::string_view(source_.data() + start, 2), startLocation);
            return;
        }
        break;
    default:
        break;
    }

    throw LexError(startLocation, "unknown character '" + std::string(1, ch) + "'");
}

std::vector<Token> lex(std::string_view source) {
    return Lexer(source).tokenize();
}

} // namespace toyc
