/// Semantic analysis scaffold — P0 stub.

#include "toyc/sema/semantic_model.h"

namespace toyc {

SymbolId SemanticModel::lookup(const std::string& /*name*/) const {
  // P0: no symbol table yet.
  return SymbolId{};
}

SymbolId SemanticModel::addSymbol(Symbol sym) {
  SymbolId id(static_cast<uint32_t>(symbols_.size()));
  sym.id = id;
  symbols_.push_back(std::move(sym));
  return id;
}

const Symbol* SemanticModel::getSymbol(SymbolId id) const {
  if (!id.valid() || id.value >= symbols_.size()) return nullptr;
  return &symbols_[id.value];
}

} // namespace toyc
