#include "parser/parser.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace toyc::parser {
namespace {

SourceLocation tokenEnd(const Token& token) {
    return SourceLocation{token.location.line,
                          token.location.column + static_cast<int>(token.lexeme.size())};
}

SourceRange tokenRange(const Token& token) {
    return SourceRange{token.location, tokenEnd(token)};
}

bool startsTopLevelItem(TokenKind kind) {
    return kind == TokenKind::KwConst || kind == TokenKind::KwInt ||
           kind == TokenKind::KwVoid;
}

bool startsStatement(TokenKind kind) {
    return kind == TokenKind::LBrace || kind == TokenKind::KwConst ||
           kind == TokenKind::KwInt || kind == TokenKind::KwIf ||
           kind == TokenKind::KwWhile || kind == TokenKind::KwBreak ||
           kind == TokenKind::KwContinue || kind == TokenKind::KwReturn ||
           kind == TokenKind::Identifier || kind == TokenKind::IntegerLiteral ||
           kind == TokenKind::LParen || kind == TokenKind::Plus ||
           kind == TokenKind::Minus || kind == TokenKind::Bang;
}

ast::ExprPtr makeBinaryExpr(TokenKind op, ast::ExprPtr left, ast::ExprPtr right) {
    auto expression = std::make_unique<ast::BinaryExpr>();
    expression->range = SourceRange{left->range.begin, right->range.end};
    expression->op = op;
    expression->left = std::move(left);
    expression->right = std::move(right);
    return expression;
}

} // namespace

Parser::Parser(TokenStream& tokens) : tokens_(tokens) {}

std::unique_ptr<ast::CompUnit> Parser::parseCompUnit() {
    auto unit = std::make_unique<ast::CompUnit>();
    unit->range.begin = tokens_.peek().location;

    if (tokens_.peek().kind == TokenKind::Eof) {
        reportError(tokenRange(tokens_.peek()), "expected at least one top-level declaration");
        unit->range.end = tokens_.peek().location;
        return unit;
    }

    while (tokens_.peek().kind != TokenKind::Eof) {
        const Token* const start = &tokens_.peek();
        try {
            unit->declarations.push_back(parseTopLevelItem());
        } catch (const ParseError& error) {
            reportError(tokenRange(tokens_.peek()), error.what());
            if (&tokens_.peek() == start && tokens_.peek().kind != TokenKind::Eof) {
                (void)tokens_.consume();
            }
            synchronizeTopLevel();
        }
    }

    unit->range.end = tokens_.peek().location;
    return unit;
}

std::unique_ptr<ast::Expr> Parser::parseExpr() {
    try {
        return parseLogicalOrExpr();
    } catch (const ParseError& error) {
        reportError(tokenRange(tokens_.peek()), error.what());
        return nullptr;
    }
}

bool Parser::hasError() const noexcept {
    return std::any_of(diagnostics_.begin(), diagnostics_.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Error;
    });
}

const std::vector<Diagnostic>& Parser::diagnostics() const noexcept {
    return diagnostics_;
}

ast::DeclPtr Parser::parseTopLevelItem() {
    if (tokens_.peek().kind == TokenKind::KwConst) {
        return parseConstDecl();
    }

    if (tokens_.peek().kind == TokenKind::KwInt) {
        if (tokens_.peek(1).kind == TokenKind::Identifier &&
            tokens_.peek(2).kind == TokenKind::LParen) {
            return parseFuncDef(ast::TypeKind::Int);
        }
        return parseVarDecl();
    }

    if (tokens_.peek().kind == TokenKind::KwVoid) {
        return parseFuncDef(ast::TypeKind::Void);
    }

    (void)tokens_.expect(TokenKind::KwInt, "expected top-level declaration or function");
    return nullptr;
}

ast::DeclPtr Parser::parseDecl() {
    if (tokens_.peek().kind == TokenKind::KwConst) {
        return parseConstDecl();
    }
    return parseVarDecl();
}

