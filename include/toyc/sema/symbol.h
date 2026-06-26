#pragma once
/// Symbol table entries for ToyC.

#include "toyc/sema/type.h"
#include "toyc/support/ids.h"
#include "toyc/support/source_location.h"

#include <string>

namespace toyc {

/// Kinds of symbols in ToyC.
enum class SymbolKind : uint8_t {
  Variable,
  Constant,
  Function,
  Parameter,
};

/// A single symbol table entry.
struct Symbol {
  SymbolId id;
  SymbolKind kind;
  std::string name;
  Type type;
  SourceLocation declLoc;
  bool isGlobal = false;

  // For constants: compile-time value (stored as int64_t to avoid overflow).
  bool hasConstValue = false;
  int64_t constValue = 0;
};

} // namespace toyc
