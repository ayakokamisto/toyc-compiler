#pragma once
/// Nested scope and scope stack for ToyC semantic analysis.

#include "toyc/support/ids.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace toyc {

/// A single scope containing name → SymbolId mappings.
struct Scope {
  ScopeId id;
  std::optional<ScopeId> parent;
  std::unordered_map<std::string, SymbolId> symbols;
};

/// Manages nested scopes during semantic analysis.
class ScopeStack {
public:
  ScopeStack();

  /// Create a new child scope and push it. Returns the new scope's id.
  ScopeId pushScope();

  /// Pop the current scope. Must not pop the global scope.
  void popScope();

  /// Look up a name in the current scope only.
  [[nodiscard]] std::optional<SymbolId> lookupCurrent(std::string_view name) const;

  /// Look up a name in the current scope and all parent scopes.
  [[nodiscard]] std::optional<SymbolId> lookupVisible(std::string_view name) const;

  /// Declare a symbol in the current scope.
  /// Returns true if successful, false if the name already exists in this scope.
  bool declare(const std::string& name, SymbolId symbol);

  /// The current (innermost) scope id.
  [[nodiscard]] ScopeId currentScopeId() const;

  /// Check if we are in the global scope.
  [[nodiscard]] bool isGlobalScope() const;

private:
  std::vector<Scope> scopes_;
  std::size_t current_ = 0;
};

} // namespace toyc