std::unique_ptr<ast::VarDecl> Parser::parseVarDecl() {
    const Token& begin = tokens_.expect(TokenKind::KwInt, "expected 'int' in variable declaration");
    const Token& name = tokens_.expect(TokenKind::Identifier, "expected variable name");
    (void)tokens_.expect(TokenKind::Equal, "expected '=' in variable declaration");
    ast::ExprPtr initializer = parseLogicalOrExpr();
    const Token& end = tokens_.expect(TokenKind::Semicolon, "expected ';' after variable declaration");

    auto declaration = std::make_unique<ast::VarDecl>();
    declaration->range = SourceRange{begin.location, tokenEnd(end)};
    declaration->name = name.lexeme;
    declaration->initializer = std::move(initializer);
    return declaration;
}

std::unique_ptr<ast::ConstDecl> Parser::parseConstDecl() {
    const Token& begin = tokens_.expect(TokenKind::KwConst, "expected 'const'");
    (void)tokens_.expect(TokenKind::KwInt, "expected 'int' after 'const'");
    const Token& name = tokens_.expect(TokenKind::Identifier, "expected constant name");
    (void)tokens_.expect(TokenKind::Equal, "expected '=' in constant declaration");
    ast::ExprPtr initializer = parseLogicalOrExpr();
    const Token& end = tokens_.expect(TokenKind::Semicolon, "expected ';' after constant declaration");

    auto declaration = std::make_unique<ast::ConstDecl>();
    declaration->range = SourceRange{begin.location, tokenEnd(end)};
    declaration->name = name.lexeme;
    declaration->initializer = std::move(initializer);
    return declaration;
}

std::unique_ptr<ast::FuncDef> Parser::parseFuncDef(ast::TypeKind returnType) {
    const TokenKind returnTokenKind =
        returnType == ast::TypeKind::Int ? TokenKind::KwInt : TokenKind::KwVoid;
    const Token& begin = tokens_.expect(returnTokenKind, "expected function return type");
    const Token& name = tokens_.expect(TokenKind::Identifier, "expected function name");
    (void)tokens_.expect(TokenKind::LParen, "expected '(' after function name");

    std::vector<std::unique_ptr<ast::Param>> parameters;
    if (tokens_.peek().kind != TokenKind::RParen) {
        parameters.push_back(parseParam());
        while (tokens_.match(TokenKind::Comma)) {
            parameters.push_back(parseParam());
        }
    }

    (void)tokens_.expect(TokenKind::RParen, "expected ')' after parameter list");
    std::unique_ptr<ast::BlockStmt> body = parseBlockStmt();

    auto function = std::make_unique<ast::FuncDef>();
    function->range = SourceRange{begin.location, body->range.end};
    function->returnType = returnType;
    function->name = name.lexeme;
    function->parameters = std::move(parameters);
    function->body = std::move(body);
    return function;
}

std::unique_ptr<ast::Param> Parser::parseParam() {
    const Token& begin = tokens_.expect(TokenKind::KwInt, "expected 'int' in parameter");
    const Token& name = tokens_.expect(TokenKind::Identifier, "expected parameter name");

    auto parameter = std::make_unique<ast::Param>();
    parameter->range = SourceRange{begin.location, tokenEnd(name)};
    parameter->name = name.lexeme;
    return parameter;
}

