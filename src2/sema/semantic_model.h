#pragma once

#include "sema/symbol.h"
#include "sema/type.h"
#include "ast/ast.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace toyc::sema {

class SemanticModel {
public:
    SemanticModel() = default;

    // --- Symbol registration (called by Sema) ---
    const Symbol* registerSymbol(std::unique_ptr<Symbol> symbol);
    const FunctionSymbol* registerFunctionSymbol(
        std::unique_ptr<FunctionSymbol> symbol);

    // --- Side-table population (called by Sema) ---
    void setBinding(const ast::DeclRefExpr* expr, const Symbol* symbol);
    void setCallee(const ast::CallExpr* call, const FunctionSymbol* func);
    void setExprType(const ast::Expr* expr, Type type);
    void setConstantValue(const ast::Expr* expr, std::int32_t value);
    void setDeclSymbol(const ast::Decl* decl, const Symbol* symbol);

    // --- Public query API (used by IRGenerator and tests) ---
    [[nodiscard]] const Symbol* lookupBinding(
        const ast::DeclRefExpr& expr) const;
    [[nodiscard]] const FunctionSymbol* lookupCallee(
        const ast::CallExpr& call) const;
    [[nodiscard]] Type getExprType(const ast::Expr& expr) const;
    [[nodiscard]] std::optional<std::int32_t> getConstantValue(
        const ast::Expr& expr) const;
    [[nodiscard]] const Symbol* getDeclSymbol(const ast::Decl& decl) const;

    // --- Iteration over top-level symbols (for IRGenerator) ---
    [[nodiscard]] const std::vector<const Symbol*>& globalSymbols()
        const noexcept;
    [[nodiscard]] const std::vector<const FunctionSymbol*>& functionSymbols()
        const noexcept;

private:
    // Owned symbol storage. Raw pointers into these are stable.
    std::vector<std::unique_ptr<Symbol>> symbolStorage_;

    // Ordered lists for IRGenerator consumption (preserving source order)
    std::vector<const Symbol*> globalSymbols_;
    std::vector<const FunctionSymbol*> functionSymbols_;

    // Side-tables keyed by AST node address (stable due to unique_ptr ownership)
    std::unordered_map<const ast::DeclRefExpr*, const Symbol*> bindings_;
    std::unordered_map<const ast::CallExpr*, const FunctionSymbol*> callees_;
    std::unordered_map<const ast::Expr*, Type> exprTypes_;
    std::unordered_map<const ast::Expr*, std::int32_t> constantValues_;
    std::unordered_map<const ast::Decl*, const Symbol*> declSymbols_;
};

} // namespace toyc::sema
