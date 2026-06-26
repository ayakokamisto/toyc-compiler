#pragma once
/// SemanticModel: holds all semantic analysis results for a compilation unit.
/// Separate from AST — no SymbolId/Type/const-value written into AST nodes.

#include "toyc/frontend/ast.h"
#include "toyc/sema/symbol.h"
#include "toyc/support/ids.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace toyc {

/// Semantic type with error sentinel.
enum class SemanticType : uint8_t {
  Int,
  Void,
  Error,
};

std::string_view semanticTypeName(SemanticType t);

/// Per-expression semantic info.
struct ExprSemanticInfo {
  SemanticType type = SemanticType::Error;
  std::optional<int32_t> constantValue;
};

/// Global initializer classification for P4.
enum class GlobalInitKind : uint8_t {
  StaticConstant,  ///< Value known at compile time.
  Runtime,         ///< Requires runtime initialization.
};

struct GlobalInitInfo {
  GlobalInitKind kind = GlobalInitKind::Runtime;
  std::optional<int32_t> staticValue;
};

/// Holds all semantic analysis results for a compilation unit.
class SemanticModel {
public:
  SemanticModel() = default;

  // ── Symbol access ────────────────────────────────────────────────────────

  /// Get a symbol by id.
  [[nodiscard]] const Symbol& symbol(SymbolId id) const;
  [[nodiscard]] Symbol& symbol(SymbolId id);

  /// All symbols (indexed by id.value).
  [[nodiscard]] const std::vector<Symbol>& symbols() const;

  /// Add a new symbol. Returns its id.
  SymbolId addSymbol(Symbol sym);

  // ── Node → Symbol resolution ─────────────────────────────────────────────

  void resolveNode(const ASTNode* node, SymbolId id);
  [[nodiscard]] std::optional<SymbolId> resolvedSymbol(const ASTNode& node) const;

  // ── Expression semantic info ─────────────────────────────────────────────

  void setExprInfo(const Expr* expr, ExprSemanticInfo info);
  [[nodiscard]] std::optional<ExprSemanticInfo> exprInfo(const Expr& expr) const;

  // ── Global init classification ───────────────────────────────────────────

  void setGlobalInitInfo(const Decl* decl, GlobalInitInfo info);
  [[nodiscard]] std::optional<GlobalInitInfo> globalInitInfo(const Decl& decl) const;

private:
  std::vector<Symbol> symbols_;
  std::unordered_map<const ASTNode*, SymbolId> nodeToSymbol_;
  std::unordered_map<const Expr*, ExprSemanticInfo> exprInfo_;
  std::unordered_map<const Decl*, GlobalInitInfo> globalInit_;
};

/// Dump the semantic model to an output stream.
void dumpSema(const SemanticModel& model, const CompUnit& unit,
              std::ostream& output);

} // namespace toyc
