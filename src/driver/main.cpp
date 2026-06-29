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
#include "toyc/passes/dce.h"
#include "toyc/passes/inst_combine.h"
#include "toyc/passes/mem2reg.h"
#include "toyc/passes/out_of_ssa.h"
#include "toyc/passes/pass_manager.h"
#include "toyc/passes/sccp.h"
#include "toyc/passes/simplify_cfg.h"
#include "toyc/passes/ir_utils.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/sema/semantic_model.h"
#include "toyc/support/diagnostics.h"
#include "toyc/target/riscv32/asm_emitter.h"
#include "toyc/target/riscv32/instruction_selector.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <iostream>
#include <exception>
#include <memory>
#include <sstream>
#include <string>

namespace {

bool verifyCanonicalOrReport(toyc::Module& module, std::ostream& err) {
  auto verify = toyc::verifyModule(module, toyc::VerificationMode::CanonicalSlot);
  if (verify.ok) return true;
  for (const auto& message : verify.errors) {
    err << "IR verification error: " << message << "\n";
  }
  return false;
}

bool buildSSA(toyc::Module& module, std::ostream& err) {
  for (const auto& func : module.functions()) {
    (void)toyc::removeUnreachableBlocks(*func);
  }
  toyc::rebuildCFG(module);
  if (!verifyCanonicalOrReport(module, err)) return false;

  try {
    toyc::Mem2RegPass mem2reg;
    for (const auto& func : module.functions()) {
      (void)mem2reg.run(*func);
    }
  } catch (const std::exception& ex) {
    err << "internal optimizer diagnostic: SSA construction error: " << ex.what() << "\n";
    return false;
  }
  toyc::rebuildCFG(module);
  auto ssaVerify = toyc::verifySSAModule(module);
  if (ssaVerify.ok) return true;
  for (const auto& message : ssaVerify.errors) {
    err << "SSA verification error: " << message << "\n";
  }
  return false;
}

bool optimizeSSA(toyc::Module& module, std::ostream& err) {
  toyc::FunctionPassManager manager;
  manager.add(std::make_unique<toyc::InstCombineLitePass>());
  manager.add(std::make_unique<toyc::SCCPPass>());
  manager.add(std::make_unique<toyc::SimplifyCFGPass>());
  manager.add(std::make_unique<toyc::DCEPass>());

  for (const auto& func : module.functions()) {
    if (!manager.runToFixedPoint(*func, module, 3, err)) return false;
  }
  toyc::rebuildCFG(module);
  auto verify = toyc::verifySSAModule(module);
  if (verify.ok) return true;
  err << "internal optimizer diagnostic: fixed-point SSA verification failed\n";
  for (const auto& message : verify.errors) err << "  " << message << "\n";
  return false;
}

bool lowerOutOfSSA(toyc::Module& module, std::ostream& err) {
  toyc::OutOfSSAPass outOfSSA;
  for (const auto& func : module.functions()) {
    (void)outOfSSA.run(*func);
  }
  toyc::rebuildCFG(module);
  auto verify = toyc::verifyModule(module, toyc::VerificationMode::CanonicalSlot);
  if (verify.ok) return true;
  err << "internal optimizer diagnostic: Out-of-SSA produced invalid Canonical Slot IR\n";
  for (const auto& message : verify.errors) err << "  " << message << "\n";
  return false;
}

} // namespace

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
    if (!verifyCanonicalOrReport(*irModule, std::cerr)) return 1;

    if (opts.dumpIr) {
      toyc::dumpIR(*irModule, std::cerr);
      return 0;
    }

    if (opts.dumpSsa) {
      if (!buildSSA(*irModule, std::cerr)) return 1;
      if (opts.optimize && !optimizeSSA(*irModule, std::cerr)) return 1;
      toyc::dumpIR(*irModule, std::cerr);
      return 0;
    }

    if (opts.optimize) {
      if (!buildSSA(*irModule, std::cerr)) return 1;
      if (!optimizeSSA(*irModule, std::cerr)) return 1;
      if (!lowerOutOfSSA(*irModule, std::cerr)) return 1;
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

    if (opts.optimize) {
      if (!buildSSA(*irModule, std::cerr)) return 1;
      if (!optimizeSSA(*irModule, std::cerr)) return 1;
      if (!lowerOutOfSSA(*irModule, std::cerr)) return 1;
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
