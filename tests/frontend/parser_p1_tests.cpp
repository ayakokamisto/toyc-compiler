#include "ast/ast.h"
#include "common/token_stream.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "parser P1 test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

struct ParseResult {
    toyc::TokenStream tokens;
    toyc::parser::Parser parser;
    std::unique_ptr<toyc::ast::CompUnit> unit;

    explicit ParseResult(std::string_view source)
        : tokens(toyc::lex(source)), parser(tokens), unit(parser.parseCompUnit()) {}
};

void testTopLevelDeclarations() {
    ParseResult result("const int answer = 42; int copy = (answer);");
    require(!result.parser.hasError(), "valid declarations have no diagnostics");
    require(result.unit->declarations.size() == 2, "two top-level declarations");

    const auto* constant =
        dynamic_cast<const toyc::ast::ConstDecl*>(result.unit->declarations[0].get());
    require(constant != nullptr, "first declaration is const");
    require(constant->name == "answer", "constant name");
    const auto* literal =
        dynamic_cast<const toyc::ast::IntLiteralExpr*>(constant->initializer.get());
    require(literal != nullptr && literal->spelling == "42", "constant initializer");

    const auto* variable =
        dynamic_cast<const toyc::ast::VarDecl*>(result.unit->declarations[1].get());
    require(variable != nullptr, "second declaration is variable");
    require(variable->name == "copy", "variable name");
    const auto* reference =
        dynamic_cast<const toyc::ast::DeclRefExpr*>(variable->initializer.get());
    require(reference != nullptr && reference->name == "answer", "parenthesized reference");
    require(reference->range.begin.column == 35 && reference->range.end.column == 43,
            "parenthesized expression range");
}

void testFunctionsAndParameters() {
    ParseResult result("int add(int left, int right) {} void ping() {}");
    require(!result.parser.hasError(), "valid functions have no diagnostics");
    require(result.unit->declarations.size() == 2, "two functions");

    const auto* add =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(add != nullptr, "int function node");
    require(add->returnType == toyc::ast::TypeKind::Int, "int return type");
    require(add->name == "add", "function name");
    require(add->parameters.size() == 2, "parameter count");
    require(add->parameters[0]->name == "left" && add->parameters[1]->name == "right",
            "parameter names");
    require(add->body != nullptr && add->body->statements.empty(), "empty function body");

    const auto* ping =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[1].get());
    require(ping != nullptr && ping->returnType == toyc::ast::TypeKind::Void,
            "void function node");
    require(ping->parameters.empty(), "empty parameter list");
}

void testBlockAndBasicExpressionStatements() {
    ParseResult result("int main() { 1; value; (2); {;} }");
    require(!result.parser.hasError(), "P1 block statements parse");
    const auto* function =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(function != nullptr, "main function");
    require(function->body->statements.size() == 4, "block statement count");
    require(dynamic_cast<const toyc::ast::ExprStmt*>(function->body->statements[0].get()) !=
                nullptr,
            "integer expression statement");
    require(dynamic_cast<const toyc::ast::ExprStmt*>(function->body->statements[1].get()) !=
                nullptr,
            "identifier expression statement");
    const auto* nested =
        dynamic_cast<const toyc::ast::BlockStmt*>(function->body->statements[3].get());
    require(nested != nullptr && nested->statements.size() == 1, "nested block");
    require(dynamic_cast<const toyc::ast::EmptyStmt*>(nested->statements[0].get()) != nullptr,
            "empty statement");
}

void testExpressionEntryPointAndBoundary() {
    toyc::TokenStream tokens(toyc::lex("((123));"));
    toyc::parser::Parser parser(tokens);
    std::unique_ptr<toyc::ast::Expr> expression = parser.parseExpr();
    require(!parser.hasError(), "standalone expression parses");
    const auto* literal = dynamic_cast<const toyc::ast::IntLiteralExpr*>(expression.get());
    require(literal != nullptr && literal->spelling == "123", "nested parentheses");
    require(tokens.peek().kind == toyc::TokenKind::Semicolon,
            "expression leaves contextual terminator");
}

