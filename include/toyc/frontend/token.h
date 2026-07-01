#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// =============================================================================
// Token — lexer token for the P1 Toy-C subset.
// =============================================================================

enum class TokenKind : uint8_t {
    // Special
    Eof,
    Identifier,
    IntegerLiteral,

    // Keywords
    KwInt,
    KwReturn,
    KwIf,
    KwElse,
    KwWhile,
    KwBreak,
    KwContinue,
    KwVoid,

    // Delimiters
    LParen,
    RParen,
    LBrace,
    RBrace,
    Semicolon,
    Comma,

    Equal,
    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Bang,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    EqualEqual,
    BangEqual,
    AmpAmp,
    PipePipe,
};

struct SourceLocation {
    int line = 1;
    int column = 1;
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string lexeme;
    SourceLocation location;
    uint64_t int_value = 0;  // for IntegerLiteral
};

constexpr std::string_view token_kind_name(TokenKind k) {
    using K = TokenKind;
    switch (k) {
    case K::Eof:             return "EOF";
    case K::Identifier:      return "Identifier";
    case K::IntegerLiteral:  return "IntegerLiteral";
    case K::KwInt:           return "KwInt";
    case K::KwReturn:        return "KwReturn";
    case K::LParen:          return "LParen";
    case K::RParen:          return "RParen";
    case K::LBrace:          return "LBrace";
    case K::RBrace:          return "RBrace";
    case K::Semicolon:       return "Semicolon";
    case K::Plus:            return "Plus";
    case K::Minus:           return "Minus";
    case K::Star:            return "Star";
    case K::Slash:           return "Slash";
    case K::Percent:         return "Percent";
    case K::Bang:            return "Bang";
    case K::Equal:          return "Equal";
    case K::KwIf:            return "KwIf";
    case K::KwElse:          return "KwElse";
    case K::KwWhile:         return "KwWhile";
    case K::KwBreak:         return "KwBreak";
    case K::KwContinue:      return "KwContinue";
    case K::Less:            return "Less";
    case K::Greater:         return "Greater";
    case K::LessEqual:       return "LessEqual";
    case K::GreaterEqual:    return "GreaterEqual";
    case K::EqualEqual:      return "EqualEqual";
    case K::BangEqual:       return "BangEqual";
    case K::AmpAmp:          return "AmpAmp";
    case K::KwVoid:         return "KwVoid";
    case K::Comma:          return "Comma";
    case K::PipePipe:        return "PipePipe";
    }
    return "Unknown";
}
