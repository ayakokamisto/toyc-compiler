/// Command-line option parsing for toycc.

#include "toyc/driver/options.h"

#include <cstring>
#include <iostream>

namespace toyc {

CompilerOptions CompilerOptions::parse(int argc, char* argv[]) {
  CompilerOptions opts;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      opts.help = true;
    } else if (std::strcmp(argv[i], "-opt") == 0) {
      opts.optimize = true;
    } else if (std::strcmp(argv[i], "--dump-tokens") == 0) {
      opts.dumpTokens = true;
    } else if (std::strcmp(argv[i], "--dump-ast") == 0) {
      opts.dumpAst = true;
    } else if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0) {
      opts.verbose = true;
    } else {
      std::cerr << "toycc: unknown option: " << argv[i] << "\n";
      opts.hasCommandLineError = true;
    }
  }

  // Conflicting flags.
  if (opts.dumpTokens && opts.dumpAst) {
    std::cerr << "toycc: --dump-tokens and --dump-ast cannot be used together\n";
    opts.hasCommandLineError = true;
  }

  return opts;
}

void CompilerOptions::printUsage() {
  std::cout << R"(toycc — ToyC Compiler (RISC-V32)

Usage:
  toycc [options] < input.tc > output.s

Options:
  -opt              Enable optimization passes
  --dump-tokens     Tokenize input and dump tokens to stderr
  --dump-ast        Parse input and dump AST to stderr
  -v, --verbose     Verbose output
  -h, --help        Show this help message

Interface:
  stdin   ToyC source code
  stdout  RISC-V32 assembly
  stderr  Diagnostic and debug messages

Status: P2 — lexer and parser implemented. Compilation pipeline not yet implemented.
)";
}

} // namespace toyc
