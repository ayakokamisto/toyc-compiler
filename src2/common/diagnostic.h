#pragma once

#include <string>

namespace toyc {

struct SourceLocation {
    int line = 1;
    int column = 1;
};

struct SourceRange {
    SourceLocation begin{};
    SourceLocation end{}; // Half-open range [begin, end).
};

enum class DiagnosticSeverity {
    Error,
    Warning,
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    SourceRange range{};
    std::string message;
};

} // namespace toyc
