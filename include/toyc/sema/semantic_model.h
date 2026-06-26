#pragma once
/// SemanticModel: symbol tables, type info, and semantic analysis results.
/// This is a P0 placeholder — real analysis will be implemented in P3.

#include "toyc/sema/symbol.h"
#include "toyc/support/ids.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace toyc {

/// Holds all semantic analysis results for a compilation unit.
/// Will contain symbol tables, scopes, and type information.
class SemanticModel {
public:
  SemanticModel() = default;

  /// Will be implemented in P3.
  /// Placeholder: always returns an invalid SymbolId.
  [[nodiscard]] SymbolId lookup(const std::string& name) const;

  /// Register a symbol (placeholder).
  SymbolId addSymbol(Symbol sym);

  [[nodiscard]] const Symbol* getSymbol(SymbolId id) const;

private:
  std::vector<Symbol> symbols_;
};

} // namespace toyc
