#include "toyc/analysis/cfg.h"
#include "toyc/driver/options.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/verifier.h"
#include "toyc/lowering/ast_to_ir.h"
#include "toyc/mir/instruction_selector.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/target/riscv32/asm_emitter.h"

#include <gtest/gtest.h>

namespace toyc {

static std::string compileToAssembly(const std::string& source) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  SemanticAnalyzer sema(diag);
  auto model = sema.analyze(*ast);
  ASTToIRLowering lowering(*model, diag);
  auto ir = lowering.lower(*ast);
  EXPECT_TRUE(ir.has_value());
  rebuildCFG(*ir);
  EXPECT_TRUE(verifyModule(*ir).ok);
  RV32InstructionSelector selector(diag);
  auto mir = selector.lower(*ir);
  EXPECT_TRUE(mir.has_value());
  EXPECT_TRUE(verifyMIR(*mir).ok);
  return riscv32::emitAssembly(*mir);
}

TEST(CodegenDriverTest, DumpMirOptionParsing) {
  char arg0[] = "toycc";
  char arg1[] = "--dump-mir";
  char* argv[] = {arg0, arg1};
  auto opts = CompilerOptions::parse(2, argv);
  EXPECT_TRUE(opts.dumpMir);
  EXPECT_FALSE(opts.hasCommandLineError);
}

TEST(CodegenDriverTest, DumpModesAreMutuallyExclusive) {
  char arg0[] = "toycc";
  char arg1[] = "--dump-ir";
  char arg2[] = "--dump-mir";
  char* argv[] = {arg0, arg1, arg2};
  auto opts = CompilerOptions::parse(3, argv);
  EXPECT_TRUE(opts.hasCommandLineError);
}

TEST(CodegenDriverTest, CompilesMinimalProgramToAssemblyText) {
  auto asmText = compileToAssembly("int main() { return 0; }");
  EXPECT_NE(asmText.find(".section .text"), std::string::npos);
  EXPECT_NE(asmText.find("main:"), std::string::npos);
  EXPECT_EQ(asmText.find("IR lowering implemented"), std::string::npos);
  EXPECT_EQ(asmText.find("ecall"), std::string::npos);
}

TEST(CodegenDriverTest, CompilesCallAndArithmetic) {
  auto asmText = compileToAssembly(R"(
int add(int a, int b) {
  return a + b;
}
int main() {
  return add(20, 22);
}
)");
  EXPECT_NE(asmText.find("call .Ltoyc.fn."), std::string::npos);
  EXPECT_NE(asmText.find("sw ra"), std::string::npos);
  EXPECT_NE(asmText.find("add t2"), std::string::npos);
}

} // namespace toyc
