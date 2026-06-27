#pragma once
/// Compile-time constant expression evaluator for ToyC.

#include "toyc/frontend/ast.h"
#include "toyc/support/diagnostics.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace toyc {

/// Result state for constant evaluation.
enum class ConstEvalState : uint8_t {
  Known,    ///< Successfully evaluated to a constant value.
  Unknown,  ///< Not a constant expression (references variables, calls, etc.).
  Error,    ///< A constant expression with an error (overflow, div-by-zero, etc.).
};

/// Result of constant evaluation.
struct ConstEvalResult {
  ConstEvalState state = ConstEvalState::Unknown;
  int32_t value = 0;
};

/// Callback to resolve an identifier name to a constant value.
/// Returns nullopt if the identifier is not a known constant.
using ConstLookup = std::function<std::optional<int32_t>(std::string_view)>;

/// Evaluate a constant expression with optional identifier resolution.
/// @param expr       The expression to evaluate.
/// @param diag       Diagnostic engine for error reporting.
/// @param lookup     Optional callback to resolve constant identifiers. May be null.
/// @param negate     True if this is the operand of unary minus (for INT32_MIN).
ConstEvalResult evaluateConstExpr(const Expr& expr, DiagnosticEngine& diag,
                                  const ConstLookup* lookup = nullptr,
                                  bool negate = false);

/// Parse an unsigned integer literal from raw text.
std::optional<uint64_t> parseUnsignedMagnitude(std::string_view raw);

} // namespace toyc
