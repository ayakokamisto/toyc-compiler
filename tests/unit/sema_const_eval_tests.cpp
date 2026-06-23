#include "sema/const_eval.h"
#include "sema/semantic_model.h"
#include "sema/symbol.h"
#include "sema/type.h"
#include "ast/ast.h"
#include "common/diagnostic.h"
#include "common/token.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "sema const eval test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

// --- AST expression helpers ---

std::unique_ptr<toyc::ast::IntLiteralExpr> lit(const std::string& spelling) {
    auto e = std::make_unique<toyc::ast::IntLiteralExpr>();
    e->spelling = spelling;
    return e;
}

std::unique_ptr<toyc::ast::DeclRefExpr> ref(const std::string& name) {
    auto e = std::make_unique<toyc::ast::DeclRefExpr>();
    e->name = name;
    return e;
}

std::unique_ptr<toyc::ast::UnaryExpr> unary(
    toyc::TokenKind op,
    std::unique_ptr<toyc::ast::Expr> operand) {
    auto e = std::make_unique<toyc::ast::UnaryExpr>();
    e->op = op;
    e->operand = std::move(operand);
    return e;
}

std::unique_ptr<toyc::ast::BinaryExpr> binary(
    toyc::TokenKind op,
    std::unique_ptr<toyc::ast::Expr> left,
    std::unique_ptr<toyc::ast::Expr> right) {
    auto e = std::make_unique<toyc::ast::BinaryExpr>();
    e->op = op;
    e->left = std::move(left);
    e->right = std::move(right);
    return e;
}

std::unique_ptr<toyc::ast::CallExpr> call(
    const std::string& callee,
    std::vector<std::unique_ptr<toyc::ast::Expr>> args = {}) {
    auto e = std::make_unique<toyc::ast::CallExpr>();
    e->callee = callee;
    e->arguments = std::move(args);
    return e;
}

// --- Evaluation helper ---

std::optional<std::int32_t> eval(
    const toyc::ast::Expr& expr,
    toyc::sema::SemanticModel& model,
    std::vector<toyc::Diagnostic>& diagnostics) {
    return toyc::sema::evaluateConstExpr(expr, model, diagnostics);
}

// --- Tests ---

// Minimal AST node for creating symbols in tests.
struct DummyDecl : toyc::ast::Decl {};

void testLiteralZero() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = lit("0");
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "0 should evaluate");
    require(*result == 0, "0 should be 0");
    require(diags.empty(), "no diagnostics expected");
}

void testLiteral42() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = lit("42");
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "42 should evaluate");
    require(*result == 42, "42 should be 42");
}

void testLiteralMaxInt32() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = lit("2147483647");
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "2147483647 should evaluate");
    require(*result == 2147483647, "should be 2147483647");
}

void testLiteralOverflow() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = lit("2147483649");
    auto result = eval(*expr, model, diags);
    require(!result.has_value(), "2147483649 should fail");
    require(!diags.empty(), "diagnostic expected");
}

void testUnaryMinus() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = unary(toyc::TokenKind::Minus, lit("5"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "-5 should evaluate");
    require(*result == -5, "-5 should be -5");
}

void testUnaryMinusIntMin() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    // -2147483648 is INT32_MIN, the one case where the literal > INT32_MAX
    auto expr = unary(toyc::TokenKind::Minus, lit("2147483648"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "-2147483648 should evaluate");
    require(*result == -2147483648, "should be INT32_MIN");
}

void testUnaryPlus() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = unary(toyc::TokenKind::Plus, lit("42"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "+42 should evaluate");
    require(*result == 42, "+42 should be 42");
}

void testLogicalNotZero() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = unary(toyc::TokenKind::Bang, lit("0"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "!0 should evaluate");
    require(*result == 1, "!0 should be 1");
}

void testLogicalNotNonZero() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = unary(toyc::TokenKind::Bang, lit("42"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "!42 should evaluate");
    require(*result == 0, "!42 should be 0");
}

void testAddition() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = binary(toyc::TokenKind::Plus, lit("2"), lit("3"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "2+3 should evaluate");
    require(*result == 5, "2+3 should be 5");
}

void testSubtraction() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = binary(toyc::TokenKind::Minus, lit("10"), lit("7"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "10-7 should evaluate");
    require(*result == 3, "10-7 should be 3");
}

void testMultiplication() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = binary(toyc::TokenKind::Star, lit("4"), lit("5"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "4*5 should evaluate");
    require(*result == 20, "4*5 should be 20");
}

