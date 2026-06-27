#pragma once
/// ToyC lexer interface.
/// Splits source text into a stream of Tokens.

#include "toyc/frontend/token.h"
#include "toyc/support/diagnostics.h"

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace toyc {

/// Lexer: converts source text into a sequence of tokens.
class Lexer {
public:
  /// @param source   The ToyC source text. Lexer stores it as std::string_view;
  ///                 callers must keep it valid until tokenize() returns.
  /// @param diag     Diagnostic engine for error reporting.
  Lexer(std::string_view source, DiagnosticEngine& diag);

  /// Tokenize the entire source. Returns a vector of tokens (including EOF).
  /// @returns true if tokenization succeeded without lexical errors.
  bool tokenize(std::vector<Token>& out);

  /// Convenience: tokenize and return the vector.
  std::vector<Token> tokenize();

  [[nodiscard]] bool hasError() const noexcept { return hasError_; }

private:
  // ── Character access ──────────────────────────────────────────────────────
  [[nodiscard]] char peek() const noexcept;
  [[nodiscard]] char peekAt(std::size_t offset) const noexcept;
  char advance() noexcept;
  [[nodiscard]] bool atEnd() const noexcept;

  // ── Source location ───────────────────────────────────────────────────────
  [[nodiscard]] SourceLocation currentLoc() const noexcept;

  // ── Scanning ──────────────────────────────────────────────────────────────
  void skipWhitespaceAndComments();
  Token lexIdentifierOrKeyword();
  Token lexNumber();
  Token lexOperatorOrPunctuation();

  // ── Diagnostics ───────────────────────────────────────────────────────────
  void reportInvalidCharacter(char ch, SourceLocation where);
  void reportUnterminatedBlockComment(SourceLocation begin);

  // ── State ─────────────────────────────────────────────────────────────────
  std::string_view source_;
  std::size_t pos_ = 0;
  uint32_t line_ = 1;
  uint32_t col_ = 1;
  DiagnosticEngine& diag_;
  bool hasError_ = false;
};

/// Dump tokens to an output stream in the format:
///   <line>:<column> <TokenKind> '<escaped raw lexeme>'
void dumpTokens(std::span<const Token> tokens, std::ostream& out);

} // namespace toyc
