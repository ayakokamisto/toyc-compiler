/// SemanticModel implementation.

#include "toyc/sema/semantic_model.h"

namespace toyc {

std::string_view semanticTypeName(SemanticType t) {
  switch (t) {
    case SemanticType::Int:   return "int";
    case SemanticType::Void:  return "void";
    case SemanticType::Error: return "<error>";
  }
  return "<unknown>";
}

// ── Symbol access ─────────────────────────────────────────────────────────

const Symbol& SemanticModel::symbol(SymbolId id) const {
  return symbols_[id.value];
}

Symbol& SemanticModel::symbol(SymbolId id) {
  return symbols_[id.value];
}

const std::vector<Symbol>& SemanticModel::symbols() const {
  return symbols_;
}

SymbolId SemanticModel::addSymbol(Symbol sym) {
  SymbolId id(static_cast<uint32_t>(symbols_.size()));
  sym.id = id;
  symbols_.push_back(std::move(sym));
  return id;
}

// ── Node → Symbol resolution ──────────────────────────────────────────────

void SemanticModel::resolveNode(const ASTNode* node, SymbolId id) {
  nodeToSymbol_[node] = id;
}

std::optional<SymbolId> SemanticModel::resolvedSymbol(const ASTNode& node) const {
  auto it = nodeToSymbol_.find(&node);
  if (it != nodeToSymbol_.end()) return it->second;
  return std::nullopt;
}

// ── Expression semantic info ──────────────────────────────────────────────

void SemanticModel::setExprInfo(const Expr* expr, ExprSemanticInfo info) {
  exprInfo_[expr] = info;
}

std::optional<ExprSemanticInfo> SemanticModel::exprInfo(const Expr& expr) const {
  auto it = exprInfo_.find(&expr);
  if (it != exprInfo_.end()) return it->second;
  return std::nullopt;
}

// ── Global init classification ────────────────────────────────────────────

void SemanticModel::setGlobalInitInfo(const Decl* decl, GlobalInitInfo info) {
  globalInit_[decl] = info;
}

std::optional<GlobalInitInfo> SemanticModel::globalInitInfo(const Decl& decl) const {
  auto it = globalInit_.find(&decl);
  if (it != globalInit_.end()) return it->second;
  return std::nullopt;
}

} // namespace toyc
