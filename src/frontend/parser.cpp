#include "toyc/frontend/parser.h"
#include <sstream>

ParseError::ParseError(int line, int column, const std::string& msg)
    : std::runtime_error(msg), line(line), column(column) {}

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

const Token& Parser::peek(size_t offset) const {
    size_t idx = pos_ + offset;
    return idx < tokens_.size() ? tokens_[idx] : tokens_.back();
}
const Token& Parser::consume() { return tokens_[pos_++]; }
bool Parser::match(TokenKind k) { if (peek().kind == k) { consume(); return true; } return false; }
const Token& Parser::expect(TokenKind k, const char* msg) {
    if (peek().kind != k) error(msg); return consume();
}
void Parser::error(const std::string& msg) {
    auto& t = peek();
    std::ostringstream os;
    os << t.location.line << ":" << t.location.column << ": error: " << msg;
    throw ParseError(t.location.line, t.location.column, os.str());
}

// ===========================================================================
// Program: (function_def)+
// ===========================================================================
Program Parser::parse_program() {
    Program prog;
    while (peek().kind == TokenKind::KwInt || peek().kind == TokenKind::KwVoid || peek().kind == TokenKind::KwConst) {
        TokenKind kw = peek().kind;
        if (kw == TokenKind::KwConst) {
            consume();
            expect(TokenKind::KwInt, "expected 'int' after 'const'");
            auto& nt = expect(TokenKind::Identifier, "expected name");
            std::string name = nt.lexeme;
            SourceLocation loc = nt.location;
            expect(TokenKind::Equal, "expected '=' after const global name");
            auto init = parse_expr();
            expect(TokenKind::Semicolon, "expected ';' after const global declaration");
            prog.globals.push_back(GlobalVarDecl(name, std::move(init), loc, true));
            continue;
        }
        // Peek past "int/void" and "name" to see what follows
        if (peek(0).kind == TokenKind::KwInt && peek(2).kind != TokenKind::LParen) {
            // int name; or int name = expr; -> global declaration
            consume(); // int
            auto& nt = expect(TokenKind::Identifier, "expected name");
            std::string name = nt.lexeme;
            SourceLocation loc = nt.location;
            if (peek().kind == TokenKind::Equal) {
                consume();
                auto init = parse_expr();
                expect(TokenKind::Semicolon, "expected ';' after global declaration");
                prog.globals.push_back(GlobalVarDecl(name, std::move(init), loc, false));
            } else if (peek().kind == TokenKind::Semicolon) {
                consume();
                // Parse for a stable semantic diagnostic aligned with the Java oracle.
                prog.globals.push_back(GlobalVarDecl(name, nullptr, loc, false));
            } else {
                error("expected '=' or ';' after global variable name");
            }
        } else if (kw == TokenKind::KwVoid && peek(2).kind != TokenKind::LParen) {
            // void name; -> error (void cannot be a global variable)
            consume(); // void
            auto& nt = expect(TokenKind::Identifier, "expected name");
            expect(TokenKind::Semicolon, "unexpected token");
            error("void variable declaration is not allowed");
        } else {
            // Function definition
            prog.functions.push_back(parse_function_def());
        }
    }
    if (prog.functions.empty() && prog.globals.empty()) error("expected top-level declaration");
    if (peek().kind != TokenKind::Eof) error("unexpected token");
    return prog;
}

FunctionDef Parser::parse_function_def() {
    Type rt = Type::Int;
    if (match(TokenKind::KwVoid)) rt = Type::Void;
    else expect(TokenKind::KwInt, "expected 'int' or 'void'");

    auto& name_tok = expect(TokenKind::Identifier, "expected function name");
    std::string name = name_tok.lexeme;

    expect(TokenKind::LParen, "expected '('");
    auto params = parse_params();
    expect(TokenKind::RParen, "expected ')'");

    auto body = parse_compound_statement();
    return FunctionDef(rt, std::move(name), std::move(params), std::move(body));
}

std::vector<Param> Parser::parse_params() {
    std::vector<Param> params;
    if (peek().kind == TokenKind::KwInt) {
        expect(TokenKind::KwInt, "expected 'int'");
        auto& t = expect(TokenKind::Identifier, "expected parameter name");
        params.push_back({t.lexeme});
        while (peek().kind == TokenKind::Comma) {
            consume();
            expect(TokenKind::KwInt, "expected 'int'");
            auto& t2 = expect(TokenKind::Identifier, "expected parameter name");
            params.push_back({t2.lexeme});
        }
    }
    return params;
}

// ===========================================================================
// Statements
// ===========================================================================
std::unique_ptr<Stmt> Parser::parse_statement() {
    switch (peek().kind) {
    case TokenKind::KwInt:
    case TokenKind::KwConst:
        return parse_declaration();
    case TokenKind::KwIf: return parse_if();
    case TokenKind::KwWhile: return parse_while();
    case TokenKind::KwBreak: return parse_break();
    case TokenKind::KwContinue: return parse_continue();
    case TokenKind::KwReturn: return parse_return();
    case TokenKind::LBrace: return parse_compound_statement();
    default:
        if (peek().kind == TokenKind::Identifier && peek(1).kind == TokenKind::Equal)
            return parse_assignment();
        return parse_expression_statement();
    }
}