void testDivision() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = binary(toyc::TokenKind::Slash, lit("20"), lit("4"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "20/4 should evaluate");
    require(*result == 5, "20/4 should be 5");
}

void testDivisionTruncatesTowardZero() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    // -20/3 = -6 (truncates toward zero in C++11+)
    auto expr = binary(toyc::TokenKind::Slash,
                       unary(toyc::TokenKind::Minus, lit("20")),
                       lit("3"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "-20/3 should evaluate");
    require(*result == -6, "-20/3 should be -6");
}

void testDivisionByZero() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = binary(toyc::TokenKind::Slash, lit("5"), lit("0"));
    auto result = eval(*expr, model, diags);
    require(!result.has_value(), "5/0 should fail");
    require(!diags.empty(), "division by zero diagnostic expected");
}

void testRemainder() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = binary(toyc::TokenKind::Percent, lit("20"), lit("7"));
    auto result = eval(*expr, model, diags);
    require(result.has_value(), "20%7 should evaluate");
    require(*result == 6, "20%7 should be 6");
}

void testRemainderByZero() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto expr = binary(toyc::TokenKind::Percent, lit("5"), lit("0"));
    auto result = eval(*expr, model, diags);
    require(!result.has_value(), "5%0 should fail");
    require(!diags.empty(), "remainder by zero diagnostic expected");
}

void testLessThan() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto exprTrue = binary(toyc::TokenKind::Less, lit("5"), lit("3"));
    auto exprFalse = binary(toyc::TokenKind::Less, lit("3"), lit("5"));
    require(*eval(*exprTrue, model, diags) == 0, "5<3 should be 0");
    require(*eval(*exprFalse, model, diags) == 1, "3<5 should be 1");
}

void testGreaterThan() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto exprTrue = binary(toyc::TokenKind::Greater, lit("5"), lit("3"));
    auto exprFalse = binary(toyc::TokenKind::Greater, lit("3"), lit("5"));
    require(*eval(*exprTrue, model, diags) == 1, "5>3 should be 1");
    require(*eval(*exprFalse, model, diags) == 0, "3>5 should be 0");
}

void testEqual() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto exprTrue = binary(toyc::TokenKind::EqualEqual, lit("5"), lit("5"));
    auto exprFalse = binary(toyc::TokenKind::EqualEqual, lit("5"), lit("3"));
    require(*eval(*exprTrue, model, diags) == 1, "5==5 should be 1");
    require(*eval(*exprFalse, model, diags) == 0, "5==3 should be 0");
}

void testNotEqual() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto exprTrue = binary(toyc::TokenKind::BangEqual, lit("5"), lit("3"));
    auto exprFalse = binary(toyc::TokenKind::BangEqual, lit("5"), lit("5"));
    require(*eval(*exprTrue, model, diags) == 1, "5!=3 should be 1");
    require(*eval(*exprFalse, model, diags) == 0, "5!=5 should be 0");
}

void testLessEqual() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    require(*eval(*binary(toyc::TokenKind::LessEqual, lit("3"), lit("5")), model, diags) == 1,
            "3<=5 should be 1");
    require(*eval(*binary(toyc::TokenKind::LessEqual, lit("5"), lit("5")), model, diags) == 1,
            "5<=5 should be 1");
    require(*eval(*binary(toyc::TokenKind::LessEqual, lit("5"), lit("3")), model, diags) == 0,
            "5<=3 should be 0");
}

void testGreaterEqual() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    require(*eval(*binary(toyc::TokenKind::GreaterEqual, lit("5"), lit("3")), model, diags) == 1,
            "5>=3 should be 1");
    require(*eval(*binary(toyc::TokenKind::GreaterEqual, lit("5"), lit("5")), model, diags) == 1,
            "5>=5 should be 1");
    require(*eval(*binary(toyc::TokenKind::GreaterEqual, lit("3"), lit("5")), model, diags) == 0,
            "3>=5 should be 0");
}

void testLogicalAnd() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    require(*eval(*binary(toyc::TokenKind::AmpAmp, lit("1"), lit("1")), model, diags) == 1,
            "1&&1 should be 1");
    require(*eval(*binary(toyc::TokenKind::AmpAmp, lit("1"), lit("0")), model, diags) == 0,
            "1&&0 should be 0");
    require(*eval(*binary(toyc::TokenKind::AmpAmp, lit("0"), lit("1")), model, diags) == 0,
            "0&&1 should be 0");
}

