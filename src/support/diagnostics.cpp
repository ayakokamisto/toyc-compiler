/// Diagnostic engine implementation.

#include "toyc/support/diagnostics.h"

#include <iostream>

namespace toyc {

void DiagnosticEngine::report(DiagLevel level, SourceLocation loc, std::string message) {
  diags_.emplace_back(level, loc, std::move(message));
  if (level == DiagLevel::Error || level == DiagLevel::Fatal) {
    hasErrors_ = true;
    ++errorCount_;
  }
}

void DiagnosticEngine::error(SourceLocation loc, std::string message) {
  report(DiagLevel::Error, loc, std::move(message));
}

void DiagnosticEngine::warning(SourceLocation loc, std::string message) {
  report(DiagLevel::Warning, loc, std::move(message));
}

void DiagnosticEngine::note(SourceLocation loc, std::string message) {
  report(DiagLevel::Note, loc, std::move(message));
}

void DiagnosticEngine::fatal(SourceLocation loc, std::string message) {
  report(DiagLevel::Fatal, loc, std::move(message));
}

void DiagnosticEngine::clear() {
  diags_.clear();
  hasErrors_ = false;
  errorCount_ = 0;
}

} // namespace toyc
