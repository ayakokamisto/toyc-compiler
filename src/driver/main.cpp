/// toycc — ToyC compiler entry point.
/// Reads ToyC source from stdin, emits RISC-V32 assembly to stdout.

#include "toyc/analysis/cfg.h"
#include "toyc/driver/compiler_pipeline.h"
#include "toyc/driver/options.h"
#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/module.h"
#include "toyc/ir/printer.h"
#include "toyc/ir/verifier.h"
#include "toyc/lowering/ast_to_ir.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/sema/semantic_model.h"
#include "toyc/support/diagnostics.h"

#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char* argv[]) {
  auto opts = toyc::CompilerOptions::parse(argc, argv);

  if (opts.help) {
    toyc::CompilerOptions::printUsage(std::cout);
    return 0;
  }

  if (opts.hasCommandLineError) {
    toyc::CompilerOptions::printUsage(std::cerr);
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

  // --dump-sema: lex, parse, analyze, dump SemanticModel to stderr, then exit.
  if (opts.dumpSema) {
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

    toyc::SemanticAnalyzer sema(diag);
    auto model = sema.analyze(*ast);

    if (diag.hasErrors()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    toyc::dumpSema(*model, *ast, std::cerr);
    return 0;
  }

  // --dump-ir: lex, parse, analyze, lower, dump IR to stderr, then exit.
  if (opts.dumpIr) {
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

    toyc::SemanticAnalyzer sema(diag);
    auto model = sema.analyze(*ast);

    if (diag.hasErrors()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    toyc::ASTToIRLowering lowering(*model, diag);
    auto irModule = lowering.lower(*ast);

    if (diag.hasErrors() || !irModule.has_value()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    // Rebuild CFG.
    toyc::rebuildCFG(*irModule);

    // Verify.
    auto verifyResult = toyc::verifyModule(*irModule);
    if (!verifyResult.ok) {
      for (const auto& err : verifyResult.errors) {
        std::cerr << "IR verification error: " << err << "\n";
      }
      return 1;
    }

    // Dump IR to stderr.
    toyc::dumpIR(*irModule, std::cerr);
    return 0;
  }

  // Normal compilation pipeline.
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

    toyc::SemanticAnalyzer sema(diag);
    auto model = sema.analyze(*ast);

    if (diag.hasErrors()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    // Lower to IR.
    toyc::ASTToIRLowering lowering(*model, diag);
    auto irModule = lowering.lower(*ast);

    if (diag.hasErrors() || !irModule.has_value()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    // Rebuild CFG and verify.
    toyc::rebuildCFG(*irModule);
    auto verifyResult = toyc::verifyModule(*irModule);
    if (!verifyResult.ok) {
      for (const auto& err : verifyResult.errors) {
        std::cerr << "IR verification error: " << err << "\n";
      }
      return 1;
    }
  }

  // Normal mode: IR lowering done, but RV32 backend not implemented.
  std::cerr << "IR lowering implemented; RV32 backend not implemented.\n";
  return 1;
}
