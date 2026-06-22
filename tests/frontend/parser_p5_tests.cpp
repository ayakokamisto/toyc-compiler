#include "parser_test_helpers.h"

#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

using toyc::test::require;

const toyc::ast::FuncDef& functionAt(const toyc::ast::CompUnit& unit,
                                     std::size_t index) {
    require(index < unit.declarations.size(), "function index");
    return toyc::test::requireDecl<toyc::ast::FuncDef>(*unit.declarations[index]);
}

const toyc::ast::BinaryExpr& binary(const toyc::ast::Expr& expression,
                                    toyc::TokenKind op,
                                    std::string_view message) {
    const auto& result = toyc::test::requireExpr<toyc::ast::BinaryExpr>(expression);
    require(result.op == op, message);
    return result;
}

const toyc::ast::ReturnStmt& returnAt(const toyc::ast::FuncDef& function,
                                     std::size_t index) {
    require(function.body != nullptr && index < function.body->statements.size(),
            "return index");
    return toyc::test::requireStmt<toyc::ast::ReturnStmt>(
        *function.body->statements[index]);
}

void testFullLanguageAstShape() {
    std::vector<toyc::Diagnostic> diagnostics;
    const auto unit = toyc::test::parseProgram(R"(
const int BASE = 3;

int helper(int x, int y) {
    const int local = 2;
    int value = x + y * local;

    if (value >= 10 && x != 0) {
        value = value - 1;
    } else {
        value = helper(value, -1);
    }

    while (value > 0) {
        if (value % 2 == 0) {
            value = value / 2;
            continue;
        }

        if (value == BASE) {
            break;
        }

        value = value - 1;
    }

    return value;
}

void noop() {
    return;
}

int main() {
    int answer = helper(BASE, 4);
    noop();
    return answer;
}
)", &diagnostics);
    require(diagnostics.empty(), "full language program diagnostics");
    require(unit->declarations.size() == 4, "full language top-level count");
    (void)toyc::test::requireDecl<toyc::ast::ConstDecl>(*unit->declarations[0]);

    const auto& helper = functionAt(*unit, 1);
    require(helper.name == "helper" && helper.parameters.size() == 2,
            "helper signature");
    require(helper.body->statements.size() == 5, "helper body statement count");
    (void)toyc::test::requireStmt<toyc::ast::DeclStmt>(*helper.body->statements[0]);
    const auto& valueDeclStmt =
        toyc::test::requireStmt<toyc::ast::DeclStmt>(*helper.body->statements[1]);
    const auto& valueDecl =
        toyc::test::requireDecl<toyc::ast::VarDecl>(*valueDeclStmt.declaration);
    const auto& valueAdd = binary(*valueDecl.initializer, toyc::TokenKind::Plus,
                                  "value initializer addition");
    (void)binary(*valueAdd.right, toyc::TokenKind::Star, "value initializer multiply");

    const auto& ifStatement =
        toyc::test::requireStmt<toyc::ast::IfStmt>(*helper.body->statements[2]);
    const auto& logicalAnd = binary(*ifStatement.condition, toyc::TokenKind::AmpAmp,
                                    "if logical and");
    (void)binary(*logicalAnd.left, toyc::TokenKind::GreaterEqual,
                 "if greater-equal operand");
    (void)binary(*logicalAnd.right, toyc::TokenKind::BangEqual,
                 "if not-equal operand");
    const auto& elseBlock =
        toyc::test::requireStmt<toyc::ast::BlockStmt>(*ifStatement.elseBranch);
    const auto& elseAssign =
        toyc::test::requireStmt<toyc::ast::AssignStmt>(*elseBlock.statements[0]);
    const auto& recursiveCall =
        toyc::test::requireExpr<toyc::ast::CallExpr>(*elseAssign.value);
    require(recursiveCall.callee == "helper" && recursiveCall.arguments.size() == 2,
            "recursive helper call");
    const auto& negative =
        toyc::test::requireExpr<toyc::ast::UnaryExpr>(*recursiveCall.arguments[1]);
    require(negative.op == toyc::TokenKind::Minus, "negative call argument");
    const auto& one =
        toyc::test::requireExpr<toyc::ast::IntLiteralExpr>(*negative.operand);
    require(one.spelling == "1", "negative literal spelling");

    const auto& loop =
        toyc::test::requireStmt<toyc::ast::WhileStmt>(*helper.body->statements[3]);
    const auto& loopBlock = toyc::test::requireStmt<toyc::ast::BlockStmt>(*loop.body);
    const auto& evenIf =
        toyc::test::requireStmt<toyc::ast::IfStmt>(*loopBlock.statements[0]);
    const auto& evenEqual = binary(*evenIf.condition, toyc::TokenKind::EqualEqual,
                                   "even equality");
    (void)binary(*evenEqual.left, toyc::TokenKind::Percent, "even remainder");
    const auto& evenBlock =
        toyc::test::requireStmt<toyc::ast::BlockStmt>(*evenIf.thenBranch);
    const auto& divideAssign =
        toyc::test::requireStmt<toyc::ast::AssignStmt>(*evenBlock.statements[0]);
    (void)binary(*divideAssign.value, toyc::TokenKind::Slash, "loop division");
    (void)toyc::test::requireStmt<toyc::ast::ContinueStmt>(*evenBlock.statements[1]);
    const auto& breakIf =
        toyc::test::requireStmt<toyc::ast::IfStmt>(*loopBlock.statements[1]);
    const auto& breakBlock =
        toyc::test::requireStmt<toyc::ast::BlockStmt>(*breakIf.thenBranch);
    (void)toyc::test::requireStmt<toyc::ast::BreakStmt>(*breakBlock.statements[0]);
    (void)returnAt(helper, 4);

    const auto& noop = functionAt(*unit, 2);
    require(noop.returnType == toyc::ast::TypeKind::Void, "noop return type");
    require(returnAt(noop, 0).value == nullptr, "noop empty return");

    const auto& mainFunction = functionAt(*unit, 3);
    const auto& callStatement =
        toyc::test::requireStmt<toyc::ast::ExprStmt>(*mainFunction.body->statements[1]);
    const auto& noopCall =
        toyc::test::requireExpr<toyc::ast::CallExpr>(*callStatement.expression);
    require(noopCall.callee == "noop", "noop call expression statement");
}

