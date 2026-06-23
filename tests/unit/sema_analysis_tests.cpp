#include "sema/sema.h"
#include "sema/semantic_model.h"
#include "sema/symbol.h"
#include "sema/type.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "common/token_stream.h"
#include "common/diagnostic.h"
#include "ast/ast.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "sema analysis test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

// Parse ToyC source into a CompUnit.
std::unique_ptr<toyc::ast::CompUnit> parseProgram(std::string_view source) {
    toyc::TokenStream tokens(toyc::lex(source));
    toyc::parser::Parser parser(tokens);
    auto unit = parser.parseCompUnit();
    require(!parser.hasError(), "parser should not have errors");
    return unit;
}

// Analyze a program and return the result.
toyc::sema::SemaResult analyzeProgram(std::string_view source) {
    auto unit = parseProgram(source);
    toyc::sema::Sema sema;
    return sema.analyze(*unit);
}

// Count error diagnostics.
int errorCount(const std::vector<toyc::Diagnostic>& diags) {
    int count = 0;
    for (const auto& d : diags) {
        if (d.severity == toyc::DiagnosticSeverity::Error) {
            ++count;
        }
    }
    return count;
}

// Check that a string contains a substring.
bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

// Check that at least one error diagnostic contains the given substring.
bool hasErrorContaining(const std::vector<toyc::Diagnostic>& diags,
                        std::string_view substring) {
    for (const auto& d : diags) {
        if (d.severity == toyc::DiagnosticSeverity::Error &&
            contains(d.message, substring)) {
            return true;
        }
    }
    return false;
}

// --- Valid program tests ---

void testBasicMain() {
    auto result = analyzeProgram("int main() { return 42; }");
    require(errorCount(result.diagnostics) == 0,
            "basic main should have no errors");

    // Check that main function was registered.
    const auto& funcs = result.model.functionSymbols();
    require(funcs.size() == 1, "should have 1 function");
    require(funcs[0]->name == "main", "function should be main");
    require(funcs[0]->type == toyc::sema::Type::Int,
            "main should return Int");
}

void testFunctionCall() {
    auto result = analyzeProgram(
        "int add(int a, int b) { return a + b; } "
        "int main() { return add(3, 4); }");
    require(errorCount(result.diagnostics) == 0,
            "function call program should have no errors");
    const auto& funcs = result.model.functionSymbols();
    require(funcs.size() == 2, "should have 2 functions");
}

void testGlobalConst() {
    auto result = analyzeProgram(
        "const int X = 7; "
        "int main() { return X; }");
    require(errorCount(result.diagnostics) == 0,
            "global const should have no errors");
}

void testGlobalVar() {
    auto result = analyzeProgram(
        "int x = 5; "
        "int main() { return x + 3; }");
    require(errorCount(result.diagnostics) == 0,
            "global var should have no errors");
}

void testShadowing() {
    auto result = analyzeProgram(
        "int main() { int x = 1; { int x = 2; } return x; }");
    require(errorCount(result.diagnostics) == 0,
            "shadowing should have no errors");
}

void testIfElseReturns() {
    auto result = analyzeProgram(
        "int main() { if (1) return 1; else return 2; }");
    require(errorCount(result.diagnostics) == 0,
            "if-else returns should have no errors");
}

void testVoidFunction() {
    auto result = analyzeProgram(
        "void f() { return; } "
        "int main() { f(); return 0; }");
    require(errorCount(result.diagnostics) == 0,
            "void function should have no errors");
}

void testRecursion() {
    auto result = analyzeProgram(
        "int f(int n) { if (n <= 1) return 1; return n * f(n - 1); } "
        "int main() { return f(5); }");
    require(errorCount(result.diagnostics) == 0,
            "recursion should have no errors");
}

void testWhileLoop() {
    auto result = analyzeProgram(
        "int main() { "
        "  int i = 0; "
        "  while (i < 10) { i = i + 1; } "
        "  return i; "
        "}");
    require(errorCount(result.diagnostics) == 0,
            "while loop should have no errors");
}

void testBreakInWhile() {
    auto result = analyzeProgram(
        "int main() { "
        "  int i = 0; "
        "  while (i < 10) { if (i > 5) break; i = i + 1; } "
        "  return i; "
        "}");
    require(errorCount(result.diagnostics) == 0,
            "break in while should have no errors");
}

void testContinueInWhile() {
    auto result = analyzeProgram(
        "int main() { "
        "  int i = 0; int s = 0; "
        "  while (i < 10) { i = i + 1; if (i == 5) continue; s = s + i; } "
        "  return s; "
        "}");
    require(errorCount(result.diagnostics) == 0,
            "continue in while should have no errors");
}

void testShortCircuitAnd() {
    auto result = analyzeProgram(
        "int main() { if (0 && foo()) return 1; return 0; }");
    // "foo" is undeclared but this test only checks parsing+sema of short-circuit.
    // Actually, this will produce an error because foo() is undeclared.
    // Let's just check that we get the expected undeclared error, not a crash.
    require(errorCount(result.diagnostics) > 0,
            "should have undeclared function error");
    require(hasErrorContaining(result.diagnostics, "not declared"),
            "should report undeclared function");
}

// --- Invalid program tests ---

void testMissingMain() {
    auto result = analyzeProgram("int x = 5;");
    require(errorCount(result.diagnostics) > 0,
            "missing main should be an error");
    require(hasErrorContaining(result.diagnostics, "main"),
            "error should mention main");
}