std::unique_ptr<Stmt> Parser::parse_declaration() {
    bool is_const = match(TokenKind::KwConst);
    if (is_const) {
        expect(TokenKind::KwInt, "expected 'int' after 'const'");
    } else {
        expect(TokenKind::KwInt, "expected 'int'");
    }
    auto& nt = expect(TokenKind::Identifier, "expected variable name");
    std::string name = nt.lexeme;
    SourceLocation loc = nt.location;
    expect(TokenKind::Equal, "expected '='");
    auto init = parse_expr();
    expect(TokenKind::Semicolon, "expected ';'");
    auto d = std::make_unique<VarDeclStmt>();
    d->name = std::move(name); d->initializer = std::move(init); d->location = loc; d->isConst = is_const;
    return d;
}

std::unique_ptr<Stmt> Parser::parse_if() {
    expect(TokenKind::KwIf, "expected 'if'");
    expect(TokenKind::LParen, "expected '('");
    auto cond = parse_expr();
    expect(TokenKind::RParen, "expected ')'");
    auto thenS = parse_statement();
    std::unique_ptr<Stmt> elseS;
    if (peek().kind == TokenKind::KwElse) { consume(); elseS = parse_statement(); }
    auto i = std::make_unique<IfStmt>();
    i->condition = std::move(cond); i->thenStmt = std::move(thenS); i->elseStmt = std::move(elseS);
    return i;
}

std::unique_ptr<Stmt> Parser::parse_while() {
    expect(TokenKind::KwWhile, "expected 'while'");
    expect(TokenKind::LParen, "expected '('");
    auto cond = parse_expr();
    expect(TokenKind::RParen, "expected ')'");
    auto body = parse_statement();
    auto w = std::make_unique<WhileStmt>();
    w->condition = std::move(cond); w->body = std::move(body);
    return w;
}

std::unique_ptr<Stmt> Parser::parse_break() {
    expect(TokenKind::KwBreak, "expected 'break'");
    expect(TokenKind::Semicolon, "expected ';' after break");
    return std::make_unique<BreakStmt>();
}

std::unique_ptr<Stmt> Parser::parse_continue() {
    expect(TokenKind::KwContinue, "expected 'continue'");
    expect(TokenKind::Semicolon, "expected ';' after continue");
    return std::make_unique<ContinueStmt>();
}

std::unique_ptr<Stmt> Parser::parse_return() {
    expect(TokenKind::KwReturn, "expected 'return'");
    if (peek().kind == TokenKind::Semicolon) { consume(); return std::make_unique<ReturnStmt>(nullptr); }
    auto expr = parse_expr();
    expect(TokenKind::Semicolon, "expected ';'");
    return std::make_unique<ReturnStmt>(std::move(expr));
}

std::unique_ptr<Stmt> Parser::parse_assignment() {
    auto& nt = consume(); std::string name = nt.lexeme;
    consume(); // '='
    auto val = parse_expr();
    expect(TokenKind::Semicolon, "expected ';'");
    auto a = std::make_unique<AssignStmt>();
    a->name = std::move(name); a->value = std::move(val);
    return a;
}

