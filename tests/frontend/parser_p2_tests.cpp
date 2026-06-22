#include "ast/ast.h"
#include "common/token_stream.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "parser P2 test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

struct ParsedExpression {
    toyc::TokenStream tokens;
    toyc::parser::Parser parser;
    std::unique_ptr<toyc::ast::Expr> expression;

    explicit ParsedExpression(std::string_view source)
        : tokens(toyc::lex(source)), parser(tokens), expression(parser.parseExpr()) {}
};

const toyc::ast::BinaryExpr& binary(const toyc::ast::Expr* expression,
                                    toyc::TokenKind op,
                                    std::string_view message) {
    const auto* result = dynamic_cast<const toyc::ast::BinaryExpr*>(expression);
    require(result != nullptr && result->op == op, message);
    return *result;
}

const toyc::ast::UnaryExpr& unary(const toyc::ast::Expr* expression,
                                  toyc::TokenKind op,
                                  std::string_view message) {
    const auto* result = dynamic_cast<const toyc::ast::UnaryExpr*>(expression);
    require(result != nullptr && result->op == op, message);
    return *result;
}

const toyc::ast::DeclRefExpr& reference(const toyc::ast::Expr* expression,
                                        std::string_view name,
                                        std::string_view message) {
    const auto* result = dynamic_cast<const toyc::ast::DeclRefExpr*>(expression);
    require(result != nullptr && result->name == name, message);
    return *result;
}

void requireValid(const ParsedExpression& parsed, std::string_view message) {
    require(parsed.expression != nullptr, message);
    require(!parsed.parser.hasError(), message);
    require(parsed.tokens.peek().kind == toyc::TokenKind::Eof, "expression consumes to EOF");
}

void testUnaryExpressions() {
    ParsedExpression negative("-2");
    requireValid(negative, "negative literal");
    const auto& minus = unary(negative.expression.get(), toyc::TokenKind::Minus,
                              "minus creates UnaryExpr");
    const auto* literal = dynamic_cast<const toyc::ast::IntLiteralExpr*>(minus.operand.get());
    require(literal != nullptr && literal->spelling == "2", "minus operand is literal 2");
    require(minus.range.begin.column == 1 && minus.range.end.column == 3,
            "unary source range");

    ParsedExpression doubleNot("!!a");
    requireValid(doubleNot, "nested logical not");
    const auto& outer = unary(doubleNot.expression.get(), toyc::TokenKind::Bang,
                              "outer logical not");
    const auto& inner = unary(outer.operand.get(), toyc::TokenKind::Bang,
                              "inner logical not");
    (void)reference(inner.operand.get(), "a", "logical not operand");

    ParsedExpression nested("-(-a)");
    requireValid(nested, "parenthesized nested unary");
    const auto& nestedOuter = unary(nested.expression.get(), toyc::TokenKind::Minus,
                                    "outer nested minus");
    const auto& nestedInner = unary(nestedOuter.operand.get(), toyc::TokenKind::Minus,
                                    "inner nested minus");
    require(nestedInner.range.begin.column == 2 && nestedInner.range.end.column == 6,
            "parenthesized unary range");
}

void testCallExpressions() {
    ParsedExpression empty("foo()");
    requireValid(empty, "empty call");
    const auto* emptyCall = dynamic_cast<const toyc::ast::CallExpr*>(empty.expression.get());
    require(emptyCall != nullptr && emptyCall->callee == "foo" && emptyCall->arguments.empty(),
            "foo call without arguments");

    ParsedExpression arguments("foo(1, a + 2)");
    requireValid(arguments, "call with arguments");
    const auto* call = dynamic_cast<const toyc::ast::CallExpr*>(arguments.expression.get());
    require(call != nullptr && call->arguments.size() == 2, "call argument count");
    (void)binary(call->arguments[1].get(), toyc::TokenKind::Plus,
                 "call argument expression");
    require(call->range.begin.column == 1 && call->range.end.column == 14,
            "call source range");

    ParsedExpression nested("foo(bar())");
    requireValid(nested, "nested call");
    const auto* outer = dynamic_cast<const toyc::ast::CallExpr*>(nested.expression.get());
    require(outer != nullptr && outer->arguments.size() == 1, "outer call");
    const auto* inner = dynamic_cast<const toyc::ast::CallExpr*>(outer->arguments[0].get());
    require(inner != nullptr && inner->callee == "bar", "inner call");
}

void testMultiplicativeAndAdditivePrecedence() {
    ParsedExpression precedence("1 + 2 * 3");
    requireValid(precedence, "multiplication precedence");
    const auto& add = binary(precedence.expression.get(), toyc::TokenKind::Plus,
                             "addition root");
    (void)binary(add.right.get(), toyc::TokenKind::Star, "multiplication child");

    ParsedExpression parentheses("(1 + 2) * 3");
    requireValid(parentheses, "parentheses precedence");
    const auto& multiply = binary(parentheses.expression.get(), toyc::TokenKind::Star,
                                  "parentheses produce multiply root");
    const auto& grouped = binary(multiply.left.get(), toyc::TokenKind::Plus,
                                 "grouped addition child");
    require(grouped.range.begin.column == 1 && grouped.range.end.column == 8,
            "parenthesized binary range");

    ParsedExpression subtraction("a - b - c");
    requireValid(subtraction, "subtraction associativity");
    const auto& outerSub = binary(subtraction.expression.get(), toyc::TokenKind::Minus,
                                  "outer subtraction");
    (void)binary(outerSub.left.get(), toyc::TokenKind::Minus,
                 "subtraction is left associative");

    ParsedExpression division("a / b / c");
    requireValid(division, "division associativity");
    const auto& outerDiv = binary(division.expression.get(), toyc::TokenKind::Slash,
                                  "outer division");
    (void)binary(outerDiv.left.get(), toyc::TokenKind::Slash,
                 "division is left associative");
}