ast::StmtPtr Parser::parseStmt() {
    if (tokens_.peek().kind == TokenKind::LBrace) {
        return parseBlockStmt();
    }

    if (tokens_.peek().kind == TokenKind::KwConst ||
        tokens_.peek().kind == TokenKind::KwInt) {
        ast::DeclPtr declaration = parseDecl();
        auto statement = std::make_unique<ast::DeclStmt>();
        statement->range = declaration->range;
        statement->declaration = std::move(declaration);
        return statement;
    }

    if (tokens_.peek().kind == TokenKind::KwIf) {
        return parseIfStmt();
    }

    if (tokens_.peek().kind == TokenKind::KwWhile) {
        return parseWhileStmt();
    }

    if (tokens_.peek().kind == TokenKind::KwBreak) {
        return parseBreakStmt();
    }

    if (tokens_.peek().kind == TokenKind::KwContinue) {
        return parseContinueStmt();
    }

    if (tokens_.peek().kind == TokenKind::KwReturn) {
        return parseReturnStmt();
    }

    if (tokens_.peek().kind == TokenKind::Semicolon) {
        const Token& semicolon = tokens_.consume();
        auto statement = std::make_unique<ast::EmptyStmt>();
        statement->range = tokenRange(semicolon);
        return statement;
    }

    if (tokens_.peek().kind == TokenKind::Identifier &&
        tokens_.peek(1).kind == TokenKind::Equal) {
        return parseAssignStmt();
    }

    ast::ExprPtr expression = parseLogicalOrExpr();
    const SourceLocation begin = expression->range.begin;
    const Token& semicolon =
        tokens_.expect(TokenKind::Semicolon, "expected ';' after expression statement");
    auto statement = std::make_unique<ast::ExprStmt>();
    statement->range = SourceRange{begin, tokenEnd(semicolon)};
    statement->expression = std::move(expression);
    return statement;
}

std::unique_ptr<ast::BlockStmt> Parser::parseBlockStmt() {
    const Token& begin = tokens_.expect(TokenKind::LBrace, "expected '{' to start block");
    auto block = std::make_unique<ast::BlockStmt>();

    while (tokens_.peek().kind != TokenKind::RBrace &&
           tokens_.peek().kind != TokenKind::Eof) {
        const Token* const start = &tokens_.peek();
        try {
            block->statements.push_back(parseStmt());
        } catch (const ParseError& error) {
            reportError(tokenRange(tokens_.peek()), error.what());
            if (&tokens_.peek() == start && tokens_.peek().kind != TokenKind::Eof) {
                (void)tokens_.consume();
            }
            synchronizeStatement();
        }
    }

    const Token& end = tokens_.expect(TokenKind::RBrace, "expected '}' to close block");
    block->range = SourceRange{begin.location, tokenEnd(end)};
    return block;
}

std::unique_ptr<ast::AssignStmt> Parser::parseAssignStmt() {
    const Token& target = tokens_.expect(TokenKind::Identifier, "expected assignment target");
    (void)tokens_.expect(TokenKind::Equal, "expected '=' after assignment target");
    ast::ExprPtr value = parseLogicalOrExpr();
    const Token& end = tokens_.expect(TokenKind::Semicolon, "expected ';' after assignment");

    auto statement = std::make_unique<ast::AssignStmt>();
    statement->range = SourceRange{target.location, tokenEnd(end)};
    statement->target = target.lexeme;
    statement->targetRange = tokenRange(target);
    statement->value = std::move(value);
    return statement;
}

std::unique_ptr<ast::ReturnStmt> Parser::parseReturnStmt() {
    const Token& begin = tokens_.expect(TokenKind::KwReturn, "expected 'return'");
    ast::ExprPtr value;
    if (tokens_.peek().kind != TokenKind::Semicolon) {
        value = parseLogicalOrExpr();
    }
    const Token& end = tokens_.expect(TokenKind::Semicolon, "expected ';' after return");

    auto statement = std::make_unique<ast::ReturnStmt>();
    statement->range = SourceRange{begin.location, tokenEnd(end)};
    statement->value = std::move(value);
    return statement;
}

std::unique_ptr<ast::IfStmt> Parser::parseIfStmt() {
    const Token& begin = tokens_.expect(TokenKind::KwIf, "expected 'if'");
    (void)tokens_.expect(TokenKind::LParen, "expected '(' after 'if'");
    ast::ExprPtr condition = parseLogicalOrExpr();
    (void)tokens_.expect(TokenKind::RParen, "expected ')' after if condition");
    ast::StmtPtr thenBranch = parseStmt();
    ast::StmtPtr elseBranch;
    if (tokens_.match(TokenKind::KwElse)) {
        elseBranch = parseStmt();
    }

    auto statement = std::make_unique<ast::IfStmt>();
    statement->range = SourceRange{
        begin.location, elseBranch != nullptr ? elseBranch->range.end : thenBranch->range.end};
    statement->condition = std::move(condition);
    statement->thenBranch = std::move(thenBranch);
    statement->elseBranch = std::move(elseBranch);
    return statement;
}

