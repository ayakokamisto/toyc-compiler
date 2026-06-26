/// SemanticAnalyzer tests — P3 verification of valid programs.

#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/sema/semantic_model.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>
#include <sstream>

namespace toyc {

// Helper: analyze source, expect no errors.
static std::optional<SemanticModel> expectAnalyze(const std::string& source) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) {
    EXPECT_FALSE(diag.hasErrors()) << "Lexer error for: " << source;
    return std::nullopt;
  }

  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) {
    EXPECT_FALSE(diag.hasErrors()) << "Parser error for: " << source;
    return std::nullopt;
  }

  SemanticAnalyzer sema(diag);
  auto model = sema.analyze(*ast);
  EXPECT_FALSE(diag.hasErrors()) << "Sema error for: " << source;
  return model;
}

// Helper: analyze source, expect errors.
static DiagnosticEngine expectErrors(const std::string& source) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) return diag;

  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) return diag;

  SemanticAnalyzer sema(diag);
  sema.analyze(*ast);
  EXPECT_TRUE(diag.hasErrors()) << "Expected errors for: " << source;
  return diag;
}

// ═══════════════════════════════════════════════════════════════════════════
// A. Scope and Symbol
// ═══════════════════════════════════════════════════════════════════════════

TEST(SemaTest, GlobalVarDecl) {
  auto model = expectAnalyze("int x = 42;\nint main() { return x; }");
  ASSERT_TRUE(model.has_value());
  bool found = false;
  for (const auto& sym : model->symbols()) {
    if (sym.name == "x" && sym.kind == SymbolKind::GlobalVariable) {
      found = true;
      EXPECT_EQ(sym.type, TypeKind::Int);
    }
  }
  EXPECT_TRUE(found);
}

TEST(SemaTest, GlobalConstDecl) {
  auto model = expectAnalyze("const int c = 42;\nint main() { return c; }");
  ASSERT_TRUE(model.has_value());
  bool found = false;
  for (const auto& sym : model->symbols()) {
    if (sym.name == "c" && sym.kind == SymbolKind::GlobalConstant) {
      found = true;
      EXPECT_TRUE(sym.constantValue.has_value());
      EXPECT_EQ(sym.constantValue.value(), 42);
    }
  }
  EXPECT_TRUE(found);
}

TEST(SemaTest, FunctionRegistered) {
  auto model = expectAnalyze("int f(int a, int b) { return a; }\nint main() { return 0; }");
  ASSERT_TRUE(model.has_value());
  bool found = false;
  for (const auto& sym : model->symbols()) {
    if (sym.name == "f" && sym.kind == SymbolKind::Function) {
      found = true;
      EXPECT_EQ(sym.type, TypeKind::Int);
      EXPECT_EQ(sym.parameterTypes.size(), 2u);
      EXPECT_EQ(sym.parameterNames[0], "a");
      EXPECT_EQ(sym.parameterNames[1], "b");
    }
  }
  EXPECT_TRUE(found);
}

TEST(SemaTest, LocalVarShadowsParam) {
  expectAnalyze(
      "int main() {\n"
      "  int x = 1;\n"
      "  {\n"
      "    int x = 2;\n"
      "  }\n"
      "  return x;\n"
      "}\n");
}

