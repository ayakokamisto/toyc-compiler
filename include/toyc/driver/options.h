#pragma once
/// Command-line option parsing for the toycc compiler.

#include <iosfwd>
#include <string>
#include <vector>

namespace toyc {

/// Parsed command-line options.
struct CompilerOptions {
  bool help = false;               ///< --help was passed.
  bool optimize = false;           ///< -opt was passed.
  bool verbose = false;            ///< -v / --verbose was passed.
  bool dumpTokens = false;         ///< --dump-tokens was passed.
  bool dumpAst = false;            ///< --dump-ast was passed.
  bool dumpSema = false;           ///< --dump-sema was passed.
  bool hasCommandLineError = false; ///< Unknown args or conflicting flags.

  /// Parse arguments. Returns the parsed options.
  static CompilerOptions parse(int argc, char* argv[]);

  /// Print usage to the given output stream.
  static void printUsage(std::ostream& out);
};

} // namespace toyc
