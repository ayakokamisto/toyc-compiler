/// Lexer implementation — P1 real scanning.

#include "toyc/frontend/lexer.h"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <unordered_map>

namespace toyc {

// ═══════════════════════════════════════════════════════════════════════════
// Lexer
// ═══════════════════════════════════════════════════════════════════════════

static const std::unordered_map<std::string_view, TokenKind> kKeywords = {
    {"const",    TokenKind::KW_CONST},
    {"int",      TokenKind::KW_INT},
    {"void",     TokenKind::KW_VOID},
    {"if",       TokenKind::KW_IF},
    {"else",     TokenKind::KW_ELSE},
    {"while",    TokenKind::KW_WHILE},
    {"break",    TokenKind::KW_BREAK},
    {"continue", TokenKind::KW_CONTINUE},
    {"return",   TokenKind::KW_RETURN},
};

Lexer::Lexer(std::string_view source, DiagnosticEngine& diag)
    : source_(source), diag_(diag) {}

char Lexer::peek() const noexcept {
  if (pos_ >= source_.size()) return '\0';
  return source_[pos_];
}

char Lexer::peekAt(std::size_t offset) const noexcept {
  auto idx = pos_ + offset;
  if (idx >= source_.size()) return '\0';
  return source_[idx];
}

char Lexer::advance() noexcept {
  char ch = peek();
  if (ch == '\0') return ch;
  ++pos_;
  if (ch == '\n') {
    ++line_;
    col_ = 1;
  } else if (ch == '\r') {
    ++line_;
    col_ = 1;
    if (peek() == '\n') {
      ++pos_;
    }
  } else {
    ++col_;
  }
  return ch;
}

bool Lexer::atEnd() const noexcept {
  return pos_ >= source_.size();
}

SourceLocation Lexer::currentLoc() const noexcept {
  return SourceLocation{static_cast<uint32_t>(pos_), line_, col_};
}

bool Lexer::tokenize(std::vector<Token>& out) {
  while (!atEnd()) {
    skipWhitespaceAndComments();
    if (atEnd()) break;

    char ch = peek();

    if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
      out.push_back(lexIdentifierOrKeyword());
    } else if (std::isdigit(static_cast<unsigned char>(ch))) {
      out.push_back(lexNumber());
    } else {
      out.push_back(lexOperatorOrPunctuation());
    }
  }

  auto eofLoc = currentLoc();
  out.emplace_back(TokenKind::END_OF_FILE, "",
                   SourceRange{eofLoc, eofLoc});

  return !hasError_;
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  tokenize(tokens);
  return tokens;
}

Token Lexer::lexIdentifierOrKeyword() {
  auto begin = currentLoc();
  std::size_t start = pos_;

  while (!atEnd()) {
    char ch = peek();
    if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
      advance();
    } else {
      break;
    }
  }

  std::string_view lexeme(source_.data() + start, pos_ - start);
  auto end = currentLoc();

  auto it = kKeywords.find(lexeme);
  TokenKind kind = (it != kKeywords.end()) ? it->second : TokenKind::IDENT;

  return Token(kind, std::string(lexeme), SourceRange{begin, end});
}

Token Lexer::lexNumber() {
  auto begin = currentLoc();
  std::size_t start = pos_;

  if (peek() == '0') {
    advance();
    if (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
      auto errLoc = begin;
      while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
      }
      auto end = currentLoc();
      std::string_view lexeme(source_.data() + start, pos_ - start);
      diag_.error(errLoc, "invalid integer literal with leading zero: '" +
                              std::string(lexeme) + "'");
      hasError_ = true;
      return Token(TokenKind::INVALID, std::string(lexeme),
                   SourceRange{begin, end});
    }
  } else {
    advance();
    while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
      advance();
    }
  }

  auto end = currentLoc();
  std::string_view lexeme(source_.data() + start, pos_ - start);
  return Token(TokenKind::NUMBER, std::string(lexeme),
               SourceRange{begin, end});
}

