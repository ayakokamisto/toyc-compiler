#pragma once

#include "ast/ast.h"

namespace toyc::sema {

[[nodiscard]] bool alwaysReturns(const ast::Stmt& stmt);

[[nodiscard]] bool blockAlwaysReturns(const ast::BlockStmt& block);

} // namespace toyc::sema
