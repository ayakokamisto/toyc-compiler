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
    std::cerr << "parser P3 test failure: " << message << '\n';
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

const toyc::ast::FuncDef& functionAt(const ParseResult& result, std::size_t index) {
    require(index < result.unit->declarations.size(), "function index");
    const auto* function =
        dynamic_cast<const toyc::ast::FuncDef*>(result.unit->declarations[index].get());
    require(function != nullptr && function->body != nullptr, "function definition");
    return *function;
}

const toyc::ast::BinaryExpr& binary(const toyc::ast::Expr* expression,
                                    toyc::TokenKind op,
                                    std::string_view message) {
    const auto* result = dynamic_cast<const toyc::ast::BinaryExpr*>(expression);
    require(result != nullptr && result->op == op, message);
    return *result;
}

void testLocalDeclarationsAndReference() {
    ParseResult result("int f(){int a=1;const int n=1+2;a;}");
    require(!result.parser.hasError(), "valid local declarations");
    const auto& function = functionAt(result, 0);
    require(function.body->statements.size() == 3, "local statement count");

    const auto* variableStatement =
        dynamic_cast<const toyc::ast::DeclStmt*>(function.body->statements[0].get());
    require(variableStatement != nullptr, "local variable declaration statement");
    const auto* variable =
        dynamic_cast<const toyc::ast::VarDecl*>(variableStatement->declaration.get());
    require(variable != nullptr && variable->name == "a", "local variable declaration");

    const auto* constantStatement =
        dynamic_cast<const toyc::ast::DeclStmt*>(function.body->statements[1].get());
    require(constantStatement != nullptr, "local constant declaration statement");
    const auto* constant =
        dynamic_cast<const toyc::ast::ConstDecl*>(constantStatement->declaration.get());
    require(constant != nullptr && constant->name == "n", "local constant declaration");
    (void)binary(constant->initializer.get(), toyc::TokenKind::Plus,
                 "constant initializer expression");

    const auto* expressionStatement =
        dynamic_cast<const toyc::ast::ExprStmt*>(function.body->statements[2].get());
    require(expressionStatement != nullptr, "reference expression statement");
    const auto* reference =
        dynamic_cast<const toyc::ast::DeclRefExpr*>(expressionStatement->expression.get());
    require(reference != nullptr && reference->name == "a", "reference after declaration");
}

void testAssignmentAndExpressionStatementDispatch() {
    ParseResult result("int f(){a=1;a=foo(1)+-b*3;foo();a+1;}");
    require(!result.parser.hasError(), "valid assignments and expressions");
    const auto& statements = functionAt(result, 0).body->statements;
    require(statements.size() == 4, "assignment and expression statement count");

    const auto* simple = dynamic_cast<const toyc::ast::AssignStmt*>(statements[0].get());
    require(simple != nullptr && simple->target == "a", "simple assignment");

    const auto* complex = dynamic_cast<const toyc::ast::AssignStmt*>(statements[1].get());
    require(complex != nullptr && complex->target == "a", "complex assignment");
    const auto& add = binary(complex->value.get(), toyc::TokenKind::Plus,
                             "complex assignment addition");
    require(dynamic_cast<const toyc::ast::CallExpr*>(add.left.get()) != nullptr,
            "call on assignment right side");
    const auto& product = binary(add.right.get(), toyc::TokenKind::Star,
                                 "multiplication on assignment right side");
    require(dynamic_cast<const toyc::ast::UnaryExpr*>(product.left.get()) != nullptr,
            "unary operand on assignment right side");

    require(dynamic_cast<const toyc::ast::ExprStmt*>(statements[2].get()) != nullptr,
            "call remains expression statement");
    require(dynamic_cast<const toyc::ast::ExprStmt*>(statements[3].get()) != nullptr,
            "binary expression remains expression statement");
}

void testReturnStatements() {
    ParseResult result("void f(){return;} int g(){return a+foo(2);}");
    require(!result.parser.hasError(), "valid return statements");

    const auto* emptyReturn = dynamic_cast<const toyc::ast::ReturnStmt*>(
        functionAt(result, 0).body->statements[0].get());
    require(emptyReturn != nullptr && emptyReturn->value == nullptr, "empty return");

    const auto* valueReturn = dynamic_cast<const toyc::ast::ReturnStmt*>(
        functionAt(result, 1).body->statements[0].get());
    require(valueReturn != nullptr && valueReturn->value != nullptr, "return with value");
    const auto& add = binary(valueReturn->value.get(), toyc::TokenKind::Plus,
                             "return value expression");
    require(dynamic_cast<const toyc::ast::CallExpr*>(add.right.get()) != nullptr,
            "call in return value");
}

