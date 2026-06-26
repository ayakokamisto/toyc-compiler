/// Compiler pipeline — top-level orchestration.
/// P0 scaffold: reports that compilation is not yet implemented.

#include "toyc/driver/compiler_pipeline.h"
#include "toyc/support/diagnostics.h"

#include <iostream>

namespace toyc {

Result<void> runPipeline(const CompilerOptions& /*options*/, const std::string& /*source*/) {
  // P0: pipeline not yet implemented.
  // Do NOT emit fake assembly to stdout.
  DiagnosticEngine diag;
  diag.error(SourceLocation{},
             "compilation pipeline not yet implemented (P0 scaffold). "
             "No assembly output produced.");
  for (const auto& d : diag.diagnostics()) {
    std::cerr << "error: " << d.message << "\n";
  }
  return std::string{"pipeline not implemented"};
}

} // namespace toyc