void testExpressionAstPrecedenceMatrix() {
    std::vector<toyc::Diagnostic> diagnostics;
    const auto unit = toyc::test::parseProgram(R"(
int matrix() {
    return a || b && c;
    return a == b < c + d * e;
    return -a * (b + c);
    return foo(a, b + c, -d);
    return a - b - c;
    return a / b / c;
}
)", &diagnostics);
    require(diagnostics.empty(), "precedence matrix diagnostics");
    const auto& function = functionAt(*unit, 0);

    const auto& logicalOr = binary(*returnAt(function, 0).value, toyc::TokenKind::PipePipe,
                                   "logical-or root");
    (void)binary(*logicalOr.right, toyc::TokenKind::AmpAmp, "logical-and child");

    const auto& equality = binary(*returnAt(function, 1).value,
                                  toyc::TokenKind::EqualEqual, "equality root");
    const auto& less = binary(*equality.right, toyc::TokenKind::Less, "relational child");
    const auto& add = binary(*less.right, toyc::TokenKind::Plus, "additive child");
    (void)binary(*add.right, toyc::TokenKind::Star, "multiplicative child");

    const auto& product = binary(*returnAt(function, 2).value, toyc::TokenKind::Star,
                                 "unary product root");
    require(toyc::test::requireExpr<toyc::ast::UnaryExpr>(*product.left).op ==
                toyc::TokenKind::Minus,
            "unary minus child");
    (void)binary(*product.right, toyc::TokenKind::Plus, "parenthesized addition");

    const auto& call =
        toyc::test::requireExpr<toyc::ast::CallExpr>(*returnAt(function, 3).value);
    require(call.arguments.size() == 3, "matrix call argument count");
    (void)binary(*call.arguments[1], toyc::TokenKind::Plus, "matrix additive argument");
    require(toyc::test::requireExpr<toyc::ast::UnaryExpr>(*call.arguments[2]).op ==
                toyc::TokenKind::Minus,
            "matrix unary argument");

    const auto& subtraction = binary(*returnAt(function, 4).value,
                                     toyc::TokenKind::Minus, "subtraction root");
    (void)binary(*subtraction.left, toyc::TokenKind::Minus,
                 "subtraction left associativity");
    const auto& division = binary(*returnAt(function, 5).value, toyc::TokenKind::Slash,
                                  "division root");
    (void)binary(*division.left, toyc::TokenKind::Slash, "division left associativity");
}

