#include "sema/semantic_model.h"

namespace toyc::sema {

const Symbol* SemanticModel::registerSymbol(std::unique_ptr<Symbol> symbol) {
    const Symbol* ptr = symbol.get();
    symbolStorage_.push_back(std::move(symbol));
    return ptr;
}

const FunctionSymbol* SemanticModel::registerFunctionSymbol(
    std::unique_ptr<FunctionSymbol> symbol) {
    const FunctionSymbol* ptr = symbol.get();
    functionSymbols_.push_back(ptr);
    symbolStorage_.push_back(std::move(symbol));
    return ptr;
}

void SemanticModel::setBinding(const ast::DeclRefExpr* expr,
                               const Symbol* symbol) {
    bindings_[expr] = symbol;
}

void SemanticModel::setCallee(const ast::CallExpr* call,
                              const FunctionSymbol* func) {
    callees_[call] = func;
}

void SemanticModel::setExprType(const ast::Expr* expr, Type type) {
    exprTypes_[expr] = type;
}

void SemanticModel::setConstantValue(const ast::Expr* expr,
                                     std::int32_t value) {
    constantValues_[expr] = value;
}

void SemanticModel::setDeclSymbol(const ast::Decl* decl,
                                  const Symbol* symbol) {
    declSymbols_[decl] = symbol;
    // Track global variables and constants (scopeDepth == 0)
    if (symbol->kind == SymbolKind::Variable ||
        symbol->kind == SymbolKind::Constant) {
        if (symbol->scopeDepth == 0) {
            globalSymbols_.push_back(symbol);
        }
    }
}

const Symbol* SemanticModel::lookupBinding(
    const ast::DeclRefExpr& expr) const {
    auto it = bindings_.find(&expr);
    return it != bindings_.end() ? it->second : nullptr;
}

const FunctionSymbol* SemanticModel::lookupCallee(
    const ast::CallExpr& call) const {
    auto it = callees_.find(&call);
    return it != callees_.end() ? it->second : nullptr;
}

Type SemanticModel::getExprType(const ast::Expr& expr) const {
    auto it = exprTypes_.find(&expr);
    // Default to Int for error recovery; IRGenerator should never query
    // un-analyzed expressions under normal operation.
    return it != exprTypes_.end() ? it->second : Type::Int;
}

std::optional<std::int32_t> SemanticModel::getConstantValue(
    const ast::Expr& expr) const {
    auto it = constantValues_.find(&expr);
    if (it != constantValues_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const Symbol* SemanticModel::getDeclSymbol(const ast::Decl& decl) const {
    auto it = declSymbols_.find(&decl);
    return it != declSymbols_.end() ? it->second : nullptr;
}

const std::vector<const Symbol*>& SemanticModel::globalSymbols() const noexcept {
    return globalSymbols_;
}

const std::vector<const FunctionSymbol*>& SemanticModel::functionSymbols()
    const noexcept {
    return functionSymbols_;
}

} // namespace toyc::sema
