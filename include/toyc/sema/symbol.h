#pragma once
/// Symbol table entries for ToyC semantic analysis.

#include "toyc/frontend/ast.h"
#include "toyc/support/ids.h"
#include "toyc/support/source_location.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace toyc {

/// Kinds of symbols in ToyC.
enum class SymbolKind : uint8_t {
  GlobalVariable,
  GlobalConstant,
  LocalVariable,
  LocalConstant,
  Parameter,
  Function,
};

/// A single symbol table entry.
struct Symbol {
  SymbolId id;
  std::string name;
  SymbolKind kind;
  TypeKind type = TypeKind::Int;       ///< For functions: return type.
  ScopeId scope;

  /// Pointer to the AST node that declared this symbol.
  const ASTNode* declaration = nullptr;

  /// For constants: compile-time evaluated value.
  std::optional<int32_t> constantValue;

  /// For functions: parameter types and names.
  std::vector<TypeKind> parameterTypes;
  std::vector<std::string> parameterNames;

  /// Whether this symbol has been fully defined (vs just forward-declared).
  bool isDefined = false;

  /// Helper: is this a constant (global or local)?
  [[nodiscard]] bool isConstant() const {
    return kind == SymbolKind::GlobalConstant || kind == SymbolKind::LocalConstant;
  }

  /// Helper: is this a variable (global or local)?
  [[nodiscard]] bool isVariable() const {
    return kind == SymbolKind::GlobalVariable || kind == SymbolKind::LocalVariable;
  }

  /// Helper: is this assignable (variable or parameter)?
  [[nodiscard]] bool isAssignable() const {
    return isVariable() || kind == SymbolKind::Parameter;
  }
};

} // namespace toyc