void testSourceRangeMatrix() {
    constexpr std::string_view source =
        "const int A = 1;\n"
        "int f(int x) { return -x + 2; }\n"
        "int main() { if (A) { return f(1); } else return 0; }";
    std::vector<toyc::Diagnostic> diagnostics;
    const auto unit = toyc::test::parseProgram(source, &diagnostics);
    require(diagnostics.empty(), "range matrix diagnostics");
    require(unit->range.begin.line == 1 && unit->range.begin.column == 1 &&
                unit->range.end.line == 3 && unit->range.end.column == 54,
            "compilation unit range");

    const auto& constant =
        toyc::test::requireDecl<toyc::ast::ConstDecl>(*unit->declarations[0]);
    require(constant.range.begin.line == 1 && constant.range.begin.column == 1 &&
                constant.range.end.column == 17,
            "constant range");

    const auto& function = functionAt(*unit, 1);
    require(function.range.begin.line == 2 && function.range.begin.column == 1 &&
                function.range.end.column == 32,
            "function range");
    require(function.parameters[0]->range.begin.column == 7 &&
                function.parameters[0]->range.end.column == 12,
            "parameter range");
    require(function.body->range.begin.column == 14 && function.body->range.end.column == 32,
            "block range");
    const auto& returnStatement = returnAt(function, 0);
    require(returnStatement.range.begin.column == 16 &&
                returnStatement.range.end.column == 30,
            "return range");
    const auto& addition = binary(*returnStatement.value, toyc::TokenKind::Plus,
                                  "range binary expression");
    require(addition.range.begin.column == 23 && addition.range.end.column == 29,
            "binary range");
    const auto& negative =
        toyc::test::requireExpr<toyc::ast::UnaryExpr>(*addition.left);
    require(negative.range.begin.column == 23 && negative.range.end.column == 25,
            "unary range");

    const auto& mainFunction = functionAt(*unit, 2);
    const auto& ifStatement =
        toyc::test::requireStmt<toyc::ast::IfStmt>(*mainFunction.body->statements[0]);
    require(ifStatement.range.begin.column == 14 && ifStatement.range.end.column == 52,
            "if range includes else");
    const auto& thenBlock =
        toyc::test::requireStmt<toyc::ast::BlockStmt>(*ifStatement.thenBranch);
    const auto& callReturn =
        toyc::test::requireStmt<toyc::ast::ReturnStmt>(*thenBlock.statements[0]);
    const auto& call = toyc::test::requireExpr<toyc::ast::CallExpr>(*callReturn.value);
    require(call.range.begin.column == 30 && call.range.end.column == 34,
            "call range");
}

void testRecoveryAtTopLevelBoundaries() {
    std::vector<toyc::Diagnostic> diagnostics;
    const auto unit = toyc::test::parseProgram(R"(
int broken = ;
int good() { return 1; }
int main() { return good(); }
)", &diagnostics);
    require(!diagnostics.empty(), "top-level recovery diagnostic");
    require(unit->declarations.size() == 2, "top-level recovery declaration count");
    require(functionAt(*unit, 0).name == "good", "recovered good function");
    require(functionAt(*unit, 1).name == "main", "recovered main function");
}

void testRecoveryAtStatementBoundaries() {
    std::vector<toyc::Diagnostic> diagnostics;
    const auto unit = toyc::test::parseProgram(R"(
int main() {
    int a = ;
    if (a) {
        a = ;
    }
    while () {
        break
        continue;
    }
    return 0;
}
)", &diagnostics);
    require(diagnostics.size() >= 3, "statement recovery diagnostics");
    const auto& function = functionAt(*unit, 0);
    require(!function.body->statements.empty(), "statement recovery body");
    const auto& finalReturn = toyc::test::requireStmt<toyc::ast::ReturnStmt>(
        *function.body->statements.back());
    require(finalReturn.value != nullptr, "statement recovery final return");
}

void testParserNeverPerformsSemanticChecks() {
    std::vector<toyc::Diagnostic> diagnostics;
    const auto unit = toyc::test::parseProgram(R"(
int main() {
    break;
    continue;
    const int c = 1;
    c = 2;
    undeclared = foo(1);
    return;
}
)", &diagnostics);
    require(diagnostics.empty(), "semantic-only errors produce no parser diagnostics");
    const auto& statements = functionAt(*unit, 0).body->statements;
    require(statements.size() == 6, "semantic-only AST statement count");
    (void)toyc::test::requireStmt<toyc::ast::BreakStmt>(*statements[0]);
    (void)toyc::test::requireStmt<toyc::ast::ContinueStmt>(*statements[1]);
    (void)toyc::test::requireStmt<toyc::ast::DeclStmt>(*statements[2]);
    (void)toyc::test::requireStmt<toyc::ast::AssignStmt>(*statements[3]);
    const auto& undeclaredAssign =
        toyc::test::requireStmt<toyc::ast::AssignStmt>(*statements[4]);
    (void)toyc::test::requireExpr<toyc::ast::CallExpr>(*undeclaredAssign.value);
    require(toyc::test::requireStmt<toyc::ast::ReturnStmt>(*statements[5]).value == nullptr,
            "empty return preserved");
    // Sema diagnoses loop context, const assignment, unresolved names, calls, and returns.
}

} // namespace

int main() {
    try {
        testFullLanguageAstShape();
        testExpressionAstPrecedenceMatrix();
        testSourceRangeMatrix();
        testRecoveryAtTopLevelBoundaries();
        testRecoveryAtStatementBoundaries();
        testParserNeverPerformsSemanticChecks();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
