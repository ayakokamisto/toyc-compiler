#include "sema/symbol.h"
#include "sema/scope.h"
#include "sema/type.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "sema symbol test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

// Minimal VarDecl-like node for creating symbols.
struct DummyDecl : toyc::ast::Decl {};

std::unique_ptr<toyc::sema::Symbol> makeVar(std::string name, int depth = 0) {
    auto decl = std::make_unique<DummyDecl>();
    return std::make_unique<toyc::sema::Symbol>(
        toyc::sema::SymbolKind::Variable, std::move(name),
        toyc::sema::Type::Int, decl.get(), depth);
}

std::unique_ptr<toyc::sema::Symbol> makeConst(std::string name, int depth = 0) {
    auto decl = std::make_unique<DummyDecl>();
    return std::make_unique<toyc::sema::Symbol>(
        toyc::sema::SymbolKind::Constant, std::move(name),
        toyc::sema::Type::Int, decl.get(), depth);
}

std::unique_ptr<toyc::sema::FunctionSymbol> makeFunc(
    std::string name, toyc::sema::Type returnType,
    std::vector<toyc::sema::Type> paramTypes, int depth = 0) {
    auto decl = std::make_unique<DummyDecl>();
    return std::make_unique<toyc::sema::FunctionSymbol>(
        std::move(name), returnType, decl.get(), depth,
        std::move(paramTypes));
}

void testInsertAndLookup() {
    toyc::sema::Scope scope(nullptr);
    auto sym = makeVar("x");
    const toyc::sema::Symbol* ptr = sym.get();

    bool inserted = scope.insert("x", ptr);
    require(inserted, "insert should succeed for new name");

    const toyc::sema::Symbol* found = scope.lookup("x");
    require(found == ptr, "lookup should return the inserted symbol");

    const toyc::sema::Symbol* missing = scope.lookup("y");
    require(missing == nullptr, "lookup for missing name should return nullptr");
}

void testDuplicateRejection() {
    toyc::sema::Scope scope(nullptr);
    auto sym1 = makeVar("x");
    auto sym2 = makeVar("x");

    bool first = scope.insert("x", sym1.get());
    require(first, "first insert should succeed");

    bool second = scope.insert("x", sym2.get());
    require(!second, "duplicate insert in same scope should fail");
}

void testShadowing() {
    toyc::sema::Scope parentScope(nullptr);
    auto outer = makeVar("x", 0);
    parentScope.insert("x", outer.get());

    toyc::sema::Scope childScope(&parentScope);
    auto inner = makeVar("x", 1);
    childScope.insert("x", inner.get());

    // From child scope, lookup returns inner (shadowing)
    const toyc::sema::Symbol* found = childScope.lookup("x");
    require(found == inner.get(),
            "child scope lookup should return inner symbol (shadowing)");

    // From parent scope, lookup returns outer
    const toyc::sema::Symbol* parentFound = parentScope.lookup("x");
    require(parentFound == outer.get(),
            "parent scope lookup should return outer symbol");

    // lookupLocal in child only finds inner
    const toyc::sema::Symbol* local = childScope.lookupLocal("x");
    require(local == inner.get(),
            "local lookup in child should return inner symbol");

    // lookupLocal in child for outer name returns nullptr
    const toyc::sema::Symbol* localOuter = childScope.lookupLocal("y");
    require(localOuter == nullptr,
            "local lookup for absent name should return nullptr");
}

void testParentChainLookup() {
    toyc::sema::Scope grandparent(nullptr);
    auto sym = makeVar("a", 0);
    grandparent.insert("a", sym.get());

    toyc::sema::Scope parent(&grandparent);
    toyc::sema::Scope child(&parent);

    const toyc::sema::Symbol* found = child.lookup("a");
    require(found == sym.get(),
            "lookup should walk up parent chain to grandparent");
}

void testFunctionSymbol() {
    auto func = makeFunc("add", toyc::sema::Type::Int,
                         {toyc::sema::Type::Int, toyc::sema::Type::Int});

    require(func->kind == toyc::sema::SymbolKind::Function,
            "function symbol kind should be Function");
    require(func->name == "add", "function name should match");
    require(func->type == toyc::sema::Type::Int,
            "function return type should be Int");
    require(func->paramTypes.size() == 2,
            "function should have 2 parameters");
    require(func->paramTypes[0] == toyc::sema::Type::Int,
            "first param should be Int");
    require(func->paramTypes[1] == toyc::sema::Type::Int,
            "second param should be Int");
}

void testDepthTracking() {
    toyc::sema::Scope global(nullptr);
    require(global.depth() == 0, "global scope depth should be 0");

    toyc::sema::Scope functionScope(&global);
    require(functionScope.depth() == 1, "function scope depth should be 1");

    toyc::sema::Scope blockScope(&functionScope);
    require(blockScope.depth() == 2, "block scope depth should be 2");

    // Verify parent chain
    require(blockScope.parent() == &functionScope,
            "block parent should be function scope");
    require(functionScope.parent() == &global,
            "function parent should be global scope");
    require(global.parent() == nullptr,
            "global parent should be nullptr");
}

void testMultipleSymbolsInOneScope() {
    toyc::sema::Scope scope(nullptr);
    auto a = makeVar("a");
    auto b = makeConst("b");
    auto c = makeVar("c");

    require(scope.insert("a", a.get()), "insert a");
    require(scope.insert("b", b.get()), "insert b");
    require(scope.insert("c", c.get()), "insert c");

    require(scope.lookup("a") == a.get(), "lookup a");
    require(scope.lookup("b") == b.get(), "lookup b");
    require(scope.lookup("c") == c.get(), "lookup c");
}

void testVoidFunctionSymbol() {
    auto func = makeFunc("touch", toyc::sema::Type::Void, {});

    require(func->kind == toyc::sema::SymbolKind::Function,
            "void function kind should be Function");
    require(func->type == toyc::sema::Type::Void,
            "void function return type should be Void");
    require(func->paramTypes.empty(),
            "void function should have no parameters");
}

} // namespace

int main() {
    testInsertAndLookup();
    testDuplicateRejection();
    testShadowing();
    testParentChainLookup();
    testFunctionSymbol();
    testDepthTracking();
    testMultipleSymbolsInOneScope();
    testVoidFunctionSymbol();

    std::cout << "All sema symbol tests passed.\n";
    return 0;
}
