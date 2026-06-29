#pragma once

#include "sema/symbol.h"

#include <string>
#include <unordered_map>

namespace toyc::sema {

class Scope {
public:
    explicit Scope(Scope* parent = nullptr) : parent_(parent), depth_(parent ? parent->depth_ + 1 : 0) {}

    // Insert a symbol into this scope level.
    // Returns false if the name already exists in THIS scope (duplicate).
    bool insert(const std::string& name, const Symbol* symbol) {
        return symbols_.try_emplace(name, symbol).second;
    }

    // Look up a name, walking up parent scopes (inner-to-outer shadowing).
    // Returns nullptr if not found in this scope or any ancestor.
    [[nodiscard]] const Symbol* lookup(const std::string& name) const {
        for (const Scope* scope = this; scope != nullptr; scope = scope->parent_) {
            auto it = scope->symbols_.find(name);
            if (it != scope->symbols_.end()) {
                return it->second;
            }
        }
        return nullptr;
    }

    // Look up a name in this scope only (no ancestor walk).
    [[nodiscard]] const Symbol* lookupLocal(const std::string& name) const {
        auto it = symbols_.find(name);
        return it != symbols_.end() ? it->second : nullptr;
    }

    [[nodiscard]] Scope* parent() const noexcept { return parent_; }
    [[nodiscard]] int depth() const noexcept { return depth_; }

private:
    Scope* parent_;
    int depth_;
    std::unordered_map<std::string, const Symbol*> symbols_;
};

} // namespace toyc::sema
