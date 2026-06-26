#pragma once
/// ToyC AST node definitions — P2 complete implementation.

#include "toyc/support/source_location.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace toyc {

// ═══════════════════════════════════════════════════════════════════════════
// Enums
// ═══════════════════════════════════════════════════════════════════════════

enum class TypeKind : uint8_t { Int, Void };

enum class UnaryOperator : uint8_t { Plus, Minus, LogicalNot };

enum class BinaryOperator : uint8_t {
  LogicalOr,
  LogicalAnd,
  Equal,
  NotEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  Add,
  Subtract,
  Multiply,
  Divide,
  Modulo,
};

/// Human-readable names.
const char* typeKindName(TypeKind k);
const char* unaryOpName(UnaryOperator op);
const char* binaryOpName(BinaryOperator op);

// ═══════════════════════════════════════════════════════════════════════════
// AST kind enum
// ═══════════════════════════════════════════════════════════════════════════

enum class ASTKind : uint8_t {
  CompUnit,
  GlobalDecl,
  FuncDef,
  ConstDecl,
  VarDecl,
  ParamDecl,
  BlockStmt,
  EmptyStmt,
  ExprStmt,
  AssignStmt,
  DeclStmt,
  IfStmt,
  WhileStmt,
  BreakStmt,
  ContinueStmt,
  ReturnStmt,
  IntegerLiteralExpr,
  IdentifierExpr,
  UnaryExpr,
  BinaryExpr,
  CallExpr,
};

// ═══════════════════════════════════════════════════════════════════════════
// Base node
// ═══════════════════════════════════════════════════════════════════════════

class ASTNode {
public:
  explicit ASTNode(ASTKind k) : kind_(k) {}
  virtual ~ASTNode() = default;

  [[nodiscard]] ASTKind kind() const { return kind_; }
  [[nodiscard]] SourceRange range() const { return range_; }
  void setRange(SourceRange r) { range_ = r; }

private:
  ASTKind kind_;
  SourceRange range_;
};

// ═══════════════════════════════════════════════════════════════════════════
// Forward declarations
// ═══════════════════════════════════════════════════════════════════════════

class Expr;
class Decl;
class Stmt;
class TopLevelItem;

// ═══════════════════════════════════════════════════════════════════════════
// Expression nodes
// ═══════════════════════════════════════════════════════════════════════════

class Expr : public ASTNode {
public:
  using ASTNode::ASTNode;
};

class IntegerLiteralExpr : public Expr {
public:
  explicit IntegerLiteralExpr(std::string raw)
      : Expr(ASTKind::IntegerLiteralExpr), rawValue_(std::move(raw)) {}
  [[nodiscard]] const std::string& rawValue() const { return rawValue_; }

private:
  std::string rawValue_;
};

class IdentifierExpr : public Expr {
public:
  explicit IdentifierExpr(std::string name)
      : Expr(ASTKind::IdentifierExpr), name_(std::move(name)) {}
  [[nodiscard]] const std::string& name() const { return name_; }

private:
  std::string name_;
};

class UnaryExpr : public Expr {
public:
  UnaryExpr(UnaryOperator op, std::unique_ptr<Expr> operand)
      : Expr(ASTKind::UnaryExpr), op_(op), operand_(std::move(operand)) {}
  [[nodiscard]] UnaryOperator op() const { return op_; }
  [[nodiscard]] const Expr* operand() const { return operand_.get(); }
  [[nodiscard]] std::unique_ptr<Expr> takeOperand() { return std::move(operand_); }

private:
  UnaryOperator op_;
  std::unique_ptr<Expr> operand_;
};

class BinaryExpr : public Expr {
public:
  BinaryExpr(BinaryOperator op, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
      : Expr(ASTKind::BinaryExpr), op_(op), lhs_(std::move(l)), rhs_(std::move(r)) {}
  [[nodiscard]] BinaryOperator op() const { return op_; }
  [[nodiscard]] const Expr* lhs() const { return lhs_.get(); }
  [[nodiscard]] const Expr* rhs() const { return rhs_.get(); }

private:
  BinaryOperator op_;
  std::unique_ptr<Expr> lhs_;
  std::unique_ptr<Expr> rhs_;
};

class CallExpr : public Expr {
public:
  CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args)
      : Expr(ASTKind::CallExpr), calleeName_(std::move(callee)),
        arguments_(std::move(args)) {}
  [[nodiscard]] const std::string& calleeName() const { return calleeName_; }
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>>& arguments() const { return arguments_; }

private:
  std::string calleeName_;
  std::vector<std::unique_ptr<Expr>> arguments_;
};

