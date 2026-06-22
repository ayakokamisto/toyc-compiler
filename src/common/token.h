#pragma once

#include "common/diagnostic.h"

#include <string>
#include <string_view>

namespace toyc {

enum class TokenKind {
    Eof,
    Identifier,
    IntegerLiteral,

    KwInt,
    KwVoid,
    KwConst,
    KwIf,
    KwElse,
    KwWhile,
    KwBreak,
    KwContinue,
    KwReturn,

    LParen,
    RParen,
    LBrace,
    RBrace,
    Comma,
    Semicolon,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Bang,
    Less,
    Greater,
    Equal,
    LessEqual,
    GreaterEqual,
    EqualEqual,
    BangEqual,
    AmpAmp,
    PipePipe,
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string lexeme;
    SourceLocation location;
};

inline std::string_view tokenKindToString(TokenKind kind) {
    switch (kind) {
    case TokenKind::Eof:
        return "EOF";
    case TokenKind::Identifier:
        return "Identifier";
    case TokenKind::IntegerLiteral:
        return "IntegerLiteral";
    case TokenKind::KwInt:
        return "int";
    case TokenKind::KwVoid:
        return "void";
    case TokenKind::KwConst:
        return "const";
    case TokenKind::KwIf:
        return "if";
    case TokenKind::KwElse:
        return "else";
    case TokenKind::KwWhile:
        return "while";
    case TokenKind::KwBreak:
        return "break";
    case TokenKind::KwContinue:
        return "continue";
    case TokenKind::KwReturn:
        return "return";
    case TokenKind::LParen:
        return "(";
    case TokenKind::RParen:
        return ")";
    case TokenKind::LBrace:
        return "{";
    case TokenKind::RBrace:
        return "}";
    case TokenKind::Comma:
        return ",";
    case TokenKind::Semicolon:
        return ";";
    case TokenKind::Plus:
        return "+";
    case TokenKind::Minus:
        return "-";
    case TokenKind::Star:
        return "*";
    case TokenKind::Slash:
        return "/";
    case TokenKind::Percent:
        return "%";
    case TokenKind::Bang:
        return "!";
    case TokenKind::Less:
        return "<";
    case TokenKind::Greater:
        return ">";
    case TokenKind::Equal:
        return "=";
    case TokenKind::LessEqual:
        return "<=";
    case TokenKind::GreaterEqual:
        return ">=";
    case TokenKind::EqualEqual:
        return "==";
    case TokenKind::BangEqual:
        return "!=";
    case TokenKind::AmpAmp:
        return "&&";
    case TokenKind::PipePipe:
        return "||";
    }

    return "<unknown>";
}

} // namespace toyc
