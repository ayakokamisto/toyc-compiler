#include "sema/const_eval.h"
#include "sema/semantic_model.h"
#include "common/diagnostic.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace toyc::sema {
namespace {

// Parse a decimal integer literal string to int64_t.
// Returns nullopt if the string is not a valid non-negative integer
// or if it exceeds the maximum representable value (2147483648,
// which is the absolute value of INT32_MIN, only valid under unary minus).
std::optional<std::int64_t> parseIntLiteral(const std::string& spelling) {
    if (spelling.empty()) {
        return std::nullopt;
    }

    // The lexer guarantees the spelling matches [0-9]+ with no leading zeros
    // (except "0" itself), so we only need to handle overflow.
    char* end = nullptr;
    errno = 0;
    const std::int64_t value = std::strtoll(spelling.c_str(), &end, 10);

    if (errno == ERANGE) {
        return std::nullopt;
    }
    if (end != spelling.c_str() + spelling.size()) {
        return std::nullopt;  // partial parse (should not happen)
    }
    // strtoll may return negative for huge values; reject those.
    if (value < 0) {
        return std::nullopt;
    }
    return value;
}

// Internal recursive evaluator returning int64_t to catch intermediate overflow
// before the final int32 range check.
std::optional<std::int64_t> evalInternal(
    const ast::Expr& expr,
    const SemanticModel& model,
    std::vector<Diagnostic>& diagnostics) {

    // --- IntLiteralExpr ---
    if (const auto* lit = dynamic_cast<const ast::IntLiteralExpr*>(&expr)) {
        const auto parsed = parseIntLiteral(lit->spelling);
        if (!parsed.has_value()) {
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                expr.range,
                "integer literal '" + lit->spelling + "' exceeds representable range"
            });
            return std::nullopt;
        }
        const std::int64_t value = *parsed;
        // The only literal value > INT32_MAX that is valid is 2147483648,
        // and only when negated. We allow it through here; the caller
        // (UnaryExpr Minus handler) validates the context.
        // Any value strictly greater than 2147483648 is always invalid.
        if (value > 2147483648LL) {
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                expr.range,
                "integer literal '" + lit->spelling + "' exceeds int32 range"
            });
            return std::nullopt;
        }
        return value;
    }

    // --- DeclRefExpr ---
    if (const auto* ref = dynamic_cast<const ast::DeclRefExpr*>(&expr)) {
        // Try the model's constant-value side table first.
        auto cv = model.getConstantValue(expr);
        if (cv.has_value()) {
            return static_cast<std::int64_t>(*cv);
        }
        // If the model has no constant value, determine why.
        const Symbol* sym = model.lookupBinding(*ref);
        if (sym == nullptr) {
            // Unresolved reference — Sema should have reported this already.
            return std::nullopt;
        }
        if (sym->kind == SymbolKind::Constant) {
            // A constant whose value we could not resolve.
            return std::nullopt;
        }
        diagnostics.push_back({
            DiagnosticSeverity::Error,
            expr.range,
            "identifier '" + sym->name + "' is not a compile-time constant"
        });
        return std::nullopt;
    }

    // --- UnaryExpr ---
    if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        if (unary->op == TokenKind::Plus) {
            return evalInternal(*unary->operand, model, diagnostics);
        }

        if (unary->op == TokenKind::Minus) {
            const auto operandVal = evalInternal(*unary->operand, model, diagnostics);
            if (!operandVal.has_value()) {
                return std::nullopt;
            }
            // -2147483648 is the valid INT32_MIN representation.
            // evalInternal for IntLiteralExpr("2147483648") returns 2147483648,
            // and negating it here gives -2147483648, which fits in int32.
            return -(*operandVal);
        }

        if (unary->op == TokenKind::Bang) {
            const auto operandVal = evalInternal(*unary->operand, model, diagnostics);
            if (!operandVal.has_value()) {
                return std::nullopt;
            }
            // Logical NOT: non-zero → 0, zero → 1
            return (*operandVal != 0) ? 0 : 1;
        }

        return std::nullopt;  // unknown unary operator
    }

    // --- BinaryExpr ---
    if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        const auto leftVal = evalInternal(*binary->left, model, diagnostics);
        const auto rightVal = evalInternal(*binary->right, model, diagnostics);
        if (!leftVal.has_value() || !rightVal.has_value()) {
            return std::nullopt;
        }

        const std::int64_t L = *leftVal;
        const std::int64_t R = *rightVal;

        switch (binary->op) {
        case TokenKind::Plus:
            return L + R;
        case TokenKind::Minus:
            return L - R;
        case TokenKind::Star:
            return L * R;
        case TokenKind::Slash:
            if (R == 0) {
                diagnostics.push_back({
                    DiagnosticSeverity::Error,
                    expr.range,
                    "division by zero"
                });
                return std::nullopt;
            }
            return L / R;  // C++11: truncates toward zero
        case TokenKind::Percent:
            if (R == 0) {
                diagnostics.push_back({
                    DiagnosticSeverity::Error,
                    expr.range,
                    "remainder by zero"
                });
                return std::nullopt;
            }
            return L % R;
        case TokenKind::Less:
            return (L < R) ? 1 : 0;
        case TokenKind::Greater:
            return (L > R) ? 1 : 0;
        case TokenKind::LessEqual:
            return (L <= R) ? 1 : 0;
        case TokenKind::GreaterEqual:
            return (L >= R) ? 1 : 0;
        case TokenKind::EqualEqual:
            return (L == R) ? 1 : 0;
        case TokenKind::BangEqual:
            return (L != R) ? 1 : 0;
        case TokenKind::AmpAmp:
            return (L && R) ? 1 : 0;
        case TokenKind::PipePipe:
            return (L || R) ? 1 : 0;
        default:
            return std::nullopt;  // unknown binary operator
        }
    }

    // --- CallExpr ---
    if (dynamic_cast<const ast::CallExpr*>(&expr) != nullptr) {
        diagnostics.push_back({
            DiagnosticSeverity::Error,
            expr.range,
            "function call is not a compile-time constant expression"
        });
        return std::nullopt;
    }

    // Unknown expression type — cannot evaluate.
    return std::nullopt;
}

} // namespace

std::optional<std::int32_t> evaluateConstExpr(
    const ast::Expr& expr,
    const SemanticModel& model,
    std::vector<Diagnostic>& diagnostics) {

    const auto internalResult = evalInternal(expr, model, diagnostics);
    if (!internalResult.has_value()) {
        return std::nullopt;
    }

    const std::int64_t value = *internalResult;

    // Final int32 range check.
    if (value < INT32_MIN || value > INT32_MAX) {
        std::ostringstream msg;
        msg << "constant expression value " << value << " exceeds int32 range";
        diagnostics.push_back({
            DiagnosticSeverity::Error,
            expr.range,
            msg.str()
        });
        return std::nullopt;
    }

    return static_cast<std::int32_t>(value);
}

} // namespace toyc::sema
