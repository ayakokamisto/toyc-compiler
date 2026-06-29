#pragma once

#include "common/diagnostic.h"
#include "common/token.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace toyc::ast {

struct Node;
struct Decl;
struct Stmt;
struct Expr;
struct BlockStmt;

using DeclPtr = std::unique_ptr<Decl>;
using StmtPtr = std::unique_ptr<Stmt>;
using ExprPtr = std::unique_ptr<Expr>;

struct Node {
    SourceRange range{};
    virtual ~Node() = default;
};

struct Decl : Node {};
struct Stmt : Node {};
struct Expr : Node {};

enum class TypeKind {
    Int,
    Void,
};

struct IntLiteralExpr final : Expr {
    std::string spelling;
};

struct DeclRefExpr final : Expr {
    std::string name;
};

struct CallExpr final : Expr {
    std::string callee;
    std::vector<ExprPtr> arguments;
};

struct UnaryExpr final : Expr {
    TokenKind op = TokenKind::Plus;
    ExprPtr operand;
};

struct BinaryExpr final : Expr {
    TokenKind op = TokenKind::Plus;
    ExprPtr left;
    ExprPtr right;
};

struct BlockStmt final : Stmt {
    std::vector<StmtPtr> statements;
};

struct EmptyStmt final : Stmt {};

struct ExprStmt final : Stmt {
    ExprPtr expression;
};

struct AssignStmt final : Stmt {
    std::string target;
    SourceRange targetRange{};
    ExprPtr value;
};

struct DeclStmt final : Stmt {
    DeclPtr declaration;
};

struct IfStmt final : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
};

struct WhileStmt final : Stmt {
    ExprPtr condition;
    StmtPtr body;
};

struct BreakStmt final : Stmt {};
struct ContinueStmt final : Stmt {};

struct ReturnStmt final : Stmt {
    ExprPtr value;
};

struct VarDecl final : Decl {
    std::string name;
    ExprPtr initializer;
};

struct ConstDecl final : Decl {
    std::string name;
    ExprPtr initializer;
};

struct Param final : Decl {
    std::string name;
};

struct FuncDef final : Decl {
    TypeKind returnType = TypeKind::Void;
    std::string name;
    std::vector<std::unique_ptr<Param>> parameters;
    std::unique_ptr<BlockStmt> body;
};

struct CompUnit final : Node {
    std::vector<DeclPtr> declarations;
};

} // namespace toyc::ast
