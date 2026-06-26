#pragma once
/// AST to IR Lowering — converts AST + SemanticModel to Canonical Slot IR.

#include "toyc/ir/module.h"
#include "toyc/support/diagnostics.h"

#include <optional>

namespace toyc {

class CompUnit;
class SemanticModel;

/// Lowers a validated AST + SemanticModel to an IRModule.
class ASTToIRLowering {
public:
  ASTToIRLowering(const SemanticModel& semanticModel, DiagnosticEngine& diagnostics);

  /// Lower the compilation unit to an IRModule.
  /// Returns nullopt on error (diagnostics emitted).
  std::optional<Module> lower(const CompUnit& unit);

  /// True if any error was emitted during lowering.
  [[nodiscard]] bool hasError() const noexcept { return hasError_; }

private:
  const SemanticModel& sema_;
  DiagnosticEngine& diag_;
  bool hasError_ = false;
};

} // namespace toyc