// ═══════════════════════════════════════════════════════════════════════════
// Declaration nodes
// ═══════════════════════════════════════════════════════════════════════════

class Decl : public ASTNode {
public:
  Decl(ASTKind k, std::string name, std::unique_ptr<Expr> init)
      : ASTNode(k), name_(std::move(name)), initializer_(std::move(init)) {}
  [[nodiscard]] const std::string& name() const { return name_; }
  [[nodiscard]] const Expr* initializer() const { return initializer_.get(); }

private:
  std::string name_;
  std::unique_ptr<Expr> initializer_;
};

class ConstDecl : public Decl {
public:
  ConstDecl(std::string name, std::unique_ptr<Expr> init)
      : Decl(ASTKind::ConstDecl, std::move(name), std::move(init)) {}
};

class VarDecl : public Decl {
public:
  VarDecl(std::string name, std::unique_ptr<Expr> init)
      : Decl(ASTKind::VarDecl, std::move(name), std::move(init)) {}
};

class ParamDecl : public ASTNode {
public:
  explicit ParamDecl(std::string name)
      : ASTNode(ASTKind::ParamDecl), name_(std::move(name)) {}
  [[nodiscard]] const std::string& name() const { return name_; }

private:
  std::string name_;
};

// ═══════════════════════════════════════════════════════════════════════════
// Statement nodes
// ═══════════════════════════════════════════════════════════════════════════

class Stmt : public ASTNode {
public:
  using ASTNode::ASTNode;
};

class BlockStmt : public Stmt {
public:
  BlockStmt() : Stmt(ASTKind::BlockStmt) {}
  void addStatement(std::unique_ptr<Stmt> s) { stmts_.push_back(std::move(s)); }
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& statements() const { return stmts_; }

private:
  std::vector<std::unique_ptr<Stmt>> stmts_;
};

class EmptyStmt : public Stmt {
public:
  EmptyStmt() : Stmt(ASTKind::EmptyStmt) {}
};

class ExprStmt : public Stmt {
public:
  explicit ExprStmt(std::unique_ptr<Expr> expr)
      : Stmt(ASTKind::ExprStmt), expr_(std::move(expr)) {}
  [[nodiscard]] const Expr* expression() const { return expr_.get(); }

private:
  std::unique_ptr<Expr> expr_;
};

class AssignStmt : public Stmt {
public:
  AssignStmt(std::string target, std::unique_ptr<Expr> val)
      : Stmt(ASTKind::AssignStmt), targetName_(std::move(target)),
        value_(std::move(val)) {}
  [[nodiscard]] const std::string& targetName() const { return targetName_; }
  [[nodiscard]] const Expr* value() const { return value_.get(); }

private:
  std::string targetName_;
  std::unique_ptr<Expr> value_;
};

class DeclStmt : public Stmt {
public:
  explicit DeclStmt(std::unique_ptr<Decl> decl)
      : Stmt(ASTKind::DeclStmt), decl_(std::move(decl)) {}
  [[nodiscard]] const Decl* declaration() const { return decl_.get(); }
  [[nodiscard]] std::unique_ptr<Decl> takeDeclaration() { return std::move(decl_); }

private:
  std::unique_ptr<Decl> decl_;
};

class IfStmt : public Stmt {
public:
  IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> then,
         std::unique_ptr<Stmt> els)
      : Stmt(ASTKind::IfStmt), cond_(std::move(cond)),
        then_(std::move(then)), else_(std::move(els)) {}
  [[nodiscard]] const Expr* condition() const { return cond_.get(); }
  [[nodiscard]] const Stmt* thenBranch() const { return then_.get(); }
  [[nodiscard]] const Stmt* elseBranch() const { return else_.get(); }

private:
  std::unique_ptr<Expr> cond_;
  std::unique_ptr<Stmt> then_;
  std::unique_ptr<Stmt> else_;  // may be nullptr
};

class WhileStmt : public Stmt {
public:
  WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body)
      : Stmt(ASTKind::WhileStmt), cond_(std::move(cond)), body_(std::move(body)) {}
  [[nodiscard]] const Expr* condition() const { return cond_.get(); }
  [[nodiscard]] const Stmt* body() const { return body_.get(); }

private:
  std::unique_ptr<Expr> cond_;
  std::unique_ptr<Stmt> body_;
};

class BreakStmt : public Stmt {
public:
  BreakStmt() : Stmt(ASTKind::BreakStmt) {}
};