void testTopLevelRecovery() {
    ParseResult result("int broken = ; int valid = 7;");
    require(result.parser.hasError(), "invalid declaration reports diagnostic");
    require(!result.parser.diagnostics().empty(), "diagnostic is collected");
    require(result.parser.diagnostics()[0].message.find("expected integer") !=
                std::string::npos,
            "diagnostic describes primary expression");
    require(result.unit->declarations.size() == 1, "parser recovers at next declaration");
    const auto* variable =
        dynamic_cast<const toyc::ast::VarDecl*>(result.unit->declarations[0].get());
    require(variable != nullptr && variable->name == "valid", "recovered declaration");
}

void testEmptyCompUnitDiagnostic() {
    ParseResult result("");
    require(result.parser.hasError(), "empty compilation unit reports error");
    require(result.unit->declarations.empty(), "empty unit has no declarations");
}

// ── Additional P1 tests ─────────────────────────────────────────────

void testCompUnitMixedTopLevelItems() {
    ParseResult result("const int X = 1; int y = 2; int f() {} void g(int a) {}");
    require(!result.parser.hasError(), "mixed top-level items have no errors");
    require(result.unit->declarations.size() == 4, "four top-level items");

    require(dynamic_cast<const toyc::ast::ConstDecl*>(result.unit->declarations[0].get()) !=
                nullptr,
            "item 0 is const decl");
    require(dynamic_cast<const toyc::ast::VarDecl*>(result.unit->declarations[1].get()) !=
                nullptr,
            "item 1 is var decl");
    const auto* fdef =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[2].get());
    require(fdef != nullptr && fdef->name == "f", "item 2 is funcDef f");
    const auto* gdef =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[3].get());
    require(gdef != nullptr && gdef->name == "g", "item 3 is funcDef g");
}

void testCompUnitMultipleFunctions() {
    ParseResult result("int a() {} int b() {} int c() {}");
    require(!result.parser.hasError(), "three functions parse");
    require(result.unit->declarations.size() == 3, "three function definitions");
    const auto* a =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    const auto* b =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[1].get());
    const auto* c =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[2].get());
    require(a != nullptr && a->name == "a", "first function is a");
    require(b != nullptr && b->name == "b", "second function is b");
    require(c != nullptr && c->name == "c", "third function is c");
}

void testFuncDefSingleParam() {
    ParseResult result("int id(int x) { x; }");
    require(!result.parser.hasError(), "single param function parses");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func != nullptr, "function node");
    require(func->parameters.size() == 1, "one parameter");
    require(func->parameters[0]->name == "x", "parameter name is x");
    require(func->body->statements.size() == 1, "body has one statement");
}

void testFuncDefMultipleParams() {
    ParseResult result("int sum(int a, int b, int c) {}");
    require(!result.parser.hasError(), "three params parse");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func != nullptr && func->parameters.size() == 3, "three parameters");
    require(func->parameters[0]->name == "a", "first param");
    require(func->parameters[1]->name == "b", "second param");
    require(func->parameters[2]->name == "c", "third param");
}

void testBlockEmpty() {
    ParseResult result("int empty() {}");
    require(!result.parser.hasError(), "empty block parses");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func != nullptr, "function node");
    require(func->body != nullptr, "body exists");
    require(func->body->statements.empty(), "empty body");
}

void testBlockNestedDeep() {
    ParseResult result("int f() { { { ; } } }");
    require(!result.parser.hasError(), "deeply nested blocks parse");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func->body->statements.size() == 1, "outer block has one statement");
    const auto* level1 =
        dynamic_cast<const toyc::ast::BlockStmt*>(func->body->statements[0].get());
    require(level1 != nullptr && level1->statements.size() == 1, "level 1 block");
    const auto* level2 =
        dynamic_cast<const toyc::ast::BlockStmt*>(level1->statements[0].get());
    require(level2 != nullptr && level2->statements.size() == 1, "level 2 block");
    require(dynamic_cast<const toyc::ast::EmptyStmt*>(level2->statements[0].get()) != nullptr,
            "innermost statement is empty");
}

void testBlockMultipleStatements() {
    ParseResult result("int f() { 1; 2; 3; (4); }");
    require(!result.parser.hasError(), "multiple statements parse");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func->body->statements.size() == 4, "four statements in block");
    for (std::size_t i = 0; i < 4; ++i) {
        require(dynamic_cast<const toyc::ast::ExprStmt*>(func->body->statements[i].get()) !=
                    nullptr,
                "statement is expression statement");
    }
}

