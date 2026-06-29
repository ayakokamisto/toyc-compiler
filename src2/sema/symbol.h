#pragma once

#include "sema/type.h"
#include "ast/ast.h"

#include <string>
#include <vector>

namespace toyc::sema {

enum class SymbolKind {
    Variable,
    Constant,
    Parameter,
    Function,
};

struct Symbol {
    SymbolKind kind;
    std::string name;
    Type type;
    const ast::Decl* decl;
    int scopeDepth;

    virtual ~Symbol() = default;

    Symbol(SymbolKind kind, std::string name, Type type, const ast::Decl* decl,
           int scopeDepth)
        : kind(kind), name(std::move(name)), type(type), decl(decl),
          scopeDepth(scopeDepth) {}
};

struct FunctionSymbol final : Symbol {
    std::vector<Type> paramTypes;

    FunctionSymbol(std::string name, Type returnType, const ast::Decl* decl,
                   int scopeDepth, std::vector<Type> paramTypes)
        : Symbol(SymbolKind::Function, std::move(name), returnType, decl,
                 scopeDepth),
          paramTypes(std::move(paramTypes)) {}
};

} // namespace toyc::sema