std::unique_ptr<ast::WhileStmt> Parser::parseWhileStmt() {
    const Token& begin = tokens_.expect(TokenKind::KwWhile, "expected 'while'");
    (void)tokens_.expect(TokenKind::LParen, "expected '(' after 'while'");
    ast::ExprPtr condition = parseLogicalOrExpr();
    (void)tokens_.expect(TokenKind::RParen, "expected ')' after while condition");
    ast::StmtPtr body = parseStmt();

    auto statement = std::make_unique<ast::WhileStmt>();
    statement->range = SourceRange{begin.location, body->range.end};
    statement->condition = std::move(condition);
    statement->body = std::move(body);
    return statement;
}

std::unique_ptr<ast::BreakStmt> Parser::parseBreakStmt() {
    const Token& begin = tokens_.expect(TokenKind::KwBreak, "expected 'break'");
    const Token& end = tokens_.expect(TokenKind::Semicolon, "expected ';' after break");
    auto statement = std::make_unique<ast::BreakStmt>();
    statement->range = SourceRange{begin.location, tokenEnd(end)};
    return statement;
}

std::unique_ptr<ast::ContinueStmt> Parser::parseContinueStmt() {
    const Token& begin = tokens_.expect(TokenKind::KwContinue, "expected 'continue'");
    const Token& end = tokens_.expect(TokenKind::Semicolon, "expected ';' after continue");
    auto statement = std::make_unique<ast::ContinueStmt>();
    statement->range = SourceRange{begin.location, tokenEnd(end)};
    return statement;
}

ast::ExprPtr Parser::parseLogicalOrExpr() {
    ast::ExprPtr expression = parseLogicalAndExpr();
    while (tokens_.peek().kind == TokenKind::PipePipe) {
        const TokenKind op = tokens_.consume().kind;
        expression = makeBinaryExpr(op, std::move(expression), parseLogicalAndExpr());
    }
    return expression;
}

ast::ExprPtr Parser::parseLogicalAndExpr() {
    ast::ExprPtr expression = parseEqualityExpr();
    while (tokens_.peek().kind == TokenKind::AmpAmp) {
        const TokenKind op = tokens_.consume().kind;
        expression = makeBinaryExpr(op, std::move(expression), parseEqualityExpr());
    }
    return expression;
}

std::unique_ptr<ast::Expr> Parser::parseEqualityExpr() {
    ast::ExprPtr expression = parseRelationalExpr();
    while (tokens_.peek().kind == TokenKind::EqualEqual ||
           tokens_.peek().kind == TokenKind::BangEqual) {
        const TokenKind op = tokens_.consume().kind;
        expression = makeBinaryExpr(op, std::move(expression), parseRelationalExpr());
    }
    return expression;
}

ast::ExprPtr Parser::parseRelationalExpr() {
    ast::ExprPtr expression = parseAdditiveExpr();
    while (tokens_.peek().kind == TokenKind::Less ||
           tokens_.peek().kind == TokenKind::LessEqual ||
           tokens_.peek().kind == TokenKind::Greater ||
           tokens_.peek().kind == TokenKind::GreaterEqual) {
        const TokenKind op = tokens_.consume().kind;
        expression = makeBinaryExpr(op, std::move(expression), parseAdditiveExpr());
    }
    return expression;
}

ast::ExprPtr Parser::parseAdditiveExpr() {
    ast::ExprPtr expression = parseMultiplicativeExpr();
    while (tokens_.peek().kind == TokenKind::Plus ||
           tokens_.peek().kind == TokenKind::Minus) {
        const TokenKind op = tokens_.consume().kind;
        expression = makeBinaryExpr(op, std::move(expression), parseMultiplicativeExpr());
    }
    return expression;
}