void testExpressionIntegerLiterals() {
    // zero
    {
        toyc::TokenStream tokens(toyc::lex("0;"));
        toyc::parser::Parser parser(tokens);
        auto expr = parser.parseExpr();
        const auto* lit = dynamic_cast<const toyc::ast::IntLiteralExpr*>(expr.get());
        require(lit != nullptr && lit->spelling == "0", "zero literal");
    }
    // multi-digit
    {
        toyc::TokenStream tokens(toyc::lex("12345;"));
        toyc::parser::Parser parser(tokens);
        auto expr = parser.parseExpr();
        const auto* lit = dynamic_cast<const toyc::ast::IntLiteralExpr*>(expr.get());
        require(lit != nullptr && lit->spelling == "12345", "multi-digit literal");
    }
}

void testExpressionIdentifiers() {
    // simple
    {
        toyc::TokenStream tokens(toyc::lex("foo;"));
        toyc::parser::Parser parser(tokens);
        auto expr = parser.parseExpr();
        const auto* ref = dynamic_cast<const toyc::ast::DeclRefExpr*>(expr.get());
        require(ref != nullptr && ref->name == "foo", "simple identifier");
    }
    // underscore start
    {
        toyc::TokenStream tokens(toyc::lex("_var;"));
        toyc::parser::Parser parser(tokens);
        auto expr = parser.parseExpr();
        const auto* ref = dynamic_cast<const toyc::ast::DeclRefExpr*>(expr.get());
        require(ref != nullptr && ref->name == "_var", "underscore identifier");
    }
    // alphanumeric
    {
        toyc::TokenStream tokens(toyc::lex("x1y2;"));
        toyc::parser::Parser parser(tokens);
        auto expr = parser.parseExpr();
        const auto* ref = dynamic_cast<const toyc::ast::DeclRefExpr*>(expr.get());
        require(ref != nullptr && ref->name == "x1y2", "alphanumeric identifier");
    }
}

void testExpressionParenthesized() {
    // simple paren
    {
        toyc::TokenStream tokens(toyc::lex("(42);"));
        toyc::parser::Parser parser(tokens);
        auto expr = parser.parseExpr();
        const auto* lit = dynamic_cast<const toyc::ast::IntLiteralExpr*>(expr.get());
        require(lit != nullptr && lit->spelling == "42", "simple paren expr");
    }
    // nested parens around identifier
    {
        toyc::TokenStream tokens(toyc::lex("((x));"));
        toyc::parser::Parser parser(tokens);
        auto expr = parser.parseExpr();
        const auto* ref = dynamic_cast<const toyc::ast::DeclRefExpr*>(expr.get());
        require(ref != nullptr && ref->name == "x", "nested paren identifier");
    }
    // deeply nested parens
    {
        toyc::TokenStream tokens(toyc::lex("((((99))));"));
        toyc::parser::Parser parser(tokens);
        auto expr = parser.parseExpr();
        const auto* lit = dynamic_cast<const toyc::ast::IntLiteralExpr*>(expr.get());
        require(lit != nullptr && lit->spelling == "99", "deeply nested paren expr");
    }
}

void testExpressionRangeTracking() {
    toyc::TokenStream tokens(toyc::lex("(123);"));
    toyc::parser::Parser parser(tokens);
    auto expr = parser.parseExpr();
    // After parsing `(123)`, the range should span from column 1 to column 6
    require(expr->range.begin.column == 1, "range begin at column 1");
    require(expr->range.end.column == 6, "range end at column 6 (after closing paren)");
}

void testStmtRecoveryInsideBlock() {
    ParseResult result("int f() { = ; 42; }");
    require(result.parser.hasError(), "broken statement reports diagnostic");
    require(result.unit->declarations.size() == 1, "function still parsed");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func != nullptr, "function node exists");
    // After recovery, the '42;' statement should be parsed
    require(func->body->statements.size() == 1, "recovered statement parsed");
    const auto* stmt =
        dynamic_cast<const toyc::ast::ExprStmt*>(func->body->statements[0].get());
    require(stmt != nullptr, "recovered statement is expr stmt");
    const auto* lit =
        dynamic_cast<const toyc::ast::IntLiteralExpr*>(stmt->expression.get());
    require(lit != nullptr && lit->spelling == "42", "recovered expression is 42");
}

