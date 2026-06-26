#pragma once
/// ToyC parser — P2 recursive descent implementation.

#include "toyc/frontend/ast.h"
#include "toyc/frontend/token.h"
#include "toyc/support/diagnostics.h"

#include <initializer_list>
#include <memory>
#include <span>
#include <string_view>

namespace toyc {

/// Parser: converts a token stream (from Lexer) into an AST.
class Parser {
public:
  Parser(std::span<const Token> tokens, DiagnosticEngine& diag);

  /// Parse the token stream into a CompUnit AST.
  std::unique_ptr<CompUnit> parse();

  [[nodiscard]] bool hasError() const noexcept { return hasError_; }

private:
  // ── Token navigation ──────────────────────────────────────────────────────
  [[nodiscard]] const Token& current() const noexcept;
  [[nodiscard]] const Token& previous() const noexcept;
  const Token& advance() noexcept;
  [[nodiscard]] bool atEnd() const noexcept;

  [[nodiscard]] bool check(TokenKind kind) const noexcept;
  bool match(TokenKind kind) noexcept;
  bool matchAny(std::initializer_list<TokenKind> kinds) noexcept;
  const Token& expect(TokenKind kind, std::string_view description);

  // ── Error recovery ────────────────────────────────────────────────────────
  void reportUnexpected(std::string_view expected);
  void synchronizeTopLevel();
  void synchronizeStatement();

  // ── Parsing ───────────────────────────────────────────────────────────────
  std::unique_ptr<CompUnit> parseCompUnit();
  std::unique_ptr<TopLevelItem> parseTopLevelItem();

  std::unique_ptr<Decl> parseDecl();
  std::unique_ptr<ConstDecl> parseConstDecl();
  std::unique_ptr<VarDecl> parseVarDecl();

  std::unique_ptr<FuncDef> parseFuncDef(TypeKind returnType,
                                         const Token& typeToken,
                                         const Token& nameToken);

  ParamDecl parseParam();
  std::unique_ptr<BlockStmt> parseBlock();

  std::unique_ptr<Stmt> parseStmt();
  std::unique_ptr<Stmt> parseIfStmt();
  std::unique_ptr<Stmt> parseWhileStmt();
  std::unique_ptr<Stmt> parseReturnStmt();
  std::unique_ptr<Stmt> parseExprOrAssignStmt();

  // ── Expression precedence levels ──────────────────────────────────────────
  std::unique_ptr<Expr> parseExpr();
  std::unique_ptr<Expr> parseLogicalOrExpr();
  std::unique_ptr<Expr> parseLogicalAndExpr();
  std::unique_ptr<Expr> parseEqualityExpr();
  std::unique_ptr<Expr> parseRelationalExpr();
  std::unique_ptr<Expr> parseAdditiveExpr();
  std::unique_ptr<Expr> parseMultiplicativeExpr();
  std::unique_ptr<Expr> parseUnaryExpr();
  std::unique_ptr<Expr> parsePrimaryExpr();

  // ── State ─────────────────────────────────────────────────────────────────
  std::span<const Token> tokens_;
  std::size_t pos_ = 0;
  DiagnosticEngine& diag_;
  bool hasError_ = false;
};

} // namespace toyc
