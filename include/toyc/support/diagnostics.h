#pragma once
/// Diagnostic infrastructure for compiler messages (errors, warnings, notes).

#include "toyc/support/source_location.h"

#include <cstdint>
#include <string>
#include <vector>

namespace toyc {

/// Severity level of a diagnostic message.
enum class DiagLevel : uint8_t {
  Note,
  Warning,
  Error,
  Fatal,
};

/// A single diagnostic message.
struct Diagnostic {
  DiagLevel level;
  SourceLocation location;
  std::string message;

  Diagnostic(DiagLevel lvl, SourceLocation loc, std::string msg)
      : level(lvl), location(loc), message(std::move(msg)) {}
};

/// Collects diagnostics and emits them to stderr.
class DiagnosticEngine {
public:
  DiagnosticEngine() = default;

  /// Report a diagnostic at a specific location.
  void report(DiagLevel level, SourceLocation loc, std::string message);

  /// Convenience helpers.
  void error(SourceLocation loc, std::string message);
  void warning(SourceLocation loc, std::string message);
  void note(SourceLocation loc, std::string message);
  void fatal(SourceLocation loc, std::string message);

  /// Access collected diagnostics.
  [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const { return diags_; }
  [[nodiscard]] bool hasErrors() const { return hasErrors_; }
  [[nodiscard]] size_t errorCount() const { return errorCount_; }

  /// Clear all diagnostics.
  void clear();

private:
  std::vector<Diagnostic> diags_;
  bool hasErrors_ = false;
  size_t errorCount_ = 0;
};

} // namespace toyc
