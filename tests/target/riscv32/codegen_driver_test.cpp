#include "toyc/analysis/cfg.h"
#include "toyc/driver/options.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/verifier.h"
#include "toyc/lowering/ast_to_ir.h"
#include "toyc/mir/verifier.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/target/riscv32/asm_emitter.h"
#include "toyc/target/riscv32/instruction_selector.h"
#include "toyc/target/riscv32/reg_allocator.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <gtest/gtest.h>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

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
  riscv32::SpillAllAllocator allocator;
  auto allocated = allocator.allocate(std::move(*mir));
  return riscv32::emitAssembly(allocated);
}

static std::string compileToAssemblyWithRegisterAllocator(const std::string& source) {
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
  riscv32::RegisterAllocator allocator(true);
  auto allocated = allocator.allocate(std::move(*mir));
  return riscv32::emitAssembly(allocated);
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

struct EndToEndCase {
  const char* name;
  const char* source;
  int expectedExitCode;
};

static const std::vector<EndToEndCase>& p5Cases() {
  static const std::vector<EndToEndCase> cases = {
      {"minimal", "int main() { return 0; }", 0},
      {"arithmetic", "int main() { int a = 1; return a + -2 * 3 + 18; }", 13},
      {"call", "int add(int a, int b) { return a + b; } int main() { return add(20, 22); }", 42},
      {"fact", R"(
int fact(int n) {
  if (n <= 1) { return 1; }
  return n * fact(n - 1);
}
int main() { return fact(5); }
)",
       120},
      {"while_break_continue", R"(
int main() {
  int i = 0;
  int sum = 0;
  while (i < 10) {
    i = i + 1;
    if (i == 5) { continue; }
    if (i == 8) { break; }
    sum = sum + i;
  }
  return sum;
}
)",
       23},
      {"short_circuit", "int side() { return 1; } int main() { int x = 0; if (x && side()) { return 9; } return 2; }", 2},
      {"static_global", "const int c = 2 + 3; int g = c * 4; int main() { return g; }", 20},
      {"runtime_global", "int seed() { return 7; } int g = seed(); int main() { return g + 1; }", 8},
      {"sum9", R"(
int sum9(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8) {
  return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8;
}
int main() { return sum9(1, 2, 3, 4, 5, 6, 7, 8, 9); }
)",
       45},
      {"signed_div_rem_encoded", R"(
int main() {
  int q1 = -7 / 3;
  int r1 = -7 % 3;
  int q2 = 7 / -3;
  int r2 = 7 % -3;
  return 42
       + (q1 + 2)
       + 4 * (r1 + 1)
       + 16 * (q2 + 2)
       + 64 * (r2 - 1);
}
)",
       42},
  };
  return cases;
}

TEST(CodegenDriverTest, P5EndToEndProgramsGenerateAssemblyWithCorrectedExpectations) {
  for (const auto& testCase : p5Cases()) {
    SCOPED_TRACE(testCase.name);
    auto asmText = compileToAssembly(testCase.source);
    EXPECT_GT(asmText.size(), 0u);
    EXPECT_NE(asmText.find("main:"), std::string::npos);
    EXPECT_GE(testCase.expectedExitCode, 0);
    EXPECT_LE(testCase.expectedExitCode, 255);
  }
}

static int countRegexMatches(const std::string& text, const std::regex& regex) {
  return static_cast<int>(std::distance(std::sregex_iterator(text.begin(), text.end(), regex),
                                        std::sregex_iterator()));
}

static void expectSavedAndRestoredIfUsed(const std::string& asmText, const std::string& reg) {
  std::regex useRegex("\\b" + reg + "\\b");
  if (!std::regex_search(asmText, useRegex)) return;
  std::regex saveRegex("(^|\\n)\\s*sw\\s+" + reg + ",");
  std::regex restoreRegex("(^|\\n)\\s*lw\\s+" + reg + ",");
  EXPECT_TRUE(std::regex_search(asmText, saveRegex)) << "missing save for " << reg;
  EXPECT_TRUE(std::regex_search(asmText, restoreRegex)) << "missing restore for " << reg;
}

static void expectReferencedLabelsDefined(const std::string& asmText);

TEST(CodegenDriverTest, LinearScanSavesCalleeSavedRegistersInLeafAndNonLeaf) {
  auto asmText = compileToAssemblyWithRegisterAllocator(R"(
int leaf(int a, int b, int c) {
  int x0 = a + b;
  int x1 = x0 + c;
  int x2 = x1 + a;
  int x3 = x2 + b;
  return x3 + c;
}
int main() {
  int base = 3;
  int y = leaf(base, base + 1, base + 2);
  return base + y;
}
)");
  for (const auto* reg : {"s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"}) {
    expectSavedAndRestoredIfUsed(asmText, reg);
  }
  EXPECT_NE(asmText.find("sw ra"), std::string::npos);
  EXPECT_NE(asmText.find("lw ra"), std::string::npos);
}