Token Lexer::lexOperatorOrPunctuation() {
  auto begin = currentLoc();
  char ch = advance();
  auto singleEnd = currentLoc();

  switch (ch) {
    case '|':
      if (peek() == '|') {
        advance();
        return Token(TokenKind::OR, "||", SourceRange{begin, currentLoc()});
      }
      reportInvalidCharacter(ch, begin);
      return Token(TokenKind::INVALID, std::string(1, ch),
                   SourceRange{begin, singleEnd});

    case '&':
      if (peek() == '&') {
        advance();
        return Token(TokenKind::AND, "&&", SourceRange{begin, currentLoc()});
      }
      reportInvalidCharacter(ch, begin);
      return Token(TokenKind::INVALID, std::string(1, ch),
                   SourceRange{begin, singleEnd});

    case '<':
      if (peek() == '=') {
        advance();
        return Token(TokenKind::LE, "<=", SourceRange{begin, currentLoc()});
      }
      return Token(TokenKind::LT, "<", SourceRange{begin, singleEnd});

    case '>':
      if (peek() == '=') {
        advance();
        return Token(TokenKind::GE, ">=", SourceRange{begin, currentLoc()});
      }
      return Token(TokenKind::GT, ">", SourceRange{begin, singleEnd});

    case '=':
      if (peek() == '=') {
        advance();
        return Token(TokenKind::EQ, "==", SourceRange{begin, currentLoc()});
      }
      return Token(TokenKind::ASSIGN, "=", SourceRange{begin, singleEnd});

    case '!':
      if (peek() == '=') {
        advance();
        return Token(TokenKind::NE, "!=", SourceRange{begin, currentLoc()});
      }
      return Token(TokenKind::NOT, "!", SourceRange{begin, singleEnd});

    case '/':
      return Token(TokenKind::DIV, "/", SourceRange{begin, singleEnd});

    case '+': return Token(TokenKind::PLUS,     "+", SourceRange{begin, singleEnd});
    case '-': return Token(TokenKind::MINUS,    "-", SourceRange{begin, singleEnd});
    case '*': return Token(TokenKind::MUL,      "*", SourceRange{begin, singleEnd});
    case '%': return Token(TokenKind::MOD,      "%", SourceRange{begin, singleEnd});

    case ';': return Token(TokenKind::SEMICOLON, ";", SourceRange{begin, singleEnd});
    case '{': return Token(TokenKind::LBRACE,    "{", SourceRange{begin, singleEnd});
    case '}': return Token(TokenKind::RBRACE,    "}", SourceRange{begin, singleEnd});
    case '(': return Token(TokenKind::LPAREN,    "(", SourceRange{begin, singleEnd});
    case ')': return Token(TokenKind::RPAREN,    ")", SourceRange{begin, singleEnd});
    case ',': return Token(TokenKind::COMMA,     ",", SourceRange{begin, singleEnd});

    default:
      reportInvalidCharacter(ch, begin);
      return Token(TokenKind::INVALID, std::string(1, ch),
                   SourceRange{begin, singleEnd});
  }
}

void Lexer::skipWhitespaceAndComments() {
  while (!atEnd()) {
    char ch = peek();

    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
      advance();
      continue;
    }

    if (ch == '/') {
      char next = peekAt(1);
      if (next == '/') {
        advance(); advance();
        while (!atEnd() && peek() != '\n') {
          advance();
        }
        continue;
      }
      if (next == '*') {
        auto begin = currentLoc();
        advance(); advance();
        bool closed = false;
        while (!atEnd()) {
          if (peek() == '*' && peekAt(1) == '/') {
            advance(); advance();
            closed = true;
            break;
          }
          advance();
        }
        if (!closed) {
          reportUnterminatedBlockComment(begin);
        }
        continue;
      }
    }

    break;
  }
}

void Lexer::reportInvalidCharacter(char ch, SourceLocation where) {
  std::ostringstream msg;
  msg << "invalid character: '";
  if (ch == '\'' || ch == '\\') {
    msg << '\\' << ch;
  } else if (ch >= 32 && ch < 127) {
    msg << ch;
  } else {
    msg << "\\x" << std::hex << static_cast<int>(static_cast<unsigned char>(ch));
  }
  msg << "'";
  diag_.error(where, msg.str());
  hasError_ = true;
}

void Lexer::reportUnterminatedBlockComment(SourceLocation begin) {
  diag_.error(begin, "unterminated block comment");
  hasError_ = true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Token dump
// ═══════════════════════════════════════════════════════════════════════════

static std::string escapeLexeme(const std::string& lexeme) {
  std::string result;
  result.reserve(lexeme.size());
  for (char c : lexeme) {
    switch (c) {
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\\': result += "\\\\"; break;
      case '\'': result += "\\'"; break;
      default:   result += c; break;
    }
  }
  return result;
}

void dumpTokens(std::span<const Token> tokens, std::ostream& out) {
  for (const auto& tok : tokens) {
    out << tok.range.begin.line << ":" << tok.range.begin.column
        << " " << tokenKindName(tok.kind)
        << " '" << escapeLexeme(tok.rawLexeme) << "'\n";
  }
}

} // namespace toyc