class ContinueStmt : public Stmt {
public:
  ContinueStmt() : Stmt(ASTKind::ContinueStmt) {}
};

class ReturnStmt : public Stmt {
public:
  ReturnStmt() : Stmt(ASTKind::ReturnStmt) {}
  explicit ReturnStmt(std::unique_ptr<Expr> val)
      : Stmt(ASTKind::ReturnStmt), value_(std::move(val)) {}
  [[nodiscard]] const Expr* value() const { return value_.get(); }

private:
  std::unique_ptr<Expr> value_;  // may be nullptr for void return
};

// ═══════════════════════════════════════════════════════════════════════════
// Top-level nodes
// ═══════════════════════════════════════════════════════════════════════════

class TopLevelItem : public ASTNode {
public:
  using ASTNode::ASTNode;
};

class GlobalDecl : public TopLevelItem {
public:
  explicit GlobalDecl(std::unique_ptr<Decl> decl)
      : TopLevelItem(ASTKind::GlobalDecl), decl_(std::move(decl)) {}
  [[nodiscard]] const Decl* declaration() const { return decl_.get(); }

private:
  std::unique_ptr<Decl> decl_;
};

class FuncDef : public TopLevelItem {
public:
  FuncDef(TypeKind ret, std::string name, std::vector<ParamDecl> params,
          std::unique_ptr<BlockStmt> body)
      : TopLevelItem(ASTKind::FuncDef), returnType_(ret), name_(std::move(name)),
        params_(std::move(params)), body_(std::move(body)) {}
  [[nodiscard]] TypeKind returnType() const { return returnType_; }
  [[nodiscard]] const std::string& name() const { return name_; }
  [[nodiscard]] const std::vector<ParamDecl>& params() const { return params_; }
  [[nodiscard]] const BlockStmt* body() const { return body_.get(); }

private:
  TypeKind returnType_;
  std::string name_;
  std::vector<ParamDecl> params_;
  std::unique_ptr<BlockStmt> body_;
};

class CompUnit : public ASTNode {
public:
  CompUnit() : ASTNode(ASTKind::CompUnit) {}
  void addItem(std::unique_ptr<TopLevelItem> item) { items_.push_back(std::move(item)); }
  [[nodiscard]] const std::vector<std::unique_ptr<TopLevelItem>>& items() const { return items_; }

private:
  std::vector<std::unique_ptr<TopLevelItem>> items_;
};

// ═══════════════════════════════════════════════════════════════════════════
// Visitor interface
// ═══════════════════════════════════════════════════════════════════════════

class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  virtual void visitCompUnit(const CompUnit& node) = 0;
  virtual void visitGlobalDecl(const GlobalDecl& node) = 0;
  virtual void visitFuncDef(const FuncDef& node) = 0;
  virtual void visitConstDecl(const ConstDecl& node) = 0;
  virtual void visitVarDecl(const VarDecl& node) = 0;
  virtual void visitParamDecl(const ParamDecl& node) = 0;
  virtual void visitBlockStmt(const BlockStmt& node) = 0;
  virtual void visitEmptyStmt(const EmptyStmt& node) = 0;
  virtual void visitExprStmt(const ExprStmt& node) = 0;
  virtual void visitAssignStmt(const AssignStmt& node) = 0;
  virtual void visitDeclStmt(const DeclStmt& node) = 0;
  virtual void visitIfStmt(const IfStmt& node) = 0;
  virtual void visitWhileStmt(const WhileStmt& node) = 0;
  virtual void visitBreakStmt(const BreakStmt& node) = 0;
  virtual void visitContinueStmt(const ContinueStmt& node) = 0;
  virtual void visitReturnStmt(const ReturnStmt& node) = 0;
  virtual void visitIntegerLiteralExpr(const IntegerLiteralExpr& node) = 0;
  virtual void visitIdentifierExpr(const IdentifierExpr& node) = 0;
  virtual void visitUnaryExpr(const UnaryExpr& node) = 0;
  virtual void visitBinaryExpr(const BinaryExpr& node) = 0;
  virtual void visitCallExpr(const CallExpr& node) = 0;
};

/// Dispatch to the correct visitor method based on node kind.
void accept(const ASTNode& node, ASTVisitor& visitor);

// ═══════════════════════════════════════════════════════════════════════════
// Dump function (declared here, implemented in ast_printer.cpp)
// ═══════════════════════════════════════════════════════════════════════════

class CompUnit;
void dumpAst(const CompUnit& unit, std::ostream& output);

} // namespace toyc