void testConstDeclInTopLevel() {
    ParseResult result("const int MAX = 100;");
    require(!result.parser.hasError(), "top-level const parses");
    const auto* decl =
        dynamic_cast<const toyc::ast::ConstDecl*>(result.unit->declarations[0].get());
    require(decl != nullptr, "is const decl");
    require(decl->name == "MAX", "constant name");
    const auto* lit =
        dynamic_cast<const toyc::ast::IntLiteralExpr*>(decl->initializer.get());
    require(lit != nullptr && lit->spelling == "100", "initializer is 100");
}

void testVoidFunctionNoReturn() {
    ParseResult result("void noop() { ; }");
    require(!result.parser.hasError(), "void function parses");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func != nullptr && func->returnType == toyc::ast::TypeKind::Void,
            "void return type");
    require(func->name == "noop", "function name");
    require(func->body->statements.size() == 1, "body has one empty statement");
}

void testMainEntryPoint() {
    ParseResult result("int main() { 0; }");
    require(!result.parser.hasError(), "main function parses");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func != nullptr, "function node");
    require(func->returnType == toyc::ast::TypeKind::Int, "main returns int");
    require(func->name == "main", "function name is main");
    require(func->parameters.empty(), "main has no parameters");
}

void testExprStmtWithIdentifierAndParen() {
    ParseResult result("int f() { x; (x); ((x)); }");
    require(!result.parser.hasError(), "identifier and paren expr stmts parse");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func->body->statements.size() == 3, "three statements");
    for (std::size_t i = 0; i < 3; ++i) {
        const auto* stmt =
            dynamic_cast<const toyc::ast::ExprStmt*>(func->body->statements[i].get());
        require(stmt != nullptr, "statement is expr stmt");
        const auto* ref = dynamic_cast<const toyc::ast::DeclRefExpr*>(stmt->expression.get());
        require(ref != nullptr && ref->name == "x", "expression is reference to x");
    }
}

void testWhitespaceAndFormatting() {
    ParseResult result("  int  main(  )  {   42  ;  }  ");
    require(!result.parser.hasError(), "extra whitespace is tolerated");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func != nullptr && func->name == "main", "function parsed despite whitespace");
    require(func->body->statements.size() == 1, "one statement");
}

void testBlockRangeIntegrity() {
    //                     123456789012345
    ParseResult result("int f() { 1; }");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    const auto* block = func->body.get();
    // '{' is at column 9, '}' is at column 14, tokenEnd('}') = 15
    require(block->range.begin.column == 9, "block starts at '{'");
    require(block->range.end.column == 15, "block ends after '}'");
}

void testParamRangeIntegrity() {
    ParseResult result("int f(int abc) {}");
    const auto* func =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[0].get());
    require(func->parameters[0]->range.begin.column == 7, "param starts at 'int'");
    require(func->parameters[0]->range.end.column == 14, "param ends after name");
}

} // namespace

int main() {
    try {
        testTopLevelDeclarations();
        testFunctionsAndParameters();
        testBlockAndBasicExpressionStatements();
        testExpressionEntryPointAndBoundary();
        testTopLevelRecovery();
        testEmptyCompUnitDiagnostic();
        testCompUnitMixedTopLevelItems();
        testCompUnitMultipleFunctions();
        testFuncDefSingleParam();
        testFuncDefMultipleParams();
        testBlockEmpty();
        testBlockNestedDeep();
        testBlockMultipleStatements();
        testExpressionIntegerLiterals();
        testExpressionIdentifiers();
        testExpressionParenthesized();
        testExpressionRangeTracking();
        testStmtRecoveryInsideBlock();
        testConstDeclInTopLevel();
        testVoidFunctionNoReturn();
        testMainEntryPoint();
        testExprStmtWithIdentifierAndParen();
        testWhitespaceAndFormatting();
        testBlockRangeIntegrity();
        testParamRangeIntegrity();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