void testRelationalAndEqualityPrecedence() {
    ParsedExpression relational("a + b < c * d");
    requireValid(relational, "relational precedence");
    const auto& less = binary(relational.expression.get(), toyc::TokenKind::Less,
                              "relational root");
    (void)binary(less.left.get(), toyc::TokenKind::Plus, "additive left operand");
    (void)binary(less.right.get(), toyc::TokenKind::Star, "multiplicative right operand");

    ParsedExpression equality("a < b == c < d");
    requireValid(equality, "equality precedence");
    const auto& equal = binary(equality.expression.get(), toyc::TokenKind::EqualEqual,
                               "equality root");
    (void)binary(equal.left.get(), toyc::TokenKind::Less, "left relational operand");
    (void)binary(equal.right.get(), toyc::TokenKind::Less, "right relational operand");
}

void testRemainingOperatorKinds() {
    ParsedExpression unaryPlus("+a");
    requireValid(unaryPlus, "unary plus");
    (void)unary(unaryPlus.expression.get(), toyc::TokenKind::Plus, "unary plus operator");

    ParsedExpression remainder("a % b");
    requireValid(remainder, "remainder expression");
    (void)binary(remainder.expression.get(), toyc::TokenKind::Percent,
                 "remainder operator");

    ParsedExpression greater("a > b");
    requireValid(greater, "greater expression");
    (void)binary(greater.expression.get(), toyc::TokenKind::Greater, "greater operator");

    ParsedExpression lessEqual("a <= b");
    requireValid(lessEqual, "less-equal expression");
    (void)binary(lessEqual.expression.get(), toyc::TokenKind::LessEqual,
                 "less-equal operator");

    ParsedExpression greaterEqual("a >= b");
    requireValid(greaterEqual, "greater-equal expression");
    (void)binary(greaterEqual.expression.get(), toyc::TokenKind::GreaterEqual,
                 "greater-equal operator");

    ParsedExpression notEqual("a != b");
    requireValid(notEqual, "not-equal expression");
    (void)binary(notEqual.expression.get(), toyc::TokenKind::BangEqual,
                 "not-equal operator");
}

void testLogicalPrecedenceAndAssociativity() {
    ParsedExpression precedence("a || b && c");
    requireValid(precedence, "logical precedence");
    const auto& logicalOr = binary(precedence.expression.get(), toyc::TokenKind::PipePipe,
                                   "logical or root");
    (void)binary(logicalOr.right.get(), toyc::TokenKind::AmpAmp,
                 "logical and has higher precedence");

    ParsedExpression logicalAnd("a && b && c");
    requireValid(logicalAnd, "logical and associativity");
    const auto& outerAnd = binary(logicalAnd.expression.get(), toyc::TokenKind::AmpAmp,
                                  "outer logical and");
    (void)binary(outerAnd.left.get(), toyc::TokenKind::AmpAmp,
                 "logical and is left associative");

    ParsedExpression logicalOrChain("a || b || c");
    requireValid(logicalOrChain, "logical or associativity");
    const auto& outerOr = binary(logicalOrChain.expression.get(), toyc::TokenKind::PipePipe,
                                 "outer logical or");
    (void)binary(outerOr.left.get(), toyc::TokenKind::PipePipe,
                 "logical or is left associative");
}

void testNegativeTokenRuleAndComplexExpression() {
    ParsedExpression negative("a + -2 * 3");
    requireValid(negative, "negative token expression");
    const auto& add = binary(negative.expression.get(), toyc::TokenKind::Plus,
                             "negative expression addition root");
    const auto& multiply = binary(add.right.get(), toyc::TokenKind::Star,
                                  "negative expression multiply child");
    (void)unary(multiply.left.get(), toyc::TokenKind::Minus,
                "negative literal remains unary expression");

    ParsedExpression complex("foo(a + 1, -b * 2) == 0 || c");
    requireValid(complex, "complex expression");
    const auto& logicalOr = binary(complex.expression.get(), toyc::TokenKind::PipePipe,
                                   "complex logical root");
    const auto& equal = binary(logicalOr.left.get(), toyc::TokenKind::EqualEqual,
                               "complex equality child");
    const auto* call = dynamic_cast<const toyc::ast::CallExpr*>(equal.left.get());
    require(call != nullptr && call->arguments.size() == 2, "complex call");
    const auto& product = binary(call->arguments[1].get(), toyc::TokenKind::Star,
                                 "complex product argument");
    (void)unary(product.left.get(), toyc::TokenKind::Minus, "complex unary argument");
    require(logicalOr.range.begin.column == 1 && logicalOr.range.end.column == 29,
            "complex expression range");
}

void expectExpressionError(std::string_view source, std::string_view message) {
    ParsedExpression parsed(source);
    require(parsed.expression == nullptr, message);
    require(parsed.parser.hasError(), message);
    require(!parsed.parser.diagnostics().empty(), "expression diagnostic collected");
}

void testErrors() {
    expectExpressionError("1 + ;", "missing binary operand");
    expectExpressionError("foo(1, )", "missing call argument");
    expectExpressionError("(1 + 2", "missing closing parenthesis");
}

} // namespace

int main() {
    try {
        testUnaryExpressions();
        testCallExpressions();
        testMultiplicativeAndAdditivePrecedence();
        testRelationalAndEqualityPrecedence();
        testRemainingOperatorKinds();
        testLogicalPrecedenceAndAssociativity();
        testNegativeTokenRuleAndComplexExpression();
        testErrors();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
