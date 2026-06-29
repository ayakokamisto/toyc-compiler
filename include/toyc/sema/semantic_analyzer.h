#pragma once
/// SemanticAnalyzer: performs semantic analysis on a ToyC AST.

#include "toyc/frontend/ast.h"
#include "toyc/sema/scope.h"
#include "toyc/sema/semantic_model.h"
#include "toyc/support/diagnostics.h"

#include <cstdint>
#include <optional>

namespace toyc {

/// Control flow summary for return-path checking.
struct FlowSummary {
  bool fallsThrough = true;     ///< Execution can continue past this statement.
  bool breaksCurrentLoop = false; ///< A break was encountered.
};

/// Semantic analyzer for ToyC.
class SemanticAnalyzer {
public:
  explicit SemanticAnalyzer(DiagnosticEngine& diagnostics);

  /// Analyze a compilation unit. Returns the semantic model.
  std::optional<SemanticModel> analyze(const CompUnit& unit);

  [[nodiscard]] bool hasError() const noexcept { return hasError_; }

private:
  // ── Top-level ─────────────────────────────────────────────────────────────
  void analyzeTopLevelItem(const TopLevelItem& item);
  void analyzeGlobalDecl(const GlobalDecl& decl);
  void analyzeFuncDef(const FuncDef& func);

  // ── Declarations ──────────────────────────────────────────────────────────
  SymbolId analyzeConstDecl(const ConstDecl& decl, bool isGlobal);
  SymbolId analyzeVarDecl(const VarDecl& decl, bool isGlobal);

  // ── Statements ────────────────────────────────────────────────────────────
  FlowSummary analyzeStmt(const Stmt& stmt, TypeKind enclosingFuncReturn,
                           bool inLoop);
  FlowSummary analyzeBlock(const BlockStmt& block, TypeKind enclosingFuncReturn,
                            bool inLoop);
  FlowSummary analyzeIfStmt(const IfStmt& stmt, TypeKind enclosingFuncReturn,
                             bool inLoop);
  FlowSummary analyzeWhileStmt(const WhileStmt& stmt, TypeKind enclosingFuncReturn);
  FlowSummary analyzeReturnStmt(const ReturnStmt& stmt, TypeKind enclosingFuncReturn);

  // ── Expressions ───────────────────────────────────────────────────────────
  ExprSemanticInfo analyzeExpr(const Expr& expr);
  ExprSemanticInfo analyzeIntegerLiteral(const IntegerLiteralExpr& expr);
  ExprSemanticInfo analyzeIdentifier(const IdentifierExpr& expr);
  ExprSemanticInfo analyzeCallExpr(const CallExpr& expr);
  ExprSemanticInfo analyzeUnaryExpr(const UnaryExpr& expr);
  ExprSemanticInfo analyzeBinaryExpr(const BinaryExpr& expr);
  ExprSemanticInfo applyBinaryOperator(const BinaryExpr& expr,
                                       const ExprSemanticInfo& lhsInfo,
                                       const ExprSemanticInfo& rhsInfo);

  // ── Constant evaluation helpers ───────────────────────────────────────────
  /// Evaluate a constant expression with symbol resolution (required mode).
  /// Reports errors for non-constant identifiers.
  std::optional<int32_t> evalConstWithSymbols(const Expr& expr);

  /// Probe whether an expression can be statically evaluated (probe mode).
  /// Returns nullopt for non-constant identifiers without reporting errors.
  std::optional<int32_t> probeStaticInitValue(const Expr& expr);

  // ── Helpers ───────────────────────────────────────────────────────────────
  void reportError(SourceLocation loc, std::string message);

  // ── State ─────────────────────────────────────────────────────────────────
  DiagnosticEngine& diag_;
  bool hasError_ = false;
  ScopeStack scopes_;
  SemanticModel model_;
  int loopDepth_ = 0;
};

} // namespace toyc
