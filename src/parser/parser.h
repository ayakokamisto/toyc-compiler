#pragma once

#include "ast/ast.h"
#include "common/diagnostic.h"
#include "common/token_stream.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace toyc::parser {

class Parser {
public:
    explicit Parser(TokenStream& tokens);

    std::unique_ptr<ast::CompUnit> parseCompUnit();
    std::unique_ptr<ast::Expr> parseExpr();

    [[nodiscard]] bool hasError() const noexcept;
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;

private:
    ast::DeclPtr parseTopLevelItem();
    ast::DeclPtr parseDecl();
    std::unique_ptr<ast::VarDecl> parseVarDecl();
    std::unique_ptr<ast::ConstDecl> parseConstDecl();
    std::unique_ptr<ast::FuncDef> parseFuncDef(ast::TypeKind returnType);
    std::unique_ptr<ast::Param> parseParam();

    ast::StmtPtr parseStmt();
    std::unique_ptr<ast::BlockStmt> parseBlockStmt();
    std::unique_ptr<ast::ReturnStmt> parseReturnStmt();
    std::unique_ptr<ast::IfStmt> parseIfStmt();
    std::unique_ptr<ast::WhileStmt> parseWhileStmt();

    ast::ExprPtr parseLogicalOrExpr();
    ast::ExprPtr parseLogicalAndExpr();
    ast::ExprPtr parseRelationalExpr();
    ast::ExprPtr parseAdditiveExpr();
    ast::ExprPtr parseMultiplicativeExpr();
    ast::ExprPtr parseUnaryExpr();
    ast::ExprPtr parsePrimaryExpr();
    ast::ExprPtr parseCallExpr(std::string callee, SourceRange calleeRange);

    void reportError(SourceRange range, std::string message);
    void synchronizeStatement();
    void synchronizeTopLevel();

    TokenStream& tokens_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace toyc::parser
