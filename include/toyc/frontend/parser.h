#pragma once

#include "ast.h"
#include "lexer.h"
#include "token.h"

#include <memory>
#include <stdexcept>
#include <vector>

class ParseError : public std::runtime_error {
public:
    int line, column;
    ParseError(int line, int column, const std::string& msg);
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    Program parse_program();
private:
    const Token& peek(size_t offset = 0) const;
    const Token& consume();
    bool match(TokenKind k);
    const Token& expect(TokenKind k, const char* msg);
    void error(const std::string& msg);

    // Top-level
    FunctionDef parse_function_def();
    std::vector<Param> parse_params();

    // Statements
    std::unique_ptr<Stmt> parse_statement();
    std::unique_ptr<Stmt> parse_declaration();
    std::unique_ptr<Stmt> parse_if();
    std::unique_ptr<Stmt> parse_while();
    std::unique_ptr<Stmt> parse_break();
    std::unique_ptr<Stmt> parse_continue();
    std::unique_ptr<Stmt> parse_return();
    std::unique_ptr<Stmt> parse_assignment();
    std::unique_ptr<Stmt> parse_expression_statement();
    std::unique_ptr<CompoundStmt> parse_compound_statement();

    // Expressions
    std::unique_ptr<Expr> parse_expr();
    std::unique_ptr<Expr> parse_logical_or();
    std::unique_ptr<Expr> parse_logical_and();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_relational();
    std::unique_ptr<Expr> parse_additive();
    std::unique_ptr<Expr> parse_multiplicative();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_primary();
    std::unique_ptr<Expr> parse_call_suffix(std::string callee, SourceLocation loc);

    // Raw literal resolution
    std::unique_ptr<Expr> resolve_unary_minus(std::unique_ptr<Expr> operand);
    std::unique_ptr<Expr> resolve_unary_plus(std::unique_ptr<Expr> operand);
    std::unique_ptr<Expr> resolve_bare_raw_literal(std::unique_ptr<Expr> primary);
    size_t pos_ = 0;
    const std::vector<Token>& tokens_;
};
