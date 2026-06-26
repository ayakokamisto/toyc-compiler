#pragma once
/// AST printer — P2 structured tree output.

#include "toyc/frontend/ast.h"

#include <ostream>

namespace toyc {

/// Prints an AST as a structured tree to an output stream.
class ASTPrinter : public ASTVisitor {
public:
  explicit ASTPrinter(std::ostream& out) : out_(out) {}

  void visitCompUnit(const CompUnit& node) override;
  void visitGlobalDecl(const GlobalDecl& node) override;
  void visitFuncDef(const FuncDef& node) override;
  void visitConstDecl(const ConstDecl& node) override;
  void visitVarDecl(const VarDecl& node) override;
  void visitParamDecl(const ParamDecl& node) override;
  void visitBlockStmt(const BlockStmt& node) override;
  void visitEmptyStmt(const EmptyStmt& node) override;
  void visitExprStmt(const ExprStmt& node) override;
  void visitAssignStmt(const AssignStmt& node) override;
  void visitDeclStmt(const DeclStmt& node) override;
  void visitIfStmt(const IfStmt& node) override;
  void visitWhileStmt(const WhileStmt& node) override;
  void visitBreakStmt(const BreakStmt& node) override;
  void visitContinueStmt(const ContinueStmt& node) override;
  void visitReturnStmt(const ReturnStmt& node) override;
  void visitIntegerLiteralExpr(const IntegerLiteralExpr& node) override;
  void visitIdentifierExpr(const IdentifierExpr& node) override;
  void visitUnaryExpr(const UnaryExpr& node) override;
  void visitBinaryExpr(const BinaryExpr& node) override;
  void visitCallExpr(const CallExpr& node) override;

private:
  void indent();
  void pushIndent();
  void popIndent();

  std::ostream& out_;
  int depth_ = 0;
};

} // namespace toyc
