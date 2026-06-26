#pragma once
/// The top-level compilation pipeline.
/// Reads ToyC source from stdin and (will) emit RISC-V32 assembly to stdout.

#include "toyc/driver/options.h"
#include "toyc/support/result.h"

#include <string>

namespace toyc {

/// Run the compilation pipeline.
/// @param options  Parsed command-line options.
/// @param source   The ToyC source text (read from stdin).
/// @returns Ok or an error message.
Result<void> runPipeline(const CompilerOptions& options, const std::string& source);

} // namespace toyc
