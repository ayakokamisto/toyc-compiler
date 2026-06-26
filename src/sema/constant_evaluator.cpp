/// Constant expression evaluator implementation.

#include "toyc/sema/constant_evaluator.h"

#include <climits>

namespace toyc {

// ── Unsigned literal parsing ─────────────────────────────────────────────

std::optional<uint64_t> parseUnsignedMagnitude(std::string_view raw) {
  if (raw.empty()) return std::nullopt;
  uint64_t value = 0;
  for (char c : raw) {
    if (c < '0' || c > '9') return std::nullopt;
    uint64_t digit = static_cast<uint64_t>(c - '0');
    if (value > UINT64_MAX / 10) return std::nullopt;
    value *= 10;
    if (value > UINT64_MAX - digit) return std::nullopt;
    value += digit;
  }
  return value;
}

// ── Helpers ──────────────────────────────────────────────────────────────

static constexpr int64_t I32_MIN = static_cast<int64_t>(INT32_MIN);
static constexpr int64_t I32_MAX = static_cast<int64_t>(INT32_MAX);

static bool fitsI32(int64_t v) {
  return v >= I32_MIN && v <= I32_MAX;
}

static ConstEvalResult known(int32_t v) {
  return {ConstEvalState::Known, v};
}

static ConstEvalResult unknown() {
  return {ConstEvalState::Unknown, 0};
}

static ConstEvalResult evalError() {
  return {ConstEvalState::Error, 0};
}

// ── Forward declaration ──────────────────────────────────────────────────

static ConstEvalResult evalImpl(const Expr& expr, DiagnosticEngine& diag,
                                ConstLookup& lookup, bool negate);

// ── Integer literal ──────────────────────────────────────────────────────

static ConstEvalResult evalIntegerLiteral(const IntegerLiteralExpr& lit,
                                          DiagnosticEngine& diag,
                                          bool negate) {
  auto mag = parseUnsignedMagnitude(lit.rawValue());
  if (!mag.has_value()) {
    diag.error(SourceLocation{}, "integer literal out of range");
    return evalError();
  }

  uint64_t uval = mag.value();

  if (negate) {
    if (uval == 2147483648ULL) {
      return known(INT32_MIN);
    }
    if (uval > static_cast<uint64_t>(I32_MAX) + 1) {
      diag.error(SourceLocation{}, "integer literal out of range");
      return evalError();
    }
    return known(-static_cast<int32_t>(uval));
  }

  if (uval > static_cast<uint64_t>(INT32_MAX)) {
    diag.error(SourceLocation{}, "integer literal out of range");
    return evalError();
  }
  return known(static_cast<int32_t>(uval));
}

// ── Identifier ───────────────────────────────────────────────────────────

static ConstEvalResult evalIdentifier(const IdentifierExpr& id,
                                      DiagnosticEngine& diag,
                                      ConstLookup& lookup) {
  if (lookup) {
    auto val = lookup(id.name());
    if (val.has_value()) return known(val.value());
    // lookup returned nullopt — identifier is not a known constant.
    // This is an error in a constant context (e.g., const initializer).
    diag.error(id.range().begin,
               "'" + std::string(id.name()) + "' is not a constant expression");
    return evalError();
  }
  return unknown();
}

// ── Unary expression ─────────────────────────────────────────────────────

static ConstEvalResult evalUnary(const UnaryExpr& expr, DiagnosticEngine& diag,
                                 ConstLookup& lookup, bool negate) {
  if (expr.op() == UnaryOperator::Minus) {
    // Pass negate=true to detect INT32_MIN.
    auto inner = evalImpl(*expr.operand(), diag, lookup, true);
    if (inner.state == ConstEvalState::Known) return inner;
    if (inner.state == ConstEvalState::Error) return evalError();
    return unknown();
  }

  auto inner = evalImpl(*expr.operand(), diag, lookup, false);
  if (inner.state == ConstEvalState::Error) return evalError();
  if (inner.state == ConstEvalState::Unknown) return unknown();

  int32_t v = inner.value;
  switch (expr.op()) {
    case UnaryOperator::Plus:
      return known(v);
    case UnaryOperator::Minus: {
      int64_t r = -static_cast<int64_t>(v);
      if (!fitsI32(r)) {
        diag.error(SourceLocation{}, "constant expression overflow");
        return evalError();
      }
      return known(static_cast<int32_t>(r));
    }
    case UnaryOperator::LogicalNot:
      return known(v == 0 ? 1 : 0);
  }
  return unknown();
}

// ── Binary expression ────────────────────────────────────────────────────

static ConstEvalResult evalBinary(const BinaryExpr& expr, DiagnosticEngine& diag,
                                  ConstLookup& lookup) {
  // Short-circuit &&
  if (expr.op() == BinaryOperator::LogicalAnd) {
    auto lhs = evalImpl(*expr.lhs(), diag, lookup, false);
    if (lhs.state == ConstEvalState::Error) return evalError();
    if (lhs.state == ConstEvalState::Known && lhs.value == 0) {
      return known(0);
    }
    auto rhs = evalImpl(*expr.rhs(), diag, lookup, false);
    if (rhs.state == ConstEvalState::Error) return evalError();
    if (lhs.state == ConstEvalState::Known && rhs.state == ConstEvalState::Known) {
      return known((lhs.value != 0 && rhs.value != 0) ? 1 : 0);
    }
    return unknown();
  }

  // Short-circuit ||
  if (expr.op() == BinaryOperator::LogicalOr) {
    auto lhs = evalImpl(*expr.lhs(), diag, lookup, false);
    if (lhs.state == ConstEvalState::Error) return evalError();
    if (lhs.state == ConstEvalState::Known && lhs.value != 0) {
      return known(1);
    }
    auto rhs = evalImpl(*expr.rhs(), diag, lookup, false);
    if (rhs.state == ConstEvalState::Error) return evalError();
    if (lhs.state == ConstEvalState::Known && rhs.state == ConstEvalState::Known) {
      return known((lhs.value != 0 || rhs.value != 0) ? 1 : 0);
    }
    return unknown();
  }

  // Non-short-circuit: evaluate both.
  auto lhs = evalImpl(*expr.lhs(), diag, lookup, false);
  auto rhs = evalImpl(*expr.rhs(), diag, lookup, false);

  if (lhs.state == ConstEvalState::Error || rhs.state == ConstEvalState::Error) {
    return evalError();
  }
  if (lhs.state == ConstEvalState::Unknown || rhs.state == ConstEvalState::Unknown) {
    return unknown();
  }

  int64_t l = lhs.value;
  int64_t r = rhs.value;
  int64_t result = 0;

  switch (expr.op()) {
    case BinaryOperator::Add:
      result = l + r;
      if (!fitsI32(result)) {
        diag.error(SourceLocation{}, "constant expression overflow");
        return evalError();
      }
      return known(static_cast<int32_t>(result));

    case BinaryOperator::Subtract:
      result = l - r;
      if (!fitsI32(result)) {
        diag.error(SourceLocation{}, "constant expression overflow");
        return evalError();
      }
      return known(static_cast<int32_t>(result));

    case BinaryOperator::Multiply:
      result = l * r;
      if (!fitsI32(result)) {
        diag.error(SourceLocation{}, "constant expression overflow");
        return evalError();
      }
      return known(static_cast<int32_t>(result));

    case BinaryOperator::Divide:
      if (r == 0) {
        diag.error(SourceLocation{}, "division by zero in constant expression");
        return evalError();
      }
      if (l == I32_MIN && r == -1) {
        diag.error(SourceLocation{}, "constant expression overflow");
        return evalError();
      }
      return known(static_cast<int32_t>(l / r));

    case BinaryOperator::Modulo:
      if (r == 0) {
        diag.error(SourceLocation{}, "modulo by zero in constant expression");
        return evalError();
      }
      if (l == I32_MIN && r == -1) {
        diag.error(SourceLocation{}, "constant expression overflow");
        return evalError();
      }
      return known(static_cast<int32_t>(l % r));

    case BinaryOperator::Equal:        return known(l == r ? 1 : 0);
    case BinaryOperator::NotEqual:     return known(l != r ? 1 : 0);
    case BinaryOperator::Less:         return known(l < r ? 1 : 0);
    case BinaryOperator::LessEqual:    return known(l <= r ? 1 : 0);
    case BinaryOperator::Greater:      return known(l > r ? 1 : 0);
    case BinaryOperator::GreaterEqual: return known(l >= r ? 1 : 0);

    default:
      return unknown();
  }
}

// ── Main dispatch ────────────────────────────────────────────────────────

static ConstEvalResult evalImpl(const Expr& expr, DiagnosticEngine& diag,
                                ConstLookup& lookup, bool negate) {
  switch (expr.kind()) {
    case ASTKind::IntegerLiteralExpr:
      return evalIntegerLiteral(
          static_cast<const IntegerLiteralExpr&>(expr), diag, negate);

    case ASTKind::IdentifierExpr:
      return evalIdentifier(
          static_cast<const IdentifierExpr&>(expr), diag, lookup);

    case ASTKind::UnaryExpr:
      return evalUnary(
          static_cast<const UnaryExpr&>(expr), diag, lookup, negate);

    case ASTKind::BinaryExpr:
      return evalBinary(
          static_cast<const BinaryExpr&>(expr), diag, lookup);

    case ASTKind::CallExpr:
      diag.error(SourceLocation{}, "function call not allowed in constant expression");
      return evalError();

    default:
      return unknown();
  }
}

// ── Public API ───────────────────────────────────────────────────────────

ConstEvalResult evaluateConstExpr(const Expr& expr, DiagnosticEngine& diag,
                                  ConstLookup lookup, bool negate) {
  return evalImpl(expr, diag, lookup, negate);
}

} // namespace toyc
