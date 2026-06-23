#pragma once

#include "sema/semantic_model.h"
#include "sema/scope.h"
#include "sema/type.h"

#include <memory>
#include <vector>

namespace toyc::ast {
struct CompUnit;
struct Decl;
struct Stmt;
struct Expr;
struct BlockStmt;
struct EmptyStmt;
struct ExprStmt;
struct AssignStmt;
struct DeclStmt;
struct IfStmt;
struct WhileStmt;
struct BreakStmt;
struct ContinueStmt;
struct ReturnStmt;
struct IntLiteralExpr;
struct DeclRefExpr;
struct CallExpr;
struct UnaryExpr;
struct BinaryExpr;
struct VarDecl;
struct ConstDecl;
struct Param;
struct FuncDef;
} // namespace toyc::ast

namespace toyc::sema {

struct SemaResult {
    SemanticModel model;
    std::vector<Diagnostic> diagnostics;
};

class Sema {
public:
    SemaResult analyze(const ast::CompUnit& unit);

private:
    // --- Scope management ---
    Scope& pushScope(Scope* parent);
    void popScope();

    // --- Diagnostic helpers ---
    void error(SourceRange range, std::string message);
    void warn(SourceRange range, std::string message);

    // --- Declaration visitors ---
    void analyzeDecl(const ast::Decl& decl);
    void analyzeGlobalVarDecl(const ast::VarDecl& decl);
    void analyzeGlobalConstDecl(const ast::ConstDecl& decl);
    void analyzeFuncDef(const ast::FuncDef& funcDef);
    void analyzeLocalVarDecl(const ast::VarDecl& decl);
    void analyzeLocalConstDecl(const ast::ConstDecl& decl);
    void analyzeParam(const ast::Param& param);

    // --- Statement visitors ---
    void analyzeStmt(const ast::Stmt& stmt);
    void analyzeBlockStmt(const ast::BlockStmt& block);
    void analyzeEmptyStmt(const ast::EmptyStmt& stmt);
    void analyzeExprStmt(const ast::ExprStmt& stmt);
    void analyzeAssignStmt(const ast::AssignStmt& stmt);
    void analyzeDeclStmt(const ast::DeclStmt& stmt);
    void analyzeIfStmt(const ast::IfStmt& stmt);
    void analyzeWhileStmt(const ast::WhileStmt& stmt);
    void analyzeBreakStmt(const ast::BreakStmt& stmt);
    void analyzeContinueStmt(const ast::ContinueStmt& stmt);
    void analyzeReturnStmt(const ast::ReturnStmt& stmt);

    // --- Expression visitors (return the expression's Type) ---
    Type analyzeExpr(const ast::Expr& expr);
    Type analyzeIntLiteralExpr(const ast::IntLiteralExpr& expr);
    Type analyzeDeclRefExpr(const ast::DeclRefExpr& expr);
    Type analyzeCallExpr(const ast::CallExpr& expr);
    Type analyzeUnaryExpr(const ast::UnaryExpr& expr);
    Type analyzeBinaryExpr(const ast::BinaryExpr& expr);

    // --- Post-pass validation ---
    void finalizeMainCheck();

    // --- Traversal state ---
    SemanticModel* model_ = nullptr;
    std::vector<Diagnostic>* diags_ = nullptr;
    Scope* currentScope_ = nullptr;
    int loopDepth_ = 0;
    Type currentReturnType_ = Type::Void;
    bool seenMain_ = false;

    // --- Owned scopes (stable pointers) ---
    std::vector<std::unique_ptr<Scope>> ownedScopes_;
};

} // namespace toyc::sema
