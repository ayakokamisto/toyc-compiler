#pragma once
/// Token definitions for the ToyC lexer.

#include "toyc/support/source_location.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace toyc {

/// Token kind enumeration — covers all ToyC tokens from 词汇翻译表.md.
enum class TokenKind : uint8_t {
  // Keywords
  KW_CONST,
  KW_INT,
  KW_VOID,
  KW_IF,
  KW_ELSE,
  KW_WHILE,
  KW_BREAK,
  KW_CONTINUE,
  KW_RETURN,

  // Identifiers & literals
  IDENT,
  NUMBER,

  // Operators
  ASSIGN,     // =
  OR,         // ||
  AND,        // &&
  LT,         // <
  LE,         // <=
  GT,         // >
  GE,         // >=
  EQ,         // ==
  NE,         // !=
  PLUS,       // +
  MINUS,      // -
  MUL,        // *
  DIV,        // /
  MOD,        // %
  NOT,        // !

  // Delimiters
  SEMICOLON,  // ;
  LBRACE,     // {
  RBRACE,     // }
  LPAREN,     // (
  RPAREN,     // )
  COMMA,      // ,

  // Special
  END_OF_FILE,
  INVALID,
};

/// Convert a TokenKind to its string name.
std::string_view tokenKindName(TokenKind kind);

/// Check if a TokenKind is a keyword.
bool isKeyword(TokenKind kind);

/// A single token produced by the lexer.
struct Token {
  TokenKind kind = TokenKind::INVALID;
  std::string rawLexeme;      ///< Original text from source.
  SourceRange range;           ///< Location in source.

  Token() = default;
  Token(TokenKind k, std::string lexeme, SourceRange r)
      : kind(k), rawLexeme(std::move(lexeme)), range(r) {}
};

} // namespace toyc