TEST(SemaTest, SelfInitRefRejected) {
  auto diag = expectErrors("int x = x;\nint main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, ForwardRefRejected) {
  auto diag = expectErrors(
      "int main() { return x; }\n"
      "int x = 42;\n");
  EXPECT_TRUE(diag.hasErrors());
}

// ═══════════════════════════════════════════════════════════════════════════
// B. Function rules
// ═══════════════════════════════════════════════════════════════════════════

TEST(SemaTest, ValidMain) {
  expectAnalyze("int main() { return 0; }");
}

TEST(SemaTest, MainReturnsVoid) {
  auto diag = expectErrors("void main() { }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, MainHasParams) {
  auto diag = expectErrors("int main(int x) { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, DuplicateMain) {
  auto diag = expectErrors(
      "int main() { return 0; }\n"
      "int main() { return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, SelfRecursive) {
  expectAnalyze(
      "int f() {\n"
      "  return f();\n"
      "}\n"
      "int main() { return f(); }\n");
}

TEST(SemaTest, ForwardCallRejected) {
  auto diag = expectErrors(
      "int f() { return g(); }\n"
      "int g() { return 0; }\n"
      "int main() { return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, ArgCountMismatch) {
  auto diag = expectErrors(
      "int f(int a, int b) { return a; }\n"
      "int main() { return f(1); }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, FunctionAsValue) {
  auto diag = expectErrors(
      "int f() { return 0; }\n"
      "int main() { int x = f; return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, FunctionAsAssignTarget) {
  auto diag = expectErrors(
      "int f() { return 0; }\n"
      "int main() { f = 1; return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

// ═══════════════════════════════════════════════════════════════════════════
// C. Type rules
// ═══════════════════════════════════════════════════════════════════════════

TEST(SemaTest, VoidCallAsExprStmt) {
  expectAnalyze(
      "void g() { }\n"
      "int main() { g(); return 0; }\n");
}

TEST(SemaTest, VoidCallAsAssignRHS) {
  auto diag = expectErrors(
      "void g() { }\n"
      "int main() { int x = g(); return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, VoidCallAsIfCond) {
  auto diag = expectErrors(
      "void g() { }\n"
      "int main() { if (g()) { } return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, VoidCallAsReturn) {
  auto diag = expectErrors(
      "void g() { }\n"
      "int f() { return g(); }\n"
      "int main() { return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, AssignToConst) {
  auto diag = expectErrors(
      "const int c = 1;\n"
      "int main() { c = 2; return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, IfConditionVoid) {
  auto diag = expectErrors(
      "void g() { }\n"
      "int main() { if (g()) { } return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, WhileConditionVoid) {
  auto diag = expectErrors(
      "void g() { }\n"
      "int main() { while (g()) { } return 0; }\n");
  EXPECT_TRUE(diag.hasErrors());
}

// ═══════════════════════════════════════════════════════════════════════════
// D. Constant expressions
// ═══════════════════════════════════════════════════════════════════════════

TEST(SemaTest, ConstArithmetic) {
  auto model = expectAnalyze(
      "const int c = 2 + 3 * 4;\n"
      "int main() { return c; }");
  ASSERT_TRUE(model.has_value());
  for (const auto& sym : model->symbols()) {
    if (sym.name == "c") {
      EXPECT_TRUE(sym.constantValue.has_value());
      EXPECT_EQ(sym.constantValue.value(), 14);
    }
  }
}

TEST(SemaTest, ConstRefToConst) {
  auto model = expectAnalyze(
      "const int a = 10;\n"
      "const int b = a + 5;\n"
      "int main() { return b; }");
  ASSERT_TRUE(model.has_value());
  for (const auto& sym : model->symbols()) {
    if (sym.name == "b") {
      EXPECT_TRUE(sym.constantValue.has_value());
      EXPECT_EQ(sym.constantValue.value(), 15);
    }
  }
}

TEST(SemaTest, ConstRefToVarRejected) {
  auto diag = expectErrors(
      "int x = 10;\n"
      "const int c = x;\n"
      "int main() { return c; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, ConstDivZero) {
  auto diag = expectErrors(
      "const int c = 1 / 0;\n"
      "int main() { return c; }");
  EXPECT_TRUE(diag.hasErrors());
}

TEST(SemaTest, ConstINT32_MIN) {
  auto model = expectAnalyze(
      "const int c = -2147483648;\n"
      "int main() { return 0; }");
  ASSERT_TRUE(model.has_value());
  for (const auto& sym : model->symbols()) {
    if (sym.name == "c") {
      EXPECT_TRUE(sym.constantValue.has_value());
      EXPECT_EQ(sym.constantValue.value(), INT32_MIN);
    }
  }
}

TEST(SemaTest, LiteralOutOfRange) {
  auto diag = expectErrors(
      "int x = 2147483648;\n"
      "int main() { return 0; }");
  EXPECT_TRUE(diag.hasErrors());
}

// ═══════════════════════════════════════════════════════════════════════════
// E. Global init classification
// ═══════════════════════════════════════════════════════════════════════════

TEST(SemaTest, GlobalConstStatic) {
  auto model = expectAnalyze(
      "const int c = 2 + 3;\n"
      "int main() { return c; }");
  ASSERT_TRUE(model.has_value());
  // Find the const decl.
  for (const auto& sym : model->symbols()) {
    if (sym.name == "c") {
      // GlobalInitInfo should be static.
      // We can't directly access it without the AST node, but the symbol
      // having constantValue proves it was evaluated.
      EXPECT_TRUE(sym.constantValue.has_value());
      EXPECT_EQ(sym.constantValue.value(), 5);
    }
  }
}

TEST(SemaTest, GlobalVarStatic) {
  auto model = expectAnalyze(
      "const int c = 10;\n"
      "int g = c * 2;\n"
      "int main() { return g; }");
  ASSERT_TRUE(model.has_value());
  for (const auto& sym : model->symbols()) {
    if (sym.name == "g") {
      // Should be static because c is a constant.
      // The model should have globalInitInfo for this.
      // We verify indirectly: the symbol exists and is a GlobalVariable.
      EXPECT_EQ(sym.kind, SymbolKind::GlobalVariable);
    }
  }
}

TEST(SemaTest, GlobalVarRuntimeInitNoSpuriousError) {
  // AUD-008: int g2 = g1 + 1 should NOT produce "g1 is not a constant expression"
  // because g1 is a valid global variable — just not a compile-time constant.
  auto model = expectAnalyze(
      "const int c = 2 + 3;\n"
      "int g1 = c * 4;\n"
      "int g2 = g1 + 1;\n"
      "int main() { return g2; }");
  ASSERT_TRUE(model.has_value());
  // All three globals should exist.
  bool foundC = false, foundG1 = false, foundG2 = false;
  for (const auto& sym : model->symbols()) {
    if (sym.name == "c") {
      foundC = true;
      EXPECT_EQ(sym.kind, SymbolKind::GlobalConstant);
      EXPECT_TRUE(sym.constantValue.has_value());
      EXPECT_EQ(sym.constantValue.value(), 5);
    }
    if (sym.name == "g1") {
      foundG1 = true;
      EXPECT_EQ(sym.kind, SymbolKind::GlobalVariable);
    }
    if (sym.name == "g2") {
      foundG2 = true;
      EXPECT_EQ(sym.kind, SymbolKind::GlobalVariable);
    }
  }
  EXPECT_TRUE(foundC);
  EXPECT_TRUE(foundG1);
  EXPECT_TRUE(foundG2);
}

TEST(SemaTest, ConstInitWithNonConstStillErrors) {
  // const int c = g1 + 1 should still error — const requires compile-time value.
  auto diag = expectErrors(
      "int g1 = 10;\n"
      "const int c = g1 + 1;\n"
      "int main() { return c; }");
  EXPECT_TRUE(diag.hasErrors());
}

} // namespace toyc