void testNestedBlockDeclarations() {
    ParseResult result("int f(){{int a=1;{const int b=2;}}}");
    require(!result.parser.hasError(), "nested local declarations");
    const auto& body = *functionAt(result, 0).body;
    const auto* outer = dynamic_cast<const toyc::ast::BlockStmt*>(body.statements[0].get());
    require(outer != nullptr && outer->statements.size() == 2, "outer nested block");
    require(dynamic_cast<const toyc::ast::DeclStmt*>(outer->statements[0].get()) != nullptr,
            "nested variable declaration");
    const auto* inner =
        dynamic_cast<const toyc::ast::BlockStmt*>(outer->statements[1].get());
    require(inner != nullptr && inner->statements.size() == 1, "inner nested block");
    require(dynamic_cast<const toyc::ast::DeclStmt*>(inner->statements[0].get()) != nullptr,
            "nested constant declaration");
}

void testStatementSourceRanges() {
    ParseResult result("int f(){int a=1;a=2;return a;}");
    require(!result.parser.hasError(), "source range input");
    const auto& statements = functionAt(result, 0).body->statements;

    const auto* declaration = dynamic_cast<const toyc::ast::DeclStmt*>(statements[0].get());
    require(declaration != nullptr && declaration->range.begin.column == 9 &&
                declaration->range.end.column == 17,
            "declaration statement range");

    const auto* assignment = dynamic_cast<const toyc::ast::AssignStmt*>(statements[1].get());
    require(assignment != nullptr && assignment->range.begin.column == 17 &&
                assignment->range.end.column == 21,
            "assignment statement range");
    require(assignment->targetRange.begin.column == 17 &&
                assignment->targetRange.end.column == 18,
            "assignment target range");

    const auto* returnStatement =
        dynamic_cast<const toyc::ast::ReturnStmt*>(statements[2].get());
    require(returnStatement != nullptr && returnStatement->range.begin.column == 21 &&
                returnStatement->range.end.column == 30,
            "return statement range");
}

void testStatementRecovery() {
    ParseResult declarationError("int f(){int a = ; return 1;}");
    require(declarationError.parser.hasError(), "invalid local declaration diagnostic");
    const auto& declarationBody = *functionAt(declarationError, 0).body;
    require(declarationBody.statements.size() == 1 &&
                dynamic_cast<const toyc::ast::ReturnStmt*>(
                    declarationBody.statements[0].get()) != nullptr,
            "recover from local declaration error");

    ParseResult assignmentError("int f(){a = ; return 2;}");
    require(assignmentError.parser.hasError(), "invalid assignment diagnostic");
    const auto& assignmentBody = *functionAt(assignmentError, 0).body;
    require(assignmentBody.statements.size() == 1 &&
                dynamic_cast<const toyc::ast::ReturnStmt*>(assignmentBody.statements[0].get()) !=
                    nullptr,
            "recover from assignment error");

    ParseResult returnError("int f(){return 1}");
    require(returnError.parser.hasError(), "missing return semicolon diagnostic");
    require(functionAt(returnError, 0).body->statements.empty(),
            "malformed return is discarded");
}

void testAcceptanceProgram() {
    ParseResult result(R"(
int inc(int x) {
    return x + 1;
}

int main() {
    const int base = 10;
    int a = base;
    a = inc(a) + 2;
    return a;
}
)");
    require(!result.parser.hasError(), "P3 acceptance program");
    require(result.unit->declarations.size() == 2, "acceptance function count");

    const auto& inc = functionAt(result, 0);
    require(inc.name == "inc" && inc.body->statements.size() == 1,
            "inc function structure");
    const auto* incReturn =
        dynamic_cast<const toyc::ast::ReturnStmt*>(inc.body->statements[0].get());
    require(incReturn != nullptr, "inc return statement");
    (void)binary(incReturn->value.get(), toyc::TokenKind::Plus, "inc return addition");

    const auto& mainFunction = functionAt(result, 1);
    require(mainFunction.name == "main" && mainFunction.body->statements.size() == 4,
            "main function structure");
    require(dynamic_cast<const toyc::ast::DeclStmt*>(
                mainFunction.body->statements[0].get()) != nullptr,
            "main constant declaration");
    require(dynamic_cast<const toyc::ast::DeclStmt*>(
                mainFunction.body->statements[1].get()) != nullptr,
            "main variable declaration");
    require(dynamic_cast<const toyc::ast::AssignStmt*>(
                mainFunction.body->statements[2].get()) != nullptr,
            "main assignment");
    require(dynamic_cast<const toyc::ast::ReturnStmt*>(
                mainFunction.body->statements[3].get()) != nullptr,
            "main return");
}

} // namespace

int main() {
    try {
        testLocalDeclarationsAndReference();
        testAssignmentAndExpressionStatementDispatch();
        testReturnStatements();
        testNestedBlockDeclarations();
        testStatementSourceRanges();
        testStatementRecovery();
        testAcceptanceProgram();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