ast::ExprPtr Parser::parseMultiplicativeExpr() {
    ast::ExprPtr expression = parseUnaryExpr();
    while (tokens_.peek().kind == TokenKind::Star ||
           tokens_.peek().kind == TokenKind::Slash ||
           tokens_.peek().kind == TokenKind::Percent) {
        const TokenKind op = tokens_.consume().kind;
        expression = makeBinaryExpr(op, std::move(expression), parseUnaryExpr());
    }
    return expression;
}

ast::ExprPtr Parser::parseUnaryExpr() {
    if (tokens_.peek().kind == TokenKind::Plus ||
        tokens_.peek().kind == TokenKind::Minus ||
        tokens_.peek().kind == TokenKind::Bang) {
        const Token& op = tokens_.consume();
        ast::ExprPtr operand = parseUnaryExpr();
        auto expression = std::make_unique<ast::UnaryExpr>();
        expression->range = SourceRange{op.location, operand->range.end};
        expression->op = op.kind;
        expression->operand = std::move(operand);
        return expression;
    }
    return parsePrimaryExpr();
}

ast::ExprPtr Parser::parsePrimaryExpr() {
    if (tokens_.peek().kind == TokenKind::IntegerLiteral) {
        const Token& token = tokens_.consume();
        auto expression = std::make_unique<ast::IntLiteralExpr>();
        expression->range = tokenRange(token);
        expression->spelling = token.lexeme;
        return expression;
    }

    if (tokens_.peek().kind == TokenKind::Identifier) {
        const Token& token = tokens_.consume();
        if (tokens_.peek().kind == TokenKind::LParen) {
            return parseCallExpr(token.lexeme, tokenRange(token));
        }
        auto expression = std::make_unique<ast::DeclRefExpr>();
        expression->range = tokenRange(token);
        expression->name = token.lexeme;
        return expression;
    }

    if (tokens_.peek().kind == TokenKind::LParen) {
        const Token& begin = tokens_.consume();
        ast::ExprPtr expression = parseLogicalOrExpr();
        const Token& end = tokens_.expect(TokenKind::RParen, "expected ')' after expression");
        expression->range = SourceRange{begin.location, tokenEnd(end)};
        return expression;
    }

    (void)tokens_.expect(TokenKind::IntegerLiteral, "expected integer, identifier, or '('");
    return nullptr;
}

ast::ExprPtr Parser::parseCallExpr(std::string callee, SourceRange calleeRange) {
    (void)tokens_.expect(TokenKind::LParen, "expected '(' after function name");

    std::vector<ast::ExprPtr> arguments;
    if (tokens_.peek().kind != TokenKind::RParen) {
        arguments.push_back(parseLogicalOrExpr());
        while (tokens_.match(TokenKind::Comma)) {
            arguments.push_back(parseLogicalOrExpr());
        }
    }

    const Token& end = tokens_.expect(TokenKind::RParen, "expected ')' after arguments");
    auto expression = std::make_unique<ast::CallExpr>();
    expression->range = SourceRange{calleeRange.begin, tokenEnd(end)};
    expression->callee = std::move(callee);
    expression->arguments = std::move(arguments);
    return expression;
}

void Parser::reportError(SourceRange range, std::string message) {
    diagnostics_.push_back(
        Diagnostic{DiagnosticSeverity::Error, range, std::move(message)});
}

void Parser::synchronizeStatement() {
    while (tokens_.peek().kind != TokenKind::Eof &&
           tokens_.peek().kind != TokenKind::RBrace) {
        if (startsStatement(tokens_.peek().kind)) {
            return;
        }
        if (tokens_.consume().kind == TokenKind::Semicolon) {
            return;
        }
    }
}

void Parser::synchronizeTopLevel() {
    while (tokens_.peek().kind != TokenKind::Eof) {
        if (startsTopLevelItem(tokens_.peek().kind)) {
            return;
        }
        if (tokens_.consume().kind == TokenKind::Semicolon) {
            return;
        }
    }
}

} // namespace toyc::parser