void testLogicalOr() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    require(*eval(*binary(toyc::TokenKind::PipePipe, lit("1"), lit("0")), model, diags) == 1,
            "1||0 should be 1");
    require(*eval(*binary(toyc::TokenKind::PipePipe, lit("0"), lit("0")), model, diags) == 0,
            "0||0 should be 0");
    require(*eval(*binary(toyc::TokenKind::PipePipe, lit("0"), lit("42")), model, diags) == 1,
            "0||42 should be 1");
}

void testNestedArithmetic() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    // (2+3)*4 = 20
    auto add = binary(toyc::TokenKind::Plus, lit("2"), lit("3"));
    auto mul = binary(toyc::TokenKind::Star, std::move(add), lit("4"));
    auto result = eval(*mul, model, diags);
    require(result.has_value(), "(2+3)*4 should evaluate");
    require(*result == 20, "(2+3)*4 should be 20");
}

void testConstantReference() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    // Set a constant value directly on a DeclRefExpr.
    // This simulates what Sema does when propagating constant values.
    auto declRef = ref("X");
    model.setConstantValue(declRef.get(), 42);
    auto result = eval(*declRef, model, diags);
    require(result.has_value(), "constant ref should evaluate");
    require(*result == 42, "constant ref should be 42");
}

void testNonConstantVariable() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    // A DeclRefExpr with no constant value and a Variable binding should fail.
    auto declRef = ref("y");
    // Set a binding to a non-constant variable (without constant value).
    DummyDecl dummyDecl;
    auto sym = std::make_unique<toyc::sema::Symbol>(
        toyc::sema::SymbolKind::Variable, "y", toyc::sema::Type::Int,
        &dummyDecl, 0);
    const toyc::sema::Symbol* symPtr = model.registerSymbol(std::move(sym));
    model.setBinding(declRef.get(), symPtr);
    auto result = eval(*declRef, model, diags);
    require(!result.has_value(), "non-constant variable should not evaluate");
    require(!diags.empty(), "diagnostic expected");
    require(contains(diags[0].message, "compile-time constant"),
            "diagnostic should mention compile-time constant");
}

void testFunctionCallNotConstant() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    auto c = call("foo");
    auto result = eval(*c, model, diags);
    require(!result.has_value(), "function call should not be constant");
    require(!diags.empty(), "diagnostic expected");
}

void testArithmeticOverflow() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    // 2147483647 + 1 overflows int32
    auto expr = binary(toyc::TokenKind::Plus, lit("2147483647"), lit("1"));
    auto result = eval(*expr, model, diags);
    require(!result.has_value(), "2147483647+1 should overflow");
    require(!diags.empty(), "overflow diagnostic expected");
}

void testUnderflow() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    // -2147483648 - 1 underflows
    auto minusIntMin = unary(toyc::TokenKind::Minus, lit("2147483648"));
    auto expr = binary(toyc::TokenKind::Minus,
                       std::move(minusIntMin), lit("1"));
    auto result = eval(*expr, model, diags);
    require(!result.has_value(), "-2147483648-1 should underflow");
    require(!diags.empty(), "underflow diagnostic expected");
}

void testNegationOverflow() {
    toyc::sema::SemanticModel model;
    std::vector<toyc::Diagnostic> diags;
    // -(-2147483648) = 2147483648 overflows
    auto innerNeg = unary(toyc::TokenKind::Minus, lit("2147483648"));
    auto outerNeg = unary(toyc::TokenKind::Minus, std::move(innerNeg));
    auto result = eval(*outerNeg, model, diags);
    require(!result.has_value(), "-(-2147483648) should overflow");
    require(!diags.empty(), "overflow diagnostic expected");
}

} // namespace

int main() {
    testLiteralZero();
    testLiteral42();
    testLiteralMaxInt32();
    testLiteralOverflow();
    testUnaryMinus();
    testUnaryMinusIntMin();
    testUnaryPlus();
    testLogicalNotZero();
    testLogicalNotNonZero();
    testAddition();
    testSubtraction();
    testMultiplication();
    testDivision();
    testDivisionTruncatesTowardZero();
    testDivisionByZero();
    testRemainder();
    testRemainderByZero();
    testLessThan();
    testGreaterThan();
    testEqual();
    testNotEqual();
    testLessEqual();
    testGreaterEqual();
    testLogicalAnd();
    testLogicalOr();
    testNestedArithmetic();
    testConstantReference();
    testNonConstantVariable();
    testFunctionCallNotConstant();
    testArithmeticOverflow();
    testUnderflow();
    testNegationOverflow();

    std::cout << "All sema const eval tests passed.\n";
    return 0;
}
