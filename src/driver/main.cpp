/// toycc — ToyC compiler entry point.
/// Reads ToyC source from stdin, emits RISC-V32 assembly to stdout.

#include "toyc/driver/compiler_pipeline.h"
#include "toyc/driver/options.h"
#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/support/diagnostics.h"

#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char* argv[]) {
  auto opts = toyc::CompilerOptions::parse(argc, argv);

  if (opts.help) {
    toyc::CompilerOptions::printUsage();
    return 0;
  }

  if (opts.hasCommandLineError) {
    toyc::CompilerOptions::printUsage();
    return 1;
  }

  // Read all of stdin into a string.
  std::ostringstream buf;
  buf << std::cin.rdbuf();
  std::string source = buf.str();

  // --dump-tokens: tokenize and dump to stderr, then exit.
  if (opts.dumpTokens) {
    toyc::DiagnosticEngine diag;
    toyc::Lexer lexer(source, diag);
    auto tokens = lexer.tokenize();

    toyc::dumpTokens(tokens, std::cerr);

    for (const auto& d : diag.diagnostics()) {
      std::cerr << d.location.line << ":" << d.location.column
                << ": error: " << d.message << "\n";
    }

    return diag.hasErrors() ? 1 : 0;
  }

  // --dump-ast: lex, parse, dump AST to stderr, then exit.
  if (opts.dumpAst) {
    toyc::DiagnosticEngine diag;
    toyc::Lexer lexer(source, diag);
    auto tokens = lexer.tokenize();

    if (diag.hasErrors()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    toyc::Parser parser(tokens, diag);
    auto ast = parser.parse();

    if (diag.hasErrors()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    toyc::dumpAst(*ast, std::cerr);
    return 0;
  }

  // Normal compilation pipeline (P0 placeholder).
  // Only run pipeline if lexer and parser succeed.
  {
    toyc::DiagnosticEngine diag;
    toyc::Lexer lexer(source, diag);
    auto tokens = lexer.tokenize();

    if (diag.hasErrors()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    toyc::Parser parser(tokens, diag);
    auto ast = parser.parse();

    if (diag.hasErrors()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }
  }

  auto result = toyc::runPipeline(opts, source);
  if (!result.ok()) {
    std::cerr << "toycc: " << result.error() << std::endl;
    return 1;
  }

  return 0;
}