std::unique_ptr<Stmt> Parser::parse_expression_statement() {
    auto expr = parse_expr();
    expect(TokenKind::Semicolon, "expected ';'");
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<CompoundStmt> Parser::parse_compound_statement() {
    expect(TokenKind::LBrace, "expected '{'");
    auto block = std::make_unique<CompoundStmt>();
    while (peek().kind != TokenKind::RBrace && peek().kind != TokenKind::Eof)
        block->statements.push_back(parse_statement());
    expect(TokenKind::RBrace, "expected '}'");
    return block;
}

// ===========================================================================
// Expressions
// ===========================================================================
std::unique_ptr<Expr> Parser::parse_expr() { return parse_logical_or(); }
std::unique_ptr<Expr> Parser::parse_logical_or() {
    auto left = parse_logical_and();
    while (peek().kind == TokenKind::PipePipe) { consume();
        auto r = parse_logical_and(); left = std::make_unique<BinaryExpr>(BinaryOp::Or, std::move(left), std::move(r)); }
    return left;
}
std::unique_ptr<Expr> Parser::parse_logical_and() {
    auto left = parse_equality();
    while (peek().kind == TokenKind::AmpAmp) { consume();
        auto r = parse_equality(); left = std::make_unique<BinaryExpr>(BinaryOp::And, std::move(left), std::move(r)); }
    return left;
}
std::unique_ptr<Expr> Parser::parse_equality() {
    auto left = parse_relational();
    while (peek().kind == TokenKind::EqualEqual || peek().kind == TokenKind::BangEqual) {
        auto op = peek().kind == TokenKind::EqualEqual ? BinaryOp::Eq : BinaryOp::Ne;
        consume(); auto r = parse_relational();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(r));
    }
    return left;
}
std::unique_ptr<Expr> Parser::parse_relational() {
    auto left = parse_additive();
    while (peek().kind == TokenKind::Less || peek().kind == TokenKind::Greater ||
           peek().kind == TokenKind::LessEqual || peek().kind == TokenKind::GreaterEqual) {
        BinaryOp op;
        switch (peek().kind) {
        case TokenKind::Less: op = BinaryOp::Lt; break;
        case TokenKind::Greater: op = BinaryOp::Gt; break;
        case TokenKind::LessEqual: op = BinaryOp::Le; break;
        case TokenKind::GreaterEqual: op = BinaryOp::Ge; break;
        default: op = BinaryOp::Lt;
        }
        consume(); auto r = parse_additive();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(r));
    }
    return left;
}
std::unique_ptr<Expr> Parser::parse_additive() {
    auto left = parse_multiplicative();
    while (peek().kind == TokenKind::Plus || peek().kind == TokenKind::Minus) {
        auto op = peek().kind == TokenKind::Plus ? BinaryOp::Add : BinaryOp::Sub;
        consume(); auto r = parse_multiplicative();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(r));
    }
    return left;
}
std::unique_ptr<Expr> Parser::parse_multiplicative() {
    auto left = parse_unary();
    while (peek().kind == TokenKind::Star || peek().kind == TokenKind::Slash || peek().kind == TokenKind::Percent) {
        BinaryOp op;
        switch (peek().kind) {
        case TokenKind::Star: op = BinaryOp::Mul; break;
        case TokenKind::Slash: op = BinaryOp::Div; break;
        case TokenKind::Percent: op = BinaryOp::Mod; break;
        default: op = BinaryOp::Mul;
        }
        consume(); auto r = parse_unary();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(r));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parse_unary() {
    if (match(TokenKind::Minus)) return resolve_unary_minus(parse_unary());
    if (match(TokenKind::Plus)) return resolve_unary_plus(parse_unary());
    if (match(TokenKind::Bang)) return std::make_unique<UnaryExpr>(UnaryOp::Not, parse_unary());
    return resolve_bare_raw_literal(parse_primary());
}

std::unique_ptr<Expr> Parser::parse_primary() {
    if (peek().kind == TokenKind::IntegerLiteral) {
        auto t = consume(); return std::make_unique<RawIntLiteralExpr>(t.int_value);
    }
    if (peek().kind == TokenKind::Identifier) {
        auto t = consume();
        std::string name = t.lexeme; SourceLocation loc = t.location;
        // Check for call: ID ( args )
        if (peek().kind == TokenKind::LParen) {
            return parse_call_suffix(std::move(name), loc);
        }
        return std::make_unique<IdentifierExpr>(std::move(name), loc);
    }
    if (match(TokenKind::LParen)) {
        auto expr = parse_expr();
        expect(TokenKind::RParen, "expected ')'");
        return expr;
    }
    error("expected expression");
    return nullptr;
}

std::unique_ptr<Expr> Parser::parse_call_suffix(std::string callee, SourceLocation loc) {
    consume(); // '('
    std::vector<std::unique_ptr<Expr>> args;
    if (peek().kind != TokenKind::RParen) {
        args.push_back(parse_expr());
        while (peek().kind == TokenKind::Comma) {
            consume();
            args.push_back(parse_expr());
        }
    }
    expect(TokenKind::RParen, "expected ')'");
    return std::make_unique<CallExpr>(std::move(callee), std::move(args), Type::Int);
}

// ===========================================================================
// Raw literal resolution
// ===========================================================================
std::unique_ptr<Expr> Parser::resolve_unary_minus(std::unique_ptr<Expr> operand) {
    auto* r = dynamic_cast<RawIntLiteralExpr*>(operand.get());
    if (r) {
        if (r->magnitude == 2147483648ULL) return std::make_unique<IntLiteralExpr>(INT32_MIN);
        if (r->magnitude > 2147483648ULL) {
            std::ostringstream m; m << "integer literal too large for 32-bit signed int";
            error(m.str());
        }
        return std::make_unique<IntLiteralExpr>(-static_cast<int32_t>(r->magnitude));
    }
    return std::make_unique<UnaryExpr>(UnaryOp::Minus, std::move(operand));
}
std::unique_ptr<Expr> Parser::resolve_unary_plus(std::unique_ptr<Expr> operand) {
    auto* r = dynamic_cast<RawIntLiteralExpr*>(operand.get());
    if (r) {
        if (r->magnitude > static_cast<uint64_t>(INT32_MAX)) error("integer literal too large");
        return std::make_unique<IntLiteralExpr>(static_cast<int32_t>(r->magnitude));
    }
    return std::make_unique<UnaryExpr>(UnaryOp::Plus, std::move(operand));
}
std::unique_ptr<Expr> Parser::resolve_bare_raw_literal(std::unique_ptr<Expr> primary) {
    auto* r = dynamic_cast<RawIntLiteralExpr*>(primary.get());
    if (r) {
        if (r->magnitude > static_cast<uint64_t>(INT32_MAX)) error("integer literal too large");
        return std::make_unique<IntLiteralExpr>(static_cast<int32_t>(r->magnitude));
    }
    return primary;
}
