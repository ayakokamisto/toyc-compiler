/// Parser implementation — P2 recursive descent.

#include "toyc/frontend/parser.h"

#include <sstream>

namespace toyc {

// ═══════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════

Parser::Parser(std::span<const Token> tokens, DiagnosticEngine& diag)
    : tokens_(tokens), diag_(diag) {
  // Ensure we always have at least EOF.
  if (tokens_.empty() || tokens_.back().kind != TokenKind::END_OF_FILE) {
    // This shouldn't happen if Lexer is correct, but be safe.
  }
}

std::unique_ptr<CompUnit> Parser::parse() {
  return parseCompUnit();
}

// ═══════════════════════════════════════════════════════════════════════════
// Token navigation
// ═══════════════════════════════════════════════════════════════════════════

const Token& Parser::current() const noexcept {
  if (pos_ >= tokens_.size()) return tokens_.back();
  return tokens_[pos_];
}

const Token& Parser::previous() const noexcept {
  if (pos_ == 0) return tokens_[0];
  return tokens_[pos_ - 1];
}

const Token& Parser::advance() noexcept {
  if (!atEnd()) ++pos_;
  return previous();
}

bool Parser::atEnd() const noexcept {
  return pos_ >= tokens_.size() || current().kind == TokenKind::END_OF_FILE;
}

bool Parser::check(TokenKind kind) const noexcept {
  return current().kind == kind;
}

bool Parser::match(TokenKind kind) noexcept {
  if (check(kind)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::matchAny(std::initializer_list<TokenKind> kinds) noexcept {
  for (auto k : kinds) {
    if (check(k)) {
      advance();
      return true;
    }
  }
  return false;
}

const Token& Parser::expect(TokenKind kind, std::string_view description) {
  if (check(kind)) {
    return advance();
  }
  std::ostringstream msg;
  msg << "expected " << description << ", got " << tokenKindName(current().kind);
  if (!current().rawLexeme.empty() && current().kind != TokenKind::END_OF_FILE) {
    msg << " '" << current().rawLexeme << "'";
  }
  diag_.error(current().range.begin, msg.str());
  hasError_ = true;
  // Return current token without advancing — caller decides recovery.
  return current();
}

// ═══════════════════════════════════════════════════════════════════════════
// Error recovery
// ═══════════════════════════════════════════════════════════════════════════

void Parser::reportUnexpected(std::string_view expected) {
  std::ostringstream msg;
  msg << "unexpected " << tokenKindName(current().kind);
  if (!current().rawLexeme.empty() && current().kind != TokenKind::END_OF_FILE) {
    msg << " '" << current().rawLexeme << "'";
  }
  msg << ", expected " << expected;
  diag_.error(current().range.begin, msg.str());
  hasError_ = true;
}

void Parser::synchronizeTopLevel() {
  while (!atEnd()) {
    if (check(TokenKind::KW_CONST) || check(TokenKind::KW_INT) ||
        check(TokenKind::KW_VOID) || check(TokenKind::END_OF_FILE)) {
      return;
    }
    advance();
  }
}

void Parser::synchronizeStatement() {
  while (!atEnd()) {
    if (check(TokenKind::SEMICOLON)) {
      advance();
      return;
    }
    if (check(TokenKind::RBRACE)) {
      return;  // Don't consume — block parser needs it.
    }
    if (check(TokenKind::KW_IF) || check(TokenKind::KW_WHILE) ||
        check(TokenKind::KW_BREAK) || check(TokenKind::KW_CONTINUE) ||
        check(TokenKind::KW_RETURN) || check(TokenKind::LBRACE) ||
        check(TokenKind::END_OF_FILE)) {
      return;
    }
    // const/int could start a local declaration — stop so parseStmt can handle it.
    // But if we haven't advanced yet, we must skip one token to avoid infinite loops.
    if (check(TokenKind::KW_CONST) || check(TokenKind::KW_INT)) {
      return;
    }
    advance();
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Top-level parsing
// ═══════════════════════════════════════════════════════════════════════════

std::unique_ptr<CompUnit> Parser::parseCompUnit() {
  auto unit = std::make_unique<CompUnit>();

  while (!check(TokenKind::END_OF_FILE)) {
    auto item = parseTopLevelItem();
    if (item) {
      unit->addItem(std::move(item));
    } else {
      synchronizeTopLevel();
    }
  }

  // Set range from first to last token.
  if (!tokens_.empty()) {
    SourceRange range(tokens_.front().range.begin, current().range.end);
    unit->setRange(range);
  }

  return unit;
}

std::unique_ptr<TopLevelItem> Parser::parseTopLevelItem() {
  // const → ConstDecl wrapped in GlobalDecl
  if (check(TokenKind::KW_CONST)) {
    auto decl = parseDecl();
    if (!decl) return nullptr;
    auto gl = std::make_unique<GlobalDecl>(std::move(decl));
    gl->setRange(gl->declaration()->range());
    return gl;
  }

  // void → FuncDef
  if (check(TokenKind::KW_VOID)) {
    auto typeTok = advance();
    auto nameTok = expect(TokenKind::IDENT, "function name");
    if (hasError_ && current().kind != TokenKind::IDENT) return nullptr;
    return parseFuncDef(TypeKind::Void, typeTok, nameTok);
  }

  // int → FuncDef or VarDecl
  if (check(TokenKind::KW_INT)) {
    auto typeTok = advance();
    auto nameTok = expect(TokenKind::IDENT, "function or variable name");
    if (hasError_ && current().kind != TokenKind::IDENT) return nullptr;

    if (check(TokenKind::LPAREN)) {
      return parseFuncDef(TypeKind::Int, typeTok, nameTok);
    }

    // VarDecl: int name = expr ;
    expect(TokenKind::ASSIGN, "'='");
    auto init = parseExpr();
    auto semiTok = expect(TokenKind::SEMICOLON, "';' after variable declaration");

    auto var = std::make_unique<VarDecl>(nameTok.rawLexeme, std::move(init));
    SourceRange range(typeTok.range.begin, semiTok.range.end);
    var->setRange(range);

    auto gl = std::make_unique<GlobalDecl>(std::move(var));
    gl->setRange(range);
    return gl;
  }

  reportUnexpected("declaration or function definition");
  return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// Declaration parsing
// ═══════════════════════════════════════════════════════════════════════════

std::unique_ptr<Decl> Parser::parseDecl() {
  if (check(TokenKind::KW_CONST)) return parseConstDecl();
  if (check(TokenKind::KW_INT)) return parseVarDecl();
  reportUnexpected("'const' or 'int'");
  return nullptr;
}

std::unique_ptr<ConstDecl> Parser::parseConstDecl() {
  auto constTok = expect(TokenKind::KW_CONST, "'const'");
  expect(TokenKind::KW_INT, "'int'");
  auto nameTok = expect(TokenKind::IDENT, "constant name");
  expect(TokenKind::ASSIGN, "'='");
  auto init = parseExpr();
  auto semiTok = expect(TokenKind::SEMICOLON, "';'");

  auto decl = std::make_unique<ConstDecl>(nameTok.rawLexeme, std::move(init));
  SourceRange range(constTok.range.begin, semiTok.range.end);
  decl->setRange(range);
  return decl;
}

std::unique_ptr<VarDecl> Parser::parseVarDecl() {
  auto intTok = expect(TokenKind::KW_INT, "'int'");
  auto nameTok = expect(TokenKind::IDENT, "variable name");
  expect(TokenKind::ASSIGN, "'='");
  auto init = parseExpr();
  auto semiTok = expect(TokenKind::SEMICOLON, "';'");

  auto decl = std::make_unique<VarDecl>(nameTok.rawLexeme, std::move(init));
  SourceRange range(intTok.range.begin, semiTok.range.end);
  decl->setRange(range);
  return decl;
}

// ═══════════════════════════════════════════════════════════════════════════
// Function parsing
// ═══════════════════════════════════════════════════════════════════════════

std::unique_ptr<FuncDef> Parser::parseFuncDef(TypeKind returnType,
                                               const Token& typeToken,
                                               const Token& nameToken) {
  expect(TokenKind::LPAREN, "'('");

  std::vector<ParamDecl> params;
  if (!check(TokenKind::RPAREN)) {
    params.push_back(parseParam());
    while (match(TokenKind::COMMA)) {
      params.push_back(parseParam());
    }
  }
  expect(TokenKind::RPAREN, "')'");

  auto body = parseBlock();

  auto func = std::make_unique<FuncDef>(returnType, nameToken.rawLexeme,
                                         std::move(params), std::move(body));
  SourceRange range(typeToken.range.begin, func->body()->range().end);
  func->setRange(range);
  return func;
}

ParamDecl Parser::parseParam() {
  expect(TokenKind::KW_INT, "'int'");
  auto nameTok = expect(TokenKind::IDENT, "parameter name");

  ParamDecl param(nameTok.rawLexeme);
  // ParamDecl range: from 'int' keyword to identifier end.
  // We use the name token range as a simple approximation.
  param.setRange(nameTok.range);
  return param;
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
  auto lbrace = expect(TokenKind::LBRACE, "'{'");
  auto block = std::make_unique<BlockStmt>();

  while (!check(TokenKind::RBRACE) && !check(TokenKind::END_OF_FILE)) {
    auto savedPos = pos_;
    auto stmt = parseStmt();
    if (stmt) {
      // If parseStmt didn't advance (stuck recovery dummy), force progress.
      if (pos_ == savedPos) {
        block->addStatement(std::move(stmt));
        if (!check(TokenKind::RBRACE) && !check(TokenKind::END_OF_FILE)) {
          advance();
        }
      } else {
        block->addStatement(std::move(stmt));
      }
    } else {
      synchronizeStatement();
    }
  }

  auto rbrace = expect(TokenKind::RBRACE, "'}'");
  SourceRange range(lbrace.range.begin, rbrace.range.end);
  block->setRange(range);
  return block;
}

// ═══════════════════════════════════════════════════════════════════════════
// Statement parsing
// ═══════════════════════════════════════════════════════════════════════════

std::unique_ptr<Stmt> Parser::parseStmt() {
  if (check(TokenKind::LBRACE)) return parseBlock();

  if (match(TokenKind::SEMICOLON)) {
    auto stmt = std::make_unique<EmptyStmt>();
    stmt->setRange(previous().range);
    return stmt;
  }

  if (check(TokenKind::KW_IF)) return parseIfStmt();
  if (check(TokenKind::KW_WHILE)) return parseWhileStmt();

  if (match(TokenKind::KW_BREAK)) {
    auto semiTok = expect(TokenKind::SEMICOLON, "';' after 'break'");
    auto stmt = std::make_unique<BreakStmt>();
    stmt->setRange(SourceRange(previous().range.begin, semiTok.range.end));
    return stmt;
  }

  if (match(TokenKind::KW_CONTINUE)) {
    auto semiTok = expect(TokenKind::SEMICOLON, "';' after 'continue'");
    auto stmt = std::make_unique<ContinueStmt>();
    stmt->setRange(SourceRange(previous().range.begin, semiTok.range.end));
    return stmt;
  }

  if (check(TokenKind::KW_RETURN)) return parseReturnStmt();

  if (check(TokenKind::KW_CONST) || check(TokenKind::KW_INT)) {
    auto decl = parseDecl();
    if (!decl) return nullptr;
    auto stmt = std::make_unique<DeclStmt>(std::move(decl));
    stmt->setRange(stmt->declaration()->range());
    return stmt;
  }

  return parseExprOrAssignStmt();
}

std::unique_ptr<Stmt> Parser::parseIfStmt() {
  auto ifTok = expect(TokenKind::KW_IF, "'if'");
  expect(TokenKind::LPAREN, "'('");
  auto cond = parseExpr();
  expect(TokenKind::RPAREN, "')'");
  auto thenBranch = parseStmt();

  // Save range before moving.
  SourceRange endRange = thenBranch->range();

  std::unique_ptr<Stmt> elseBranch;
  if (match(TokenKind::KW_ELSE)) {
    elseBranch = parseStmt();
    endRange = elseBranch->range();
  }

  auto stmt = std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch),
                                        std::move(elseBranch));
  stmt->setRange(SourceRange(ifTok.range.begin, endRange.end));
  return stmt;
}

std::unique_ptr<Stmt> Parser::parseWhileStmt() {
  auto whileTok = expect(TokenKind::KW_WHILE, "'while'");
  expect(TokenKind::LPAREN, "'('");
  auto cond = parseExpr();
  expect(TokenKind::RPAREN, "')'");
  auto body = parseStmt();

  auto bodyEnd = body->range().end;
  auto stmt = std::make_unique<WhileStmt>(std::move(cond), std::move(body));
  stmt->setRange(SourceRange(whileTok.range.begin, bodyEnd));
  return stmt;
}

std::unique_ptr<Stmt> Parser::parseReturnStmt() {
  auto retTok = expect(TokenKind::KW_RETURN, "'return'");

  if (match(TokenKind::SEMICOLON)) {
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->setRange(SourceRange(retTok.range.begin, previous().range.end));
    return stmt;
  }

  // If next token can't start an expression, report early.
  if (check(TokenKind::RBRACE) || check(TokenKind::END_OF_FILE)) {
    expect(TokenKind::SEMICOLON, "';' or expression");
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->setRange(SourceRange(retTok.range.begin, previous().range.end));
    return stmt;
  }

  auto val = parseExpr();
  auto semiTok = expect(TokenKind::SEMICOLON, "';' after return expression");

  auto stmt = std::make_unique<ReturnStmt>(std::move(val));
  stmt->setRange(SourceRange(retTok.range.begin, semiTok.range.end));
  return stmt;
}

std::unique_ptr<Stmt> Parser::parseExprOrAssignStmt() {
  // Check if this is an assignment: IDENT = expr ;
  if (check(TokenKind::IDENT)) {
    // Peek ahead to see if next token is ASSIGN.
    auto savedPos = pos_;

    auto nameTok = advance();
    if (check(TokenKind::ASSIGN)) {
      // It's an assignment.
      advance(); // consume =
      auto val = parseExpr();
      auto semiTok = expect(TokenKind::SEMICOLON, "';'");

      auto stmt = std::make_unique<AssignStmt>(nameTok.rawLexeme, std::move(val));
      stmt->setRange(SourceRange(nameTok.range.begin, semiTok.range.end));
      return stmt;
    }

    // Not an assignment — rewind and parse as expression.
    pos_ = savedPos;
  }

  // Expression statement.
  auto expr = parseExpr();
  auto semiTok = expect(TokenKind::SEMICOLON, "';'");

  auto stmt = std::make_unique<ExprStmt>(std::move(expr));
  stmt->setRange(SourceRange(stmt->expression()->range().begin, semiTok.range.end));
  return stmt;
}

// ═══════════════════════════════════════════════════════════════════════════
// Expression parsing (precedence climbing)
// ═══════════════════════════════════════════════════════════════════════════

std::unique_ptr<Expr> Parser::parseExpr() {
  return parseLogicalOrExpr();
}

// LogicalOr: logicalAndExpr (|| logicalAndExpr)*
std::unique_ptr<Expr> Parser::parseLogicalOrExpr() {
  auto lhs = parseLogicalAndExpr();
  while (match(TokenKind::OR)) {
    auto rhs = parseLogicalAndExpr();
    auto expr = std::make_unique<BinaryExpr>(BinaryOperator::LogicalOr,
                                              std::move(lhs), std::move(rhs));
    expr->setRange(SourceRange(expr->lhs()->range().begin,
                               expr->rhs()->range().end));
    lhs = std::move(expr);
  }
  return lhs;
}

// LogicalAnd: equalityExpr (&& equalityExpr)*
std::unique_ptr<Expr> Parser::parseLogicalAndExpr() {
  auto lhs = parseEqualityExpr();
  while (match(TokenKind::AND)) {
    auto rhs = parseEqualityExpr();
    auto expr = std::make_unique<BinaryExpr>(BinaryOperator::LogicalAnd,
                                              std::move(lhs), std::move(rhs));
    expr->setRange(SourceRange(expr->lhs()->range().begin,
                               expr->rhs()->range().end));
    lhs = std::move(expr);
  }
  return lhs;
}

// Equality: relationalExpr ((== | !=) relationalExpr)*
std::unique_ptr<Expr> Parser::parseEqualityExpr() {
  auto lhs = parseRelationalExpr();
  while (true) {
    BinaryOperator op;
    if (match(TokenKind::EQ)) {
      op = BinaryOperator::Equal;
    } else if (match(TokenKind::NE)) {
      op = BinaryOperator::NotEqual;
    } else {
      break;
    }
    auto rhs = parseRelationalExpr();
    auto expr = std::make_unique<BinaryExpr>(op, std::move(lhs), std::move(rhs));
    expr->setRange(SourceRange(expr->lhs()->range().begin,
                               expr->rhs()->range().end));
    lhs = std::move(expr);
  }
  return lhs;
}

// Relational: additiveExpr ((< | <= | > | >=) additiveExpr)*
std::unique_ptr<Expr> Parser::parseRelationalExpr() {
  auto lhs = parseAdditiveExpr();
  while (true) {
    BinaryOperator op;
    if (match(TokenKind::LT)) {
      op = BinaryOperator::Less;
    } else if (match(TokenKind::LE)) {
      op = BinaryOperator::LessEqual;
    } else if (match(TokenKind::GT)) {
      op = BinaryOperator::Greater;
    } else if (match(TokenKind::GE)) {
      op = BinaryOperator::GreaterEqual;
    } else {
      break;
    }
    auto rhs = parseAdditiveExpr();
    auto expr = std::make_unique<BinaryExpr>(op, std::move(lhs), std::move(rhs));
    expr->setRange(SourceRange(expr->lhs()->range().begin,
                               expr->rhs()->range().end));
    lhs = std::move(expr);
  }
  return lhs;
}

// Additive: multiplicativeExpr ((+ | -) multiplicativeExpr)*
std::unique_ptr<Expr> Parser::parseAdditiveExpr() {
  auto lhs = parseMultiplicativeExpr();
  while (true) {
    BinaryOperator op;
    if (match(TokenKind::PLUS)) {
      op = BinaryOperator::Add;
    } else if (match(TokenKind::MINUS)) {
      op = BinaryOperator::Subtract;
    } else {
      break;
    }
    auto rhs = parseMultiplicativeExpr();
    auto expr = std::make_unique<BinaryExpr>(op, std::move(lhs), std::move(rhs));
    expr->setRange(SourceRange(expr->lhs()->range().begin,
                               expr->rhs()->range().end));
    lhs = std::move(expr);
  }
  return lhs;
}

// Multiplicative: unaryExpr ((* | / | %) unaryExpr)*
std::unique_ptr<Expr> Parser::parseMultiplicativeExpr() {
  auto lhs = parseUnaryExpr();
  while (true) {
    BinaryOperator op;
    if (match(TokenKind::MUL)) {
      op = BinaryOperator::Multiply;
    } else if (match(TokenKind::DIV)) {
      op = BinaryOperator::Divide;
    } else if (match(TokenKind::MOD)) {
      op = BinaryOperator::Modulo;
    } else {
      break;
    }
    auto rhs = parseUnaryExpr();
    auto expr = std::make_unique<BinaryExpr>(op, std::move(lhs), std::move(rhs));
    expr->setRange(SourceRange(expr->lhs()->range().begin,
                               expr->rhs()->range().end));
    lhs = std::move(expr);
  }
  return lhs;
}

// Unary: (+ | - | !) unaryExpr | primaryExpr
std::unique_ptr<Expr> Parser::parseUnaryExpr() {
  UnaryOperator op;
  if (match(TokenKind::PLUS)) {
    op = UnaryOperator::Plus;
  } else if (match(TokenKind::MINUS)) {
    op = UnaryOperator::Minus;
  } else if (match(TokenKind::NOT)) {
    op = UnaryOperator::LogicalNot;
  } else {
    return parsePrimaryExpr();
  }

  auto operand = parseUnaryExpr();  // right-associative
  auto expr = std::make_unique<UnaryExpr>(op, std::move(operand));
  // Range: from the operator to the end of the operand.
  // Use previous() for the operator token.
  expr->setRange(SourceRange(previous().range.begin, expr->operand()->range().end));
  return expr;
}

// Primary: NUMBER | IDENT | IDENT(args) | (expr)
std::unique_ptr<Expr> Parser::parsePrimaryExpr() {
  // Number literal
  if (check(TokenKind::NUMBER)) {
    auto tok = advance();
    auto expr = std::make_unique<IntegerLiteralExpr>(tok.rawLexeme);
    expr->setRange(tok.range);
    return expr;
  }

  // Identifier or function call
  if (check(TokenKind::IDENT)) {
    auto nameTok = advance();

    if (match(TokenKind::LPAREN)) {
      // Function call.
      std::vector<std::unique_ptr<Expr>> args;
      if (!check(TokenKind::RPAREN)) {
        args.push_back(parseExpr());
        while (match(TokenKind::COMMA)) {
          args.push_back(parseExpr());
        }
      }
      auto rparen = expect(TokenKind::RPAREN, "')'");

      auto expr = std::make_unique<CallExpr>(nameTok.rawLexeme, std::move(args));
      expr->setRange(SourceRange(nameTok.range.begin, rparen.range.end));
      return expr;
    }

    // Simple identifier.
    auto expr = std::make_unique<IdentifierExpr>(nameTok.rawLexeme);
    expr->setRange(nameTok.range);
    return expr;
  }

  // Parenthesized expression
  if (check(TokenKind::LPAREN)) {
    auto lparen = advance();
    auto inner = parseExpr();
    auto rparen = expect(TokenKind::RPAREN, "')'");
    inner->setRange(SourceRange(lparen.range.begin, rparen.range.end));
    return inner;
  }

  reportUnexpected("expression");
  // Create a dummy literal to avoid null.
  auto dummy = std::make_unique<IntegerLiteralExpr>("0");
  dummy->setRange(current().range);
  return dummy;
}

} // namespace toyc
