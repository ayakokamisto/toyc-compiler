#include "sema/semantic_model.h"
#include "ast/ast.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "sema model test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

// Minimal AST fragments for side-table testing.
struct DummyDecl : toyc::ast::Decl {};
struct DummyExpr : toyc::ast::Expr {};

std::unique_ptr<toyc::sema::Symbol> makeVar(std::string name) {
    auto decl = std::make_unique<DummyDecl>();
    return std::make_unique<toyc::sema::Symbol>(
        toyc::sema::SymbolKind::Variable, std::move(name),
        toyc::sema::Type::Int, decl.get(), 0);
}

std::unique_ptr<toyc::sema::FunctionSymbol> makeFunc(
    std::string name, toyc::sema::Type returnType,
    std::vector<toyc::sema::Type> paramTypes) {
    auto decl = std::make_unique<DummyDecl>();
    return std::make_unique<toyc::sema::FunctionSymbol>(
        std::move(name), returnType, decl.get(), 0,
        std::move(paramTypes));
}

void testRegisterAndRetrieveSymbol() {
    toyc::sema::SemanticModel model;
    auto sym = makeVar("x");
    const toyc::sema::Symbol* ptr = sym.get();

    const toyc::sema::Symbol* registered = model.registerSymbol(std::move(sym));
    require(registered == ptr, "registerSymbol should return the stored pointer");

    DummyDecl decl;
    model.setDeclSymbol(&decl, ptr);

    const toyc::sema::Symbol* found = model.getDeclSymbol(decl);
    require(found == ptr, "getDeclSymbol should return the registered symbol");
}

void testSetAndGetBinding() {
    toyc::sema::SemanticModel model;
    auto sym = makeVar("x");
    const toyc::sema::Symbol* ptr = model.registerSymbol(std::move(sym));

    toyc::ast::DeclRefExpr refExpr;
    refExpr.name = "x";

    model.setBinding(&refExpr, ptr);

    const toyc::sema::Symbol* found = model.lookupBinding(refExpr);
    require(found == ptr, "lookupBinding should return the bound symbol");
}

void testMissingBinding() {
    toyc::sema::SemanticModel model;
    toyc::ast::DeclRefExpr refExpr;
    refExpr.name = "nonexistent";

    const toyc::sema::Symbol* found = model.lookupBinding(refExpr);
    require(found == nullptr, "lookupBinding should return nullptr for unbound expr");
}

void testSetAndGetCallee() {
    toyc::sema::SemanticModel model;
    auto func = makeFunc("f", toyc::sema::Type::Int, {});
    const toyc::sema::FunctionSymbol* fptr =
        model.registerFunctionSymbol(std::move(func));

    toyc::ast::CallExpr callExpr;
    callExpr.callee = "f";

    model.setCallee(&callExpr, fptr);

    const toyc::sema::FunctionSymbol* found = model.lookupCallee(callExpr);
    require(found == fptr, "lookupCallee should return the bound function");
}

void testMissingCallee() {
    toyc::sema::SemanticModel model;
    toyc::ast::CallExpr callExpr;
    callExpr.callee = "nonexistent";

    const toyc::sema::FunctionSymbol* found = model.lookupCallee(callExpr);
    require(found == nullptr, "lookupCallee should return nullptr for unbound call");
}

void testSetAndGetExprType() {
    toyc::sema::SemanticModel model;
    DummyExpr expr;

    model.setExprType(&expr, toyc::sema::Type::Int);
    require(model.getExprType(expr) == toyc::sema::Type::Int,
            "getExprType should return Int");

    DummyExpr voidExpr;
    model.setExprType(&voidExpr, toyc::sema::Type::Void);
    require(model.getExprType(voidExpr) == toyc::sema::Type::Void,
            "getExprType should return Void");
}

void testGetExprTypeFallback() {
    toyc::sema::SemanticModel model;
    DummyExpr expr;
    // Not set — should fall back to Int for error recovery
    require(model.getExprType(expr) == toyc::sema::Type::Int,
            "getExprType should return Int as fallback for unset expr");
}

void testSetAndGetConstantValue() {
    toyc::sema::SemanticModel model;
    DummyExpr expr;

    model.setConstantValue(&expr, 42);
    auto val = model.getConstantValue(expr);
    require(val.has_value(), "getConstantValue should return a value");
    require(val.value() == 42, "getConstantValue should return 42");
}

