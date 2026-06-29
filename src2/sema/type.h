#pragma once

#include "ast/ast.h"

namespace toyc::sema {

enum class Type {
    Int,
    Void,
};

inline Type fromAstTypeKind(ast::TypeKind kind) {
    return (kind == ast::TypeKind::Int) ? Type::Int : Type::Void;
}

} // namespace toyc::sema
