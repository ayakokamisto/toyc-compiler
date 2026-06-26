/// Token utilities — name lookup and classification.

#include "toyc/frontend/token.h"

namespace toyc {

std::string_view tokenKindName(TokenKind kind) {
  switch (kind) {
    case TokenKind::KW_CONST:    return "KW_CONST";
    case TokenKind::KW_INT:      return "KW_INT";
    case TokenKind::KW_VOID:     return "KW_VOID";
    case TokenKind::KW_IF:       return "KW_IF";
    case TokenKind::KW_ELSE:     return "KW_ELSE";
    case TokenKind::KW_WHILE:    return "KW_WHILE";
    case TokenKind::KW_BREAK:    return "KW_BREAK";
    case TokenKind::KW_CONTINUE: return "KW_CONTINUE";
    case TokenKind::KW_RETURN:   return "KW_RETURN";
    case TokenKind::IDENT:       return "IDENT";
    case TokenKind::NUMBER:      return "NUMBER";
    case TokenKind::ASSIGN:      return "ASSIGN";
    case TokenKind::OR:          return "OR";
    case TokenKind::AND:         return "AND";
    case TokenKind::LT:          return "LT";
    case TokenKind::LE:          return "LE";
    case TokenKind::GT:          return "GT";
    case TokenKind::GE:          return "GE";
    case TokenKind::EQ:          return "EQ";
    case TokenKind::NE:          return "NE";
    case TokenKind::PLUS:        return "PLUS";
    case TokenKind::MINUS:       return "MINUS";
    case TokenKind::MUL:         return "MUL";
    case TokenKind::DIV:         return "DIV";
    case TokenKind::MOD:         return "MOD";
    case TokenKind::NOT:         return "NOT";
    case TokenKind::SEMICOLON:   return "SEMICOLON";
    case TokenKind::LBRACE:      return "LBRACE";
    case TokenKind::RBRACE:      return "RBRACE";
    case TokenKind::LPAREN:      return "LPAREN";
    case TokenKind::RPAREN:      return "RPAREN";
    case TokenKind::COMMA:       return "COMMA";
    case TokenKind::END_OF_FILE: return "END_OF_FILE";
    case TokenKind::INVALID:     return "INVALID";
  }
  return "UNKNOWN";
}

bool isKeyword(TokenKind kind) {
  switch (kind) {
    case TokenKind::KW_CONST:
    case TokenKind::KW_INT:
    case TokenKind::KW_VOID:
    case TokenKind::KW_IF:
    case TokenKind::KW_ELSE:
    case TokenKind::KW_WHILE:
    case TokenKind::KW_BREAK:
    case TokenKind::KW_CONTINUE:
    case TokenKind::KW_RETURN:
      return true;
    default:
      return false;
  }
}

} // namespace toyc