void testGetConstantValueMissing() {
    toyc::sema::SemanticModel model;
    DummyExpr expr;

    auto val = model.getConstantValue(expr);
    require(!val.has_value(), "getConstantValue should return nullopt for unset expr");
}

void testGlobalSymbolsOrder() {
    toyc::sema::SemanticModel model;

    auto sym1 = makeVar("a");
    auto sym2 = makeVar("b");
    auto sym3 = makeVar("c");

    const toyc::sema::Symbol* p1 = model.registerSymbol(std::move(sym1));
    const toyc::sema::Symbol* p2 = model.registerSymbol(std::move(sym2));
    const toyc::sema::Symbol* p3 = model.registerSymbol(std::move(sym3));

    DummyDecl d1, d2, d3;
    model.setDeclSymbol(&d1, p1);
    model.setDeclSymbol(&d2, p2);
    model.setDeclSymbol(&d3, p3);

    const auto& globals = model.globalSymbols();
    require(globals.size() == 3, "should have 3 global symbols");
    require(globals[0] == p1, "first global should be a");
    require(globals[1] == p2, "second global should be b");
    require(globals[2] == p3, "third global should be c");
}

void testFunctionSymbolsOrder() {
    toyc::sema::SemanticModel model;

    auto f1 = makeFunc("f1", toyc::sema::Type::Int, {});
    auto f2 = makeFunc("f2", toyc::sema::Type::Void,
                       {toyc::sema::Type::Int});

    const toyc::sema::FunctionSymbol* fp1 =
        model.registerFunctionSymbol(std::move(f1));
    const toyc::sema::FunctionSymbol* fp2 =
        model.registerFunctionSymbol(std::move(f2));

    const auto& funcs = model.functionSymbols();
    require(funcs.size() == 2, "should have 2 function symbols");
    require(funcs[0] == fp1, "first function should be f1");
    require(funcs[1] == fp2, "second function should be f2");
}

void testMultipleSideTablesIndependent() {
    toyc::sema::SemanticModel model;

    // Register a variable
    auto sym = makeVar("v");
    const toyc::sema::Symbol* vptr = model.registerSymbol(std::move(sym));

    // Register a function
    auto func = makeFunc("g", toyc::sema::Type::Int, {});
    const toyc::sema::FunctionSymbol* fptr =
        model.registerFunctionSymbol(std::move(func));

    // Set up binding and callee for the same name
    toyc::ast::DeclRefExpr refExpr;
    refExpr.name = "v";
    model.setBinding(&refExpr, vptr);

    toyc::ast::CallExpr callExpr;
    callExpr.callee = "g";
    model.setCallee(&callExpr, fptr);

    // Verify both side tables work independently
    require(model.lookupBinding(refExpr) == vptr,
            "binding lookup should be independent");
    require(model.lookupCallee(callExpr) == fptr,
            "callee lookup should be independent");
    require(model.lookupCallee(callExpr) != nullptr, "callee should exist");
}

void testConstantValueZero() {
    toyc::sema::SemanticModel model;
    DummyExpr expr;
    model.setConstantValue(&expr, 0);
    auto val = model.getConstantValue(expr);
    require(val.has_value(), "zero constant should have value");
    require(val.value() == 0, "zero constant should be 0");
}

void testConstantValueNegative() {
    toyc::sema::SemanticModel model;
    DummyExpr expr;
    model.setConstantValue(&expr, -42);
    auto val = model.getConstantValue(expr);
    require(val.has_value(), "negative constant should have value");
    require(val.value() == -42, "negative constant should be -42");
}

} // namespace

int main() {
    testRegisterAndRetrieveSymbol();
    testSetAndGetBinding();
    testMissingBinding();
    testSetAndGetCallee();
    testMissingCallee();
    testSetAndGetExprType();
    testGetExprTypeFallback();
    testSetAndGetConstantValue();
    testGetConstantValueMissing();
    testGlobalSymbolsOrder();
    testFunctionSymbolsOrder();
    testMultipleSideTablesIndependent();
    testConstantValueZero();
    testConstantValueNegative();

    std::cout << "All sema model tests passed.\n";
    return 0;
}
