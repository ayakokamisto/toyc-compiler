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
#include "toyc/mir/mir.h"
#include "toyc/mir/verifier.h"
#include "toyc/passes/mem2reg.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/sema/semantic_model.h"
#include "toyc/support/diagnostics.h"
#include "toyc/target/riscv32/asm_emitter.h"
#include "toyc/target/riscv32/instruction_selector.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <iostream>
#include <exception>
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

  // --dump-ir / --dump-ssa / --dump-mir: stop after the requested lowered representation.
  if (opts.dumpIr || opts.dumpSsa || opts.dumpMir) {
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

    // Verify Canonical Slot IR.
    auto verifyResult = toyc::verifyModule(*irModule, toyc::VerificationMode::CanonicalSlot);
    if (!verifyResult.ok) {
      for (const auto& err : verifyResult.errors) {
        std::cerr << "IR verification error: " << err << "\n";
      }
      return 1;
    }

    if (opts.dumpIr) {
      toyc::dumpIR(*irModule, std::cerr);
      return 0;
    }

    if (opts.dumpSsa) {
      try {
        toyc::Mem2RegPass mem2reg;
        for (const auto& func : irModule->functions()) {
          (void)mem2reg.run(*func);
        }
      } catch (const std::exception& ex) {
        std::cerr << "SSA construction error: " << ex.what() << "\n";
        return 1;
      }
      toyc::rebuildCFG(*irModule);
      auto ssaVerify = toyc::verifySSAModule(*irModule);
      if (!ssaVerify.ok) {
        for (const auto& err : ssaVerify.errors) {
          std::cerr << "SSA verification error: " << err << "\n";
        }
        return 1;
      }
      toyc::dumpIR(*irModule, std::cerr);
      return 0;
    }

    toyc::RV32InstructionSelector selector(diag);
    auto mirModule = selector.lower(*irModule);
    if (diag.hasErrors() || !mirModule.has_value()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    auto mirVerify = toyc::verifyMIR(*mirModule);
    if (!mirVerify.ok) {
      for (const auto& err : mirVerify.errors) {
        std::cerr << "MIR verification error: " << err << "\n";
      }
      return 1;
    }

    toyc::dumpMIR(*mirModule, std::cerr);
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

    toyc::RV32InstructionSelector selector(diag);
    auto mirModule = selector.lower(*irModule);
    if (diag.hasErrors() || !mirModule.has_value()) {
      for (const auto& d : diag.diagnostics()) {
        std::cerr << d.location.line << ":" << d.location.column
                  << ": error: " << d.message << "\n";
      }
      return 1;
    }

    auto mirVerify = toyc::verifyMIR(*mirModule);
    if (!mirVerify.ok) {
      for (const auto& err : mirVerify.errors) {
        std::cerr << "MIR verification error: " << err << "\n";
      }
      return 1;
    }

    toyc::riscv32::SpillAllAllocator allocator;
    auto allocated = allocator.allocate(std::move(*mirModule));
    std::cout << toyc::riscv32::emitAssembly(allocated);
  }

  return 0;
}
