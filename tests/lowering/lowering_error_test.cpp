/// Lowering error tests — verification of error handling.

#include "toyc/analysis/cfg.h"
#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/module.h"
#include "toyc/ir/verifier.h"
#include "toyc/lowering/ast_to_ir.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace toyc {

static std::optional<Module> compileToIR(const std::string& source, DiagnosticEngine& diag) {
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) return std::nullopt;
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) return std::nullopt;
  SemanticAnalyzer sema(diag);
  auto model = sema.analyze(*ast);
  if (diag.hasErrors()) return std::nullopt;
  ASTToIRLowering lowering(*model, diag);
  auto ir = lowering.lower(*ast);
  if (!ir.has_value()) return std::nullopt;
  rebuildCFG(*ir);
  auto verifyResult = verifyModule(*ir);
  if (!verifyResult.ok) {
    for (const auto& err : verifyResult.errors) {
      diag.error(SourceLocation{}, "IR verification: " + err);
    }
    return std::nullopt;
  }
  return ir;
}

TEST(LoweringErrorTest, LexerError) {
  DiagnosticEngine diag;
  // Invalid token: @ is not valid ToyC.
  auto ir = compileToIR("@invalid", diag);
  EXPECT_FALSE(ir.has_value());
  EXPECT_TRUE(diag.hasErrors());
}

TEST(LoweringErrorTest, ParserError) {
  DiagnosticEngine diag;
  // Missing closing brace.
  auto ir = compileToIR("int main() { return 0;", diag);
  EXPECT_FALSE(ir.has_value());
  EXPECT_TRUE(diag.hasErrors());
}

TEST(LoweringErrorTest, SemaError) {
  DiagnosticEngine diag;
  // Undefined variable.
  auto ir = compileToIR(R"(
    int main() {
      return undefined_var;
    }
  )", diag);
  EXPECT_FALSE(ir.has_value());
  EXPECT_TRUE(diag.hasErrors());
}

TEST(LoweringErrorTest, ValidMinimalProgram) {
  DiagnosticEngine diag;
  auto ir = compileToIR("int main() { return 0; }", diag);
  EXPECT_TRUE(ir.has_value());
  EXPECT_FALSE(diag.hasErrors());
}

TEST(LoweringErrorTest, VoidFunctionCall) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    void touch(int x) { return; }
    int main() {
      touch(3);
      return 0;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // touch is void, so call should not produce a result value.
  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);

  bool hasVoidCall = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::Call) {
        const auto* callee = ir->findFunction(inst->callee);
        if (callee && callee->name() == "touch") {
          hasVoidCall = true;
          // Void call should not have result.
          EXPECT_FALSE(inst->result.has_value());
        }
      }
    }
  }
  EXPECT_TRUE(hasVoidCall);
}

TEST(LoweringErrorTest, IntFunctionCall) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int inc(int a) { return a + 1; }
    int main() {
      return inc(41);
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);

  bool hasCallResult = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::Call) {
        const auto* callee = ir->findFunction(inst->callee);
        if (callee && callee->name() == "inc") {
          EXPECT_TRUE(inst->result.has_value());
          hasCallResult = true;
        }
      }
    }
  }
  EXPECT_TRUE(hasCallResult);
}

} // namespace toyc
