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
    } else if (std::strcmp(argv[i], "--dump-sema") == 0) {
      opts.dumpSema = true;
    } else if (std::strcmp(argv[i], "--dump-ir") == 0) {
      opts.dumpIr = true;
    } else if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0) {
      opts.verbose = true;
    } else {
      std::cerr << "toycc: unknown option: " << argv[i] << "\n";
      opts.hasCommandLineError = true;
    }
  }

  // Conflicting flags: any two dump modes together is an error.
  int dumpCount = (opts.dumpTokens ? 1 : 0) + (opts.dumpAst ? 1 : 0) +
                  (opts.dumpSema ? 1 : 0) + (opts.dumpIr ? 1 : 0);
  if (dumpCount > 1) {
    std::cerr << "toycc: only one of --dump-tokens, --dump-ast, --dump-sema, --dump-ir can be used\n";
    opts.hasCommandLineError = true;
  }

  return opts;
}

void CompilerOptions::printUsage(std::ostream& out) {
  out << R"(toycc — ToyC Compiler (RISC-V32)

Usage:
  toycc [options] < input.tc > output.s

Options:
  -opt              Enable optimization passes
  --dump-tokens     Tokenize input and dump tokens to stderr
  --dump-ast        Parse input and dump AST to stderr
  --dump-sema       Analyze and dump semantic model to stderr
  --dump-ir         Lower to IR and dump to stderr
  -v, --verbose     Verbose output
  -h, --help        Show this help message

Interface:
  stdin   ToyC source code
  stdout  RISC-V32 assembly
  stderr  Diagnostic and debug messages

Status: P4 — IR lowering implemented. RV32 backend not implemented.
)";
}

} // namespace toyc