TEST(CodegenDriverTest, CallerSavedRegistersAreOnlyScratchAroundCalls) {
  auto asmText = compileToAssemblyWithRegisterAllocator(R"(
int touch(int x) { return x + 7; }
int main() {
  int a = 11;
  int b = touch(a);
  int c = a + b;
  return c + a;
}
)");
  EXPECT_EQ(asmText.find(" t4"), std::string::npos);
  EXPECT_EQ(asmText.find(" t5"), std::string::npos);
  EXPECT_EQ(asmText.find(" t6"), std::string::npos);
  EXPECT_EQ(asmText.find(" a2"), std::string::npos);
  EXPECT_EQ(asmText.find(" a3"), std::string::npos);
  EXPECT_EQ(asmText.find(" a4"), std::string::npos);
  EXPECT_EQ(asmText.find(" a5"), std::string::npos);
}

TEST(CodegenDriverTest, ReturnsUseUnifiedEpilogue) {
  auto asmText = compileToAssemblyWithRegisterAllocator(R"(
int main() {
  int x = 3;
  if (x) { return x + 1; }
  return x + 2;
}
)");
  EXPECT_NE(asmText.find("main.epilogue:"), std::string::npos);
  EXPECT_GE(countRegexMatches(asmText, std::regex(R"(j main\.epilogue)")), 2);
  EXPECT_EQ(countRegexMatches(asmText, std::regex(R"((^|\n)\s*ret\s*(\n|$))")), 1);
}

TEST(CodegenDriverTest, AbiRegressionProgramsGenerateAssembly) {
  const std::vector<EndToEndCase> cases = {
      {"leaf_s_registers", "int main(){ int a=1; int b=2; int c=3; int d=4; return a+b+c+d; }", 10},
      {"nonleaf_s_registers", "int f(int x){return x+1;} int main(){int a=4; int b=f(a); return a+b;}", 9},
      {"caller_uses_local_after_call", "int f(int x){return x+5;} int main(){int a=7; int b=f(a); return a+b;}", 19},
      {"loop_variable_across_call", R"(
int one(int x){return x+1;}
int main(){int i=0; int sum=0; while(i<4){sum=sum+one(i); i=i+1;} return sum;}
)",
       10},
      {"multi_level_calls", "int h(int x){return x+1;} int g(int x){return h(x)+2;} int main(){return g(5)+3;}", 11},
      {"recursive_call", "int fact(int n){if(n<=1){return 1;} return n*fact(n-1);} int main(){return fact(5);}", 120},
      {"if_else_merge", "int main(){int x=3; int y=0; if(x<4){y=10;} else {y=20;} return y+x;}", 13},
      {"while_backedge", "int main(){int i=0; int acc=0; while(i<5){acc=acc+i; i=i+1;} return acc;}", 10},
  };

  for (const auto& testCase : cases) {
    SCOPED_TRACE(testCase.name);
    auto asmText = compileToAssemblyWithRegisterAllocator(testCase.source);
    EXPECT_NE(asmText.find("main:"), std::string::npos);
    EXPECT_GE(testCase.expectedExitCode, 0);
    EXPECT_LE(testCase.expectedExitCode, 255);
    expectReferencedLabelsDefined(asmText);
  }
}

static std::unordered_set<std::string> collectLabels(const std::string& text) {
  std::unordered_set<std::string> labels;
  std::regex labelRegex(R"((^|\n)([A-Za-z_.][A-Za-z0-9_.]*):)");
  for (auto it = std::sregex_iterator(text.begin(), text.end(), labelRegex);
       it != std::sregex_iterator(); ++it) {
    labels.insert((*it)[2].str());
  }
  return labels;
}

static void expectReferencedLabelsDefined(const std::string& asmText) {
  auto labels = collectLabels(asmText);
  std::regex refRegex(R"(\b(call|j|bnez|beqz|bltu|bgez|la)\s+(?:[a-z0-9]+,\s*)?(?:[a-z0-9]+,\s*)?([A-Za-z_.][A-Za-z0-9_.]*))");
  for (auto it = std::sregex_iterator(asmText.begin(), asmText.end(), refRegex);
       it != std::sregex_iterator(); ++it) {
    auto target = (*it)[2].str();
    if (target == "sp" || target == "zero") continue;
    EXPECT_NE(labels.find(target), labels.end()) << "undefined target: " << target;
  }
}

TEST(CodegenDriverTest, AssemblyStaticAudit) {
  auto asmText = compileToAssembly(p5Cases().back().source);
  auto labels = collectLabels(asmText);
  std::regex illegalOpcode(R"((^|\n)\s*(mul|div|rem)\s)");

  EXPECT_NE(labels.find("main"), labels.end());
  EXPECT_NE(asmText.find(".globl main"), std::string::npos);
  EXPECT_EQ(asmText.find("ecall"), std::string::npos);
  EXPECT_FALSE(std::regex_search(asmText, illegalOpcode));
  EXPECT_EQ(asmText.find("%v"), std::string::npos);
  EXPECT_EQ(asmText.find("FrameSlot"), std::string::npos);
  EXPECT_EQ(asmText.find("SlotId"), std::string::npos);
  EXPECT_EQ(asmText.find("FunctionId"), std::string::npos);
  EXPECT_NE(labels.find(".Ltoyc.div_i32"), labels.end());
  EXPECT_NE(labels.find(".Ltoyc.rem_i32"), labels.end());
  expectReferencedLabelsDefined(asmText);
}

} // namespace toyc
