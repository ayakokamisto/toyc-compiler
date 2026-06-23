#pragma once

#include "ast/ast.h"
#include "common/diagnostic.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace toyc::sema {

class SemanticModel;

std::optional<std::int32_t> evaluateConstExpr(
    const ast::Expr& expr,
    const SemanticModel& model,
    std::vector<Diagnostic>& diagnostics);

} // namespace toyc::sema
