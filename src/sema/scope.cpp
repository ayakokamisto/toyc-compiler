/// Scope implementation — nested scope management.

#include "toyc/sema/scope.h"

namespace toyc {

ScopeStack::ScopeStack() {
  // Create the global scope.
  Scope global;
  global.id = ScopeId(0);
  global.parent = std::nullopt;
  scopes_.push_back(std::move(global));
  current_ = 0;
}

ScopeId ScopeStack::pushScope() {
  Scope child;
  child.id = ScopeId(static_cast<uint32_t>(scopes_.size()));
  child.parent = scopes_[current_].id;
  scopes_.push_back(std::move(child));
  current_ = scopes_.size() - 1;
  return scopes_[current_].id;
}

void ScopeStack::popScope() {
  // Don't pop the global scope.
  if (scopes_[current_].parent.has_value()) {
    auto parentId = scopes_[current_].parent.value();
    // Find parent by id.
    for (std::size_t i = 0; i < scopes_.size(); ++i) {
      if (scopes_[i].id == parentId) {
        current_ = i;
        return;
      }
    }
  }
}

std::optional<SymbolId> ScopeStack::lookupCurrent(std::string_view name) const {
  const auto& syms = scopes_[current_].symbols;
  auto it = syms.find(std::string(name));
  if (it != syms.end()) return it->second;
  return std::nullopt;
}

std::optional<SymbolId> ScopeStack::lookupVisible(std::string_view name) const {
  std::size_t idx = current_;
  while (true) {
    const auto& syms = scopes_[idx].symbols;
    auto it = syms.find(std::string(name));
    if (it != syms.end()) return it->second;

    if (scopes_[idx].parent.has_value()) {
      auto parentId = scopes_[idx].parent.value();
      bool found = false;
      for (std::size_t i = 0; i < scopes_.size(); ++i) {
        if (scopes_[i].id == parentId) {
          idx = i;
          found = true;
          break;
        }
      }
      if (!found) break;
    } else {
      break;
    }
  }
  return std::nullopt;
}

bool ScopeStack::declare(const std::string& name, SymbolId symbol) {
  auto& syms = scopes_[current_].symbols;
  if (syms.count(name)) return false;
  syms[name] = symbol;
  return true;
}

ScopeId ScopeStack::currentScopeId() const {
  return scopes_[current_].id;
}

bool ScopeStack::isGlobalScope() const {
  return !scopes_[current_].parent.has_value();
}

} // namespace toyc
