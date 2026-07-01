#pragma once

#include "token.h"
#include "toyc/ir/type.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// =============================================================================
// AST — Toy-C P2-B1
// Supports: multi-function, ≤8 params, call expressions
// =============================================================================

enum class UnaryOp : uint8_t { Plus, Minus, Not };
enum class BinaryOp : uint8_t {
    Add, Sub, Mul, Div, Mod,
    Lt, Gt, Le, Ge, Eq, Ne,
    And, Or
};

struct Expr { virtual ~Expr() = default; };

struct RawIntLiteralExpr final : Expr {
    uint64_t magnitude = 0;
    explicit RawIntLiteralExpr(uint64_t m) : magnitude(m) {}
};
struct IntLiteralExpr final : Expr {
    int32_t value = 0;
    explicit IntLiteralExpr(int32_t v) : value(v) {}
};
struct IdentifierExpr final : Expr {
    std::string name;
    SourceLocation location;
    IdentifierExpr(std::string n, SourceLocation l) : name(std::move(n)), location(l) {}
};
struct UnaryExpr final : Expr {
    UnaryOp op;
    std::unique_ptr<Expr> operand;
    UnaryExpr(UnaryOp o, std::unique_ptr<Expr> opnd) : op(o), operand(std::move(opnd)) {}
};
struct BinaryExpr final : Expr {
    BinaryOp op;
    std::unique_ptr<Expr> left, right;
    BinaryExpr(BinaryOp o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
};
struct CallExpr final : Expr {
    Type return_type;
    CallExpr(std::string c, std::vector<std::unique_ptr<Expr>> a, Type rt = Type::Int)
        : callee(std::move(c)), args(std::move(a)), return_type(rt) {}
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
};

// --- Statements ---
struct Stmt { virtual ~Stmt() = default; };
struct CompoundStmt final : Stmt { std::vector<std::unique_ptr<Stmt>> statements; };
struct VarDeclStmt final : Stmt {
    std::string name;
    std::unique_ptr<Expr> initializer;
    SourceLocation location;
    bool isConst = false;
};
struct AssignStmt final : Stmt { std::string name; std::unique_ptr<Expr> value; };
struct IfStmt final : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenStmt, elseStmt;
};
struct WhileStmt final : Stmt { std::unique_ptr<Expr> condition; std::unique_ptr<Stmt> body; };
struct BreakStmt final : Stmt {};
struct ContinueStmt final : Stmt {};
struct ReturnStmt final : Stmt {
    std::unique_ptr<Expr> value;
    explicit ReturnStmt(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};
struct ExprStmt final : Stmt { std::unique_ptr<Expr> expr; explicit ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {} };

// --- Global ---
struct GlobalVarDecl {
    std::string name;
    std::unique_ptr<Expr> initializer; // always present per Java oracle
    SourceLocation location;
    bool isConst = false;
    GlobalVarDecl(std::string n, std::unique_ptr<Expr> i, SourceLocation l, bool c = false)
        : name(std::move(n)), initializer(std::move(i)), location(l), isConst(c) {}
};

// --- Function ---
struct Param {
    std::string name;
};

struct FunctionDef {
    SourceLocation location;
    Type returnType;
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<CompoundStmt> body;
    FunctionDef(Type rt, std::string n, std::vector<Param> p, std::unique_ptr<CompoundStmt> b)
        : returnType(rt), name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
};

struct Program {
    std::vector<GlobalVarDecl> globals;
    std::vector<FunctionDef> functions;
};
