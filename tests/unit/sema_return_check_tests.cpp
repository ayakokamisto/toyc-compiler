#include "sema/return_check.h"
#include "ast/ast.h"
#include "common/token.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "sema return check test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

// --- Test helpers ---

std::unique_ptr<toyc::ast::ReturnStmt> makeReturn() {
    auto stmt = std::make_unique<toyc::ast::ReturnStmt>();
    auto lit = std::make_unique<toyc::ast::IntLiteralExpr>();
    lit->spelling = "0";
    stmt->value = std::move(lit);
    return stmt;
}

std::unique_ptr<toyc::ast::ReturnStmt> makeVoidReturn() {
    return std::make_unique<toyc::ast::ReturnStmt>();
}

std::unique_ptr<toyc::ast::BlockStmt> makeBlock(
    std::vector<std::unique_ptr<toyc::ast::Stmt>> statements) {
    auto block = std::make_unique<toyc::ast::BlockStmt>();
    block->statements = std::move(statements);
    return block;
}

// Single-statement block convenience overload.
std::unique_ptr<toyc::ast::BlockStmt> makeBlock(
    std::unique_ptr<toyc::ast::Stmt> stmt) {
    std::vector<std::unique_ptr<toyc::ast::Stmt>> stmts;
    stmts.push_back(std::move(stmt));
    return makeBlock(std::move(stmts));
}

// Empty block convenience overload.
std::unique_ptr<toyc::ast::BlockStmt> makeEmptyBlock() {
    return makeBlock(std::vector<std::unique_ptr<toyc::ast::Stmt>>{});
}

std::unique_ptr<toyc::ast::IfStmt> makeIf(
    std::unique_ptr<toyc::ast::Stmt> thenBranch,
    std::unique_ptr<toyc::ast::Stmt> elseBranch = nullptr) {
    auto ifStmt = std::make_unique<toyc::ast::IfStmt>();
    auto cond = std::make_unique<toyc::ast::IntLiteralExpr>();
    cond->spelling = "1";
    ifStmt->condition = std::move(cond);
    ifStmt->thenBranch = std::move(thenBranch);
    ifStmt->elseBranch = std::move(elseBranch);
    return ifStmt;
}

std::unique_ptr<toyc::ast::WhileStmt> makeWhile(
    std::unique_ptr<toyc::ast::Stmt> body) {
    auto whileStmt = std::make_unique<toyc::ast::WhileStmt>();
    auto cond = std::make_unique<toyc::ast::IntLiteralExpr>();
    cond->spelling = "1";
    whileStmt->condition = std::move(cond);
    whileStmt->body = std::move(body);
    return whileStmt;
}

std::unique_ptr<toyc::ast::AssignStmt> makeAssign() {
    auto stmt = std::make_unique<toyc::ast::AssignStmt>();
    stmt->target = "x";
    auto val = std::make_unique<toyc::ast::IntLiteralExpr>();
    val->spelling = "1";
    stmt->value = std::move(val);
    return stmt;
}

// --- Tests ---

void testReturnAlwaysReturns() {
    auto ret = makeReturn();
    require(toyc::sema::alwaysReturns(*ret),
            "return statement should always return");
}

void testEmptyBlockDoesNotAlwaysReturn() {
    auto block = makeEmptyBlock();
    require(!toyc::sema::blockAlwaysReturns(*block),
            "empty block should not always return");
    require(!toyc::sema::alwaysReturns(*block),
            "empty block should not always return via alwaysReturns");
}

void testBlockWithReturnAtEnd() {
    auto block = makeBlock(makeReturn());
    require(toyc::sema::blockAlwaysReturns(*block),
            "block ending with return should always return");
}

void testBlockWithReturnInMiddle() {
    std::vector<std::unique_ptr<toyc::ast::Stmt>> stmts;
    stmts.push_back(makeReturn());
    stmts.push_back(makeAssign());
    auto block = makeBlock(std::move(stmts));
    require(toyc::sema::blockAlwaysReturns(*block),
            "block with return before other statements should always return");
}

void testBlockWithNoReturn() {
    auto block = makeBlock(makeAssign());
    require(!toyc::sema::blockAlwaysReturns(*block),
            "block without return should not always return");
}

void testIfElseBothReturn() {
    auto ifStmt = makeIf(makeReturn(), makeReturn());
    require(toyc::sema::alwaysReturns(*ifStmt),
            "if-else with both branches returning should always return");
}

void testIfWithoutElse() {
    auto ifStmt = makeIf(makeReturn(), nullptr);
    require(!toyc::sema::alwaysReturns(*ifStmt),
            "if without else should not always return");
}

void testIfElseOnlyThenReturns() {
    auto ifStmt = makeIf(makeReturn(), makeAssign());
    require(!toyc::sema::alwaysReturns(*ifStmt),
            "if-else where only then returns should not always return");
}

void testIfElseOnlyElseReturns() {
    auto ifStmt = makeIf(makeAssign(), makeReturn());
    require(!toyc::sema::alwaysReturns(*ifStmt),
            "if-else where only else returns should not always return");
}

void testWhileDoesNotAlwaysReturn() {
    auto whileStmt = makeWhile(makeReturn());
    require(!toyc::sema::alwaysReturns(*whileStmt),
            "while loop should not always return (may not execute)");
}

void testWhileWithoutReturn() {
    auto whileStmt = makeWhile(makeAssign());
    require(!toyc::sema::alwaysReturns(*whileStmt),
            "while loop without return should not always return");
}

void testNestedIfElseBothReturn() {
    auto innerIf = makeIf(makeReturn(), makeReturn());
    auto outerIf = makeIf(
        std::unique_ptr<toyc::ast::Stmt>(std::move(innerIf)),
        makeReturn());
    require(toyc::sema::alwaysReturns(*outerIf),
            "nested if-else with all paths returning should always return");
}

void testEmptyStmtDoesNotReturn() {
    auto empty = std::make_unique<toyc::ast::EmptyStmt>();
    require(!toyc::sema::alwaysReturns(*empty),
            "empty statement should not return");
}

void testBreakDoesNotReturn() {
    auto breakStmt = std::make_unique<toyc::ast::BreakStmt>();
    require(!toyc::sema::alwaysReturns(*breakStmt),
            "break statement should not return");
}

void testContinueDoesNotReturn() {
    auto contStmt = std::make_unique<toyc::ast::ContinueStmt>();
    require(!toyc::sema::alwaysReturns(*contStmt),
            "continue statement should not return");
}

void testAssignDoesNotReturn() {
    auto assign = makeAssign();
    require(!toyc::sema::alwaysReturns(*assign),
            "assign statement should not return");
}

void testVoidReturnAlwaysReturns() {
    auto ret = makeVoidReturn();
    require(toyc::sema::alwaysReturns(*ret),
            "void return statement should always return");
}

} // namespace

int main() {
    testReturnAlwaysReturns();
    testEmptyBlockDoesNotAlwaysReturn();
    testBlockWithReturnAtEnd();
    testBlockWithReturnInMiddle();
    testBlockWithNoReturn();
    testIfElseBothReturn();
    testIfWithoutElse();
    testIfElseOnlyThenReturns();
    testIfElseOnlyElseReturns();
    testWhileDoesNotAlwaysReturn();
    testWhileWithoutReturn();
    testNestedIfElseBothReturn();
    testEmptyStmtDoesNotReturn();
    testBreakDoesNotReturn();
    testContinueDoesNotReturn();
    testAssignDoesNotReturn();
    testVoidReturnAlwaysReturns();

    std::cout << "All sema return check tests passed.\n";
    return 0;
}