void testMainReturnsVoid() {
    auto result = analyzeProgram("void main() {}");
    require(hasErrorContaining(result.diagnostics, "main"),
            "void main should be an error");
}

void testMainHasParameters() {
    auto result = analyzeProgram("int main(int x) { return x; }");
    require(hasErrorContaining(result.diagnostics, "main"),
            "main with params should be an error");
}

void testUndeclaredIdentifier() {
    auto result = analyzeProgram(
        "int main() { return y; }");
    require(hasErrorContaining(result.diagnostics, "not declared"),
            "undeclared identifier should be an error");
}

void testDuplicateLocalDeclaration() {
    auto result = analyzeProgram(
        "int main() { int x = 1; int x = 2; return x; }");
    require(hasErrorContaining(result.diagnostics, "duplicate"),
            "duplicate local should be an error");
}

void testAssignToConstant() {
    auto result = analyzeProgram(
        "const int X = 5; "
        "int main() { X = 3; return X; }");
    require(hasErrorContaining(result.diagnostics, "assign to constant"),
            "assign to constant should be an error");
}

void testBreakOutsideLoop() {
    auto result = analyzeProgram(
        "int main() { break; return 0; }");
    require(hasErrorContaining(result.diagnostics, "break"),
            "break outside loop should be an error");
}

void testContinueOutsideLoop() {
    auto result = analyzeProgram(
        "int main() { continue; return 0; }");
    require(hasErrorContaining(result.diagnostics, "continue"),
            "continue outside loop should be an error");
}

void testWrongArgumentCount() {
    auto result = analyzeProgram(
        "int f(int n) { return n; } "
        "int main() { return f(1, 2); }");
    require(hasErrorContaining(result.diagnostics, "expects"),
            "wrong arg count should be an error");
}

void testVoidFunctionReturnsValue() {
    auto result = analyzeProgram(
        "void f() { return 5; } "
        "int main() { f(); return 0; }");
    require(hasErrorContaining(result.diagnostics, "void function"),
            "void function returning value should be an error");
}

void testIntFunctionMissingReturn() {
    auto result = analyzeProgram(
        "int main() { return; }");
    require(hasErrorContaining(result.diagnostics, "int function"),
            "int function missing return value should be an error");
}

void testNotAllPathsReturn() {
    auto result = analyzeProgram(
        "int f(int x) { if (x) return 1; } "
        "int main() { return f(0); }");
    require(hasErrorContaining(result.diagnostics, "not all control paths"),
            "not all paths returning should be an error");
}

void testVoidExpressionAsValue() {
    auto result = analyzeProgram(
        "void f() {} "
        "int main() { int x = f(); return 0; }");
    require(hasErrorContaining(result.diagnostics, "void"),
            "void expression as value should be an error");
}

void testVoidExpressionInBinaryOp() {
    auto result = analyzeProgram(
        "void f() {} "
        "int main() { return f() + 1; }");
    // f() returns void, used in binary op — should error
    require(hasErrorContaining(result.diagnostics, "void"),
            "void in binary op should be an error");
}

void testUndeclaredFunction() {
    auto result = analyzeProgram(
        "int main() { return foo(); }");
    require(hasErrorContaining(result.diagnostics, "not declared"),
            "undeclared function should be an error");
}

void testDuplicateFunction() {
    auto result = analyzeProgram(
        "int f() { return 1; } "
        "int f() { return 2; } "
        "int main() { return f(); }");
    require(hasErrorContaining(result.diagnostics, "duplicate"),
            "duplicate function should be an error");
}

void testDuplicateGlobalVar() {
    auto result = analyzeProgram(
        "int x = 1; int x = 2; int main() { return x; }");
    require(hasErrorContaining(result.diagnostics, "duplicate"),
            "duplicate global var should be an error");
}

void testCallNonFunction() {
    auto result = analyzeProgram(
        "int main() { int x = 1; return x(); }");
    require(hasErrorContaining(result.diagnostics, "not a function"),
            "calling a variable should be an error");
}

void testConditionalReturnAllPaths() {
    // This program has return on all paths and should compile cleanly.
    auto result = analyzeProgram(
        "int abs(int x) { "
        "  if (x < 0) return -x; "
        "  else return x; "
        "} "
        "int main() { return abs(-5); }");
    require(errorCount(result.diagnostics) == 0,
            "all-paths-returning if-else should have no errors");
}

} // namespace

int main() {
    // Valid programs
    testBasicMain();
    testFunctionCall();
    testGlobalConst();
    testGlobalVar();
    testShadowing();
    testIfElseReturns();
    testVoidFunction();
    testRecursion();
    testWhileLoop();
    testBreakInWhile();
    testContinueInWhile();
    testShortCircuitAnd();
    testConditionalReturnAllPaths();

    // Invalid programs
    testMissingMain();
    testMainReturnsVoid();
    testMainHasParameters();
    testUndeclaredIdentifier();
    testDuplicateLocalDeclaration();
    testAssignToConstant();
    testBreakOutsideLoop();
    testContinueOutsideLoop();
    testWrongArgumentCount();
    testVoidFunctionReturnsValue();
    testIntFunctionMissingReturn();
    testNotAllPathsReturn();
    testVoidExpressionAsValue();
    testVoidExpressionInBinaryOp();
    testUndeclaredFunction();
    testDuplicateFunction();
    testDuplicateGlobalVar();
    testCallNonFunction();

    std::cout << "All sema analysis tests passed.\n";
    return 0;
}
