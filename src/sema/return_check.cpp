#include "sema/return_check.h"

namespace toyc::sema {

bool alwaysReturns(const ast::Stmt& stmt) {
    // ReturnStmt always returns.
    if (dynamic_cast<const ast::ReturnStmt*>(&stmt) != nullptr) {
        return true;
    }

    // BlockStmt: check if any statement in sequence always returns.
    if (const auto* block = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
        return blockAlwaysReturns(*block);
    }

    // IfStmt: both branches must exist and both must always return.
    if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        if (ifStmt->elseBranch == nullptr) {
            return false;
        }
        return alwaysReturns(*ifStmt->thenBranch) &&
               alwaysReturns(*ifStmt->elseBranch);
    }

    // WhileStmt: conservative — the loop body may not execute.
    if (dynamic_cast<const ast::WhileStmt*>(&stmt) != nullptr) {
        return false;
    }

    // All other statements (EmptyStmt, ExprStmt, AssignStmt, DeclStmt,
    // BreakStmt, ContinueStmt) do not return.
    return false;
}

bool blockAlwaysReturns(const ast::BlockStmt& block) {
    for (const auto& stmt : block.statements) {
        if (alwaysReturns(*stmt)) {
            return true;
        }
    }
    return false;
}

} // namespace toyc::sema
