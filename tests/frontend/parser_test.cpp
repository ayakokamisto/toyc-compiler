/// Parser tests — P2 comprehensive syntax verification.

#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>
#include <sstream>

namespace toyc {

// Helper: parse source, assert no errors, return AST.
static std::unique_ptr<CompUnit> expectParse(const std::string& source) {
  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  EXPECT_FALSE(diag.hasErrors()) << "Lexer error for: " << source;
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  EXPECT_FALSE(diag.hasErrors()) << "Parser error for: " << source;
  EXPECT_FALSE(parser.hasError());
  return ast;
}

// Helper: get AST dump.
static std::string astDump(const CompUnit& unit) {
  std::ostringstream out;
  dumpAst(unit, out);
  return out.str();
}

// ═══════════════════════════════════════════════════════════════════════════
// B. Top-level structure
// ═══════════════════════════════════════════════════════════════════════════

TEST(ParserTest, MinimalMain) {
  auto ast = expectParse("int main() { return 0; }");
  ASSERT_EQ(ast->items().size(), 1u);
  EXPECT_EQ(ast->items()[0]->kind(), ASTKind::FuncDef);

  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  EXPECT_EQ(func.name(), "main");
  EXPECT_EQ(func.returnType(), TypeKind::Int);
  EXPECT_TRUE(func.params().empty());
  EXPECT_NE(func.body(), nullptr);
}

TEST(ParserTest, GlobalVarDecl) {
  auto ast = expectParse("int x = 42;");
  ASSERT_EQ(ast->items().size(), 1u);
  EXPECT_EQ(ast->items()[0]->kind(), ASTKind::GlobalDecl);

  auto& gl = static_cast<const GlobalDecl&>(*ast->items()[0]);
  EXPECT_EQ(gl.declaration()->kind(), ASTKind::VarDecl);
  EXPECT_EQ(gl.declaration()->name(), "x");
}

TEST(ParserTest, GlobalConstDecl) {
  auto ast = expectParse("const int N = 10;");
  ASSERT_EQ(ast->items().size(), 1u);
  EXPECT_EQ(ast->items()[0]->kind(), ASTKind::GlobalDecl);

  auto& gl = static_cast<const GlobalDecl&>(*ast->items()[0]);
  EXPECT_EQ(gl.declaration()->kind(), ASTKind::ConstDecl);
  EXPECT_EQ(gl.declaration()->name(), "N");
}

TEST(ParserTest, GlobalDeclAndMultipleFunctions) {
  auto ast = expectParse(
      "int g = 1;\n"
      "int f() { return 0; }\n"
      "int main() { return f(); }\n");
  ASSERT_EQ(ast->items().size(), 3u);
  EXPECT_EQ(ast->items()[0]->kind(), ASTKind::GlobalDecl);
  EXPECT_EQ(ast->items()[1]->kind(), ASTKind::FuncDef);
  EXPECT_EQ(ast->items()[2]->kind(), ASTKind::FuncDef);

  // Verify source order is preserved.
  EXPECT_EQ(static_cast<const FuncDef&>(*ast->items()[1]).name(), "f");
  EXPECT_EQ(static_cast<const FuncDef&>(*ast->items()[2]).name(), "main");
}

TEST(ParserTest, IntAndVoidFunctions) {
  auto ast = expectParse(
      "int f() { return 0; }\n"
      "void g() { }\n"
      "int main() { return 0; }\n");
  ASSERT_EQ(ast->items().size(), 3u);
  EXPECT_EQ(static_cast<const FuncDef&>(*ast->items()[0]).returnType(), TypeKind::Int);
  EXPECT_EQ(static_cast<const FuncDef&>(*ast->items()[1]).returnType(), TypeKind::Void);
}

TEST(ParserTest, EmptyParamFunction) {
  auto ast = expectParse("int f() { return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  EXPECT_TRUE(func.params().empty());
}

TEST(ParserTest, MultiParamFunction) {
  auto ast = expectParse("int f(int a, int b, int c) { return a; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  ASSERT_EQ(func.params().size(), 3u);
  EXPECT_EQ(func.params()[0].name(), "a");
  EXPECT_EQ(func.params()[1].name(), "b");
  EXPECT_EQ(func.params()[2].name(), "c");
}

// ═══════════════════════════════════════════════════════════════════════════
// C. Declarations and statements
// ═══════════════════════════════════════════════════════════════════════════

TEST(ParserTest, ConstDecl) {
  auto ast = expectParse("int main() { const int x = 1; return x; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  ASSERT_EQ(block.statements().size(), 2u);

  auto& declStmt = static_cast<const DeclStmt&>(*block.statements()[0]);
  EXPECT_EQ(declStmt.declaration()->kind(), ASTKind::ConstDecl);
  EXPECT_EQ(declStmt.declaration()->name(), "x");
}

TEST(ParserTest, VarDeclWithExpr) {
  auto ast = expectParse("int main() { int x = 1 + 2; return x; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& declStmt = static_cast<const DeclStmt&>(*block.statements()[0]);
  EXPECT_EQ(declStmt.declaration()->kind(), ASTKind::VarDecl);
  EXPECT_EQ(declStmt.declaration()->initializer()->kind(), ASTKind::BinaryExpr);
}

TEST(ParserTest, EmptyStatement) {
  auto ast = expectParse("int main() { ; return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  ASSERT_GE(block.statements().size(), 2u);
  EXPECT_EQ(block.statements()[0]->kind(), ASTKind::EmptyStmt);
}

TEST(ParserTest, ExprStatement) {
  auto ast = expectParse("int main() { foo(); return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  EXPECT_EQ(block.statements()[0]->kind(), ASTKind::ExprStmt);

  auto& exprStmt = static_cast<const ExprStmt&>(*block.statements()[0]);
  EXPECT_EQ(exprStmt.expression()->kind(), ASTKind::CallExpr);
}

TEST(ParserTest, AssignStatement) {
  auto ast = expectParse("int main() { int x = 0; x = x + 1; return x; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  ASSERT_GE(block.statements().size(), 3u);

  auto& assign = static_cast<const AssignStmt&>(*block.statements()[1]);
  EXPECT_EQ(assign.targetName(), "x");
  EXPECT_EQ(assign.value()->kind(), ASTKind::BinaryExpr);
}

TEST(ParserTest, NestedBlockStmt) {
  auto ast = expectParse("int main() { { int x = 1; } return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  EXPECT_EQ(block.statements()[0]->kind(), ASTKind::BlockStmt);
}

TEST(ParserTest, IfStatement) {
  auto ast = expectParse("int main() { if (1) return 1; return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ifStmt = static_cast<const IfStmt&>(*block.statements()[0]);

  EXPECT_EQ(ifStmt.condition()->kind(), ASTKind::IntegerLiteralExpr);
  EXPECT_NE(ifStmt.thenBranch(), nullptr);
  EXPECT_EQ(ifStmt.elseBranch(), nullptr);
}

TEST(ParserTest, IfElseStatement) {
  auto ast = expectParse("int main() { if (1) return 1; else return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ifStmt = static_cast<const IfStmt&>(*block.statements()[0]);

  EXPECT_NE(ifStmt.thenBranch(), nullptr);
  EXPECT_NE(ifStmt.elseBranch(), nullptr);
}

TEST(ParserTest, DanglingElseBindsToNearestIf) {
  // if (a) if (b) x; else y;
  // else binds to inner if(b).
  auto ast = expectParse(
      "int main() {\n"
      "  int a = 1;\n"
      "  int b = 2;\n"
      "  int x = 0;\n"
      "  if (a) if (b) x = 1; else x = 2;\n"
      "  return x;\n"
      "}\n");

  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());

  // The if statement should be at index 4 (after 4 decl statements).
  const IfStmt* outerIf = nullptr;
  for (const auto& s : block.statements()) {
    if (s->kind() == ASTKind::IfStmt) {
      outerIf = static_cast<const IfStmt*>(s.get());
      break;
    }
  }
  ASSERT_NE(outerIf, nullptr);

  // Outer if has no else.
  EXPECT_EQ(outerIf->elseBranch(), nullptr);

  // Then branch of outer if is an inner if with else.
  EXPECT_EQ(outerIf->thenBranch()->kind(), ASTKind::IfStmt);
  auto& innerIf = static_cast<const IfStmt&>(*outerIf->thenBranch());
  EXPECT_NE(innerIf.thenBranch(), nullptr);
  EXPECT_NE(innerIf.elseBranch(), nullptr);
}

TEST(ParserTest, WhileStatement) {
  auto ast = expectParse("int main() { while (1) break; return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  EXPECT_EQ(block.statements()[0]->kind(), ASTKind::WhileStmt);
}

TEST(ParserTest, BreakStatement) {
  auto ast = expectParse("int main() { while (1) break; return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& whileStmt = static_cast<const WhileStmt&>(*block.statements()[0]);
  EXPECT_EQ(whileStmt.body()->kind(), ASTKind::BreakStmt);
}

TEST(ParserTest, ContinueStatement) {
  auto ast = expectParse("int main() { while (1) continue; return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& whileStmt = static_cast<const WhileStmt&>(*block.statements()[0]);
  EXPECT_EQ(whileStmt.body()->kind(), ASTKind::ContinueStmt);
}

TEST(ParserTest, ReturnEmpty) {
  auto ast = expectParse("void f() { return; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);
  EXPECT_EQ(ret.value(), nullptr);
}

TEST(ParserTest, ReturnExpr) {
  auto ast = expectParse("int f() { return 42; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);
  EXPECT_NE(ret.value(), nullptr);
  EXPECT_EQ(ret.value()->kind(), ASTKind::IntegerLiteralExpr);
}

// ═══════════════════════════════════════════════════════════════════════════
// D. Expressions
// ═══════════════════════════════════════════════════════════════════════════

TEST(ParserTest, IntegerLiteral) {
  auto ast = expectParse("int main() { return 42; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);
  auto& lit = static_cast<const IntegerLiteralExpr&>(*ret.value());
  EXPECT_EQ(lit.rawValue(), "42");
}

TEST(ParserTest, IdentifierExpr) {
  auto ast = expectParse("int main() { int x = 0; return x; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[1]);
  auto& id = static_cast<const IdentifierExpr&>(*ret.value());
  EXPECT_EQ(id.name(), "x");
}

TEST(ParserTest, EmptyCall) {
  auto ast = expectParse("int main() { return foo(); }");
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("CallExpr callee=foo"), std::string::npos);
}

TEST(ParserTest, MultiArgCall) {
  auto ast = expectParse("int main() { return foo(1, 2, 3); }");
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("CallExpr callee=foo"), std::string::npos);
  // Should have 3 arguments.
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);
  auto& call = static_cast<const CallExpr&>(*ret.value());
  EXPECT_EQ(call.arguments().size(), 3u);
}

TEST(ParserTest, UnaryPlusMinusNot) {
  auto ast = expectParse("int main() { return + - ! 1; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  // + (- (! 1))
  auto& uplus = static_cast<const UnaryExpr&>(*ret.value());
  EXPECT_EQ(uplus.op(), UnaryOperator::Plus);

  auto& uminus = static_cast<const UnaryExpr&>(*uplus.operand());
  EXPECT_EQ(uminus.op(), UnaryOperator::Minus);

  auto& unot = static_cast<const UnaryExpr&>(*uminus.operand());
  EXPECT_EQ(unot.op(), UnaryOperator::LogicalNot);

  EXPECT_EQ(unot.operand()->kind(), ASTKind::IntegerLiteralExpr);
}

TEST(ParserTest, MultiplyDivideModuloLeftAssoc) {
  auto ast = expectParse("int main() { return a * b / c % d; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  // ((a * b) / c) % d
  auto& mod = static_cast<const BinaryExpr&>(*ret.value());
  EXPECT_EQ(mod.op(), BinaryOperator::Modulo);

  auto& div = static_cast<const BinaryExpr&>(*mod.lhs());
  EXPECT_EQ(div.op(), BinaryOperator::Divide);

  auto& mul = static_cast<const BinaryExpr&>(*div.lhs());
  EXPECT_EQ(mul.op(), BinaryOperator::Multiply);
}

TEST(ParserTest, AddSubtractLeftAssoc) {
  auto ast = expectParse("int main() { return a + b - c; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  // (a + b) - c
  auto& sub = static_cast<const BinaryExpr&>(*ret.value());
  EXPECT_EQ(sub.op(), BinaryOperator::Subtract);

  auto& add = static_cast<const BinaryExpr&>(*sub.lhs());
  EXPECT_EQ(add.op(), BinaryOperator::Add);
}

TEST(ParserTest, RelationalOperators) {
  auto ast = expectParse("int main() { return a < b <= c > d >= e; }");
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("BinaryExpr op=Less"), std::string::npos);
  EXPECT_NE(dump.find("BinaryExpr op=LessEqual"), std::string::npos);
  EXPECT_NE(dump.find("BinaryExpr op=Greater"), std::string::npos);
  EXPECT_NE(dump.find("BinaryExpr op=GreaterEqual"), std::string::npos);
}

TEST(ParserTest, EqualityOperators) {
  auto ast = expectParse("int main() { return a == b != c; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  // (a == b) != c
  auto& ne = static_cast<const BinaryExpr&>(*ret.value());
  EXPECT_EQ(ne.op(), BinaryOperator::NotEqual);

  auto& eq = static_cast<const BinaryExpr&>(*ne.lhs());
  EXPECT_EQ(eq.op(), BinaryOperator::Equal);
}

TEST(ParserTest, LogicalAnd) {
  auto ast = expectParse("int main() { return a && b && c; }");
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("BinaryExpr op=LogicalAnd"), std::string::npos);
}

TEST(ParserTest, LogicalOr) {
  auto ast = expectParse("int main() { return a || b || c; }");
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("BinaryExpr op=LogicalOr"), std::string::npos);
}

TEST(ParserTest, ParenChangesTree) {
  auto ast = expectParse("int main() { return (a + b) * c; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  // (a + b) * c — multiply at top.
  auto& mul = static_cast<const BinaryExpr&>(*ret.value());
  EXPECT_EQ(mul.op(), BinaryOperator::Multiply);
  EXPECT_EQ(mul.lhs()->kind(), ASTKind::BinaryExpr);

  auto& add = static_cast<const BinaryExpr&>(*mul.lhs());
  EXPECT_EQ(add.op(), BinaryOperator::Add);
}

TEST(ParserTest, RelationEqualPrecedence_a_eq_b_lt_c) {
  // a == b < c  →  Equal(a, Less(b, c))
  auto ast = expectParse("int main() { return a == b < c; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  auto& eq = static_cast<const BinaryExpr&>(*ret.value());
  EXPECT_EQ(eq.op(), BinaryOperator::Equal);
  EXPECT_EQ(eq.lhs()->kind(), ASTKind::IdentifierExpr);
  EXPECT_EQ(eq.rhs()->kind(), ASTKind::BinaryExpr);

  auto& lt = static_cast<const BinaryExpr&>(*eq.rhs());
  EXPECT_EQ(lt.op(), BinaryOperator::Less);
}

TEST(ParserTest, RelationEqualPrecedence_a_lt_b_eq_c) {
  // a < b == c  →  Equal(Less(a, b), c)
  auto ast = expectParse("int main() { return a < b == c; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  auto& eq = static_cast<const BinaryExpr&>(*ret.value());
  EXPECT_EQ(eq.op(), BinaryOperator::Equal);
  EXPECT_EQ(eq.lhs()->kind(), ASTKind::BinaryExpr);
  EXPECT_EQ(eq.rhs()->kind(), ASTKind::IdentifierExpr);

  auto& lt = static_cast<const BinaryExpr&>(*eq.lhs());
  EXPECT_EQ(lt.op(), BinaryOperator::Less);
}

TEST(ParserTest, RelationEqualPrecedence_2_eq_3_lt_4) {
  // 2 == 3 < 4  →  Equal(IntegerLiteral(2), Less(IntegerLiteral(3), IntegerLiteral(4)))
  // This verifies that < binds tighter than == per C semantics.
  auto ast = expectParse("int main() { return 2 == 3 < 4; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  auto& eq = static_cast<const BinaryExpr&>(*ret.value());
  EXPECT_EQ(eq.op(), BinaryOperator::Equal);
  EXPECT_EQ(eq.lhs()->kind(), ASTKind::IntegerLiteralExpr);
  EXPECT_EQ(eq.rhs()->kind(), ASTKind::BinaryExpr);

  auto& lhsLit = static_cast<const IntegerLiteralExpr&>(*eq.lhs());
  EXPECT_EQ(lhsLit.rawValue(), "2");

  auto& lt = static_cast<const BinaryExpr&>(*eq.rhs());
  EXPECT_EQ(lt.op(), BinaryOperator::Less);
  EXPECT_EQ(lt.lhs()->kind(), ASTKind::IntegerLiteralExpr);
  EXPECT_EQ(lt.rhs()->kind(), ASTKind::IntegerLiteralExpr);

  auto& ltLhs = static_cast<const IntegerLiteralExpr&>(*lt.lhs());
  auto& ltRhs = static_cast<const IntegerLiteralExpr&>(*lt.rhs());
  EXPECT_EQ(ltLhs.rawValue(), "3");
  EXPECT_EQ(ltRhs.rawValue(), "4");
}

TEST(ParserTest, ComprehensivePrecedence) {
  // a || b && c == d + e * f
  // → LogicalOr(a, LogicalAnd(b, Equal(c, Add(d, Multiply(e, f)))))
  auto ast = expectParse("int main() { return a || b && c == d + e * f; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);

  auto& orExpr = static_cast<const BinaryExpr&>(*ret.value());
  EXPECT_EQ(orExpr.op(), BinaryOperator::LogicalOr);

  auto& andExpr = static_cast<const BinaryExpr&>(*orExpr.rhs());
  EXPECT_EQ(andExpr.op(), BinaryOperator::LogicalAnd);

  auto& eq = static_cast<const BinaryExpr&>(*andExpr.rhs());
  EXPECT_EQ(eq.op(), BinaryOperator::Equal);

  auto& add = static_cast<const BinaryExpr&>(*eq.rhs());
  EXPECT_EQ(add.op(), BinaryOperator::Add);

  auto& mul = static_cast<const BinaryExpr&>(*add.rhs());
  EXPECT_EQ(mul.op(), BinaryOperator::Multiply);
}

// ═══════════════════════════════════════════════════════════════════════════
// E. SourceRange
// ═══════════════════════════════════════════════════════════════════════════

TEST(ParserTest, IntegerLiteralRange) {
  auto ast = expectParse("int main() { return 42; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);
  auto& lit = static_cast<const IntegerLiteralExpr&>(*ret.value());

  // "42" starts at 1:21, ends at 1:23.
  EXPECT_EQ(lit.range().begin.line, 1u);
  EXPECT_EQ(lit.range().begin.column, 21u);
  EXPECT_EQ(lit.range().end.column, 23u);
}

TEST(ParserTest, BinaryExprRangeCoversAll) {
  auto ast = expectParse("int main() { return 1 + 2; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = static_cast<const BlockStmt&>(*func.body());
  auto& ret = static_cast<const ReturnStmt&>(*block.statements()[0]);
  auto& bin = static_cast<const BinaryExpr&>(*ret.value());

  // "1 + 2" — range should cover from "1" to "2".
  EXPECT_LE(bin.range().begin.offset, bin.lhs()->range().begin.offset);
  EXPECT_GE(bin.range().end.offset, bin.rhs()->range().end.offset);
}

TEST(ParserTest, BlockStmtRangeCoversBraces) {
  auto ast = expectParse("int main() { return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  auto& block = *func.body();

  // Block should start at '{' and end after '}'.
  // In "int main() { return 0; }":
  //   '{' is at column 12, '}' is at column 24.
  //   SourceRange end is exclusive → end.column = 25.
  EXPECT_EQ(block.range().begin.column, 12u); // '{' position
  EXPECT_EQ(block.range().end.column, 25u);   // one past '}'
}

TEST(ParserTest, FuncDefRangeCoversAll) {
  auto ast = expectParse("int main() { return 0; }");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);

  // Should start at 'int' and end at '}'.
  EXPECT_EQ(func.range().begin.column, 1u);
  EXPECT_EQ(func.range().end.column, 25u);
}

TEST(ParserTest, MultiLineRange) {
  auto ast = expectParse("int main() {\n  return 0;\n}\n");
  auto& func = static_cast<const FuncDef&>(*ast->items()[0]);
  EXPECT_EQ(func.range().begin.line, 1u);
  EXPECT_EQ(func.range().end.line, 3u);
}

// ═══════════════════════════════════════════════════════════════════════════
// Fixture file tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(ParserFixtureTest, MinimalMain) {
  auto ast = expectParse("int main() {\n  return 0;\n}\n");
  ASSERT_EQ(ast->items().size(), 1u);
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("FuncDef return=int name=main"), std::string::npos);
  EXPECT_NE(dump.find("IntegerLiteral value=0"), std::string::npos);
}

TEST(ParserFixtureTest, GlobalDecls) {
  auto ast = expectParse(
      "const int N = 42;\n"
      "int x = 1 + 2;\n"
      "int main() {\n"
      "  return x;\n"
      "}\n");
  ASSERT_EQ(ast->items().size(), 3u);
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("GlobalConstDecl name=N"), std::string::npos);
  EXPECT_NE(dump.find("GlobalVarDecl name=x"), std::string::npos);
}

TEST(ParserFixtureTest, FunctionsAndCalls) {
  auto ast = expectParse(
      "int add(int a, int b) {\n"
      "  return a + b;\n"
      "}\n"
      "\n"
      "int main() {\n"
      "  return add(1, 2);\n"
      "}\n");
  ASSERT_EQ(ast->items().size(), 2u);
  auto& addFunc = static_cast<const FuncDef&>(*ast->items()[0]);
  EXPECT_EQ(addFunc.name(), "add");
  EXPECT_EQ(addFunc.params().size(), 2u);

  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("CallExpr callee=add"), std::string::npos);
}

TEST(ParserFixtureTest, ControlFlow) {
  auto ast = expectParse(
      "int main() {\n"
      "  int x = 0;\n"
      "  if (x) {\n"
      "    x = 1;\n"
      "  } else {\n"
      "    x = 2;\n"
      "  }\n"
      "  while (x) {\n"
      "    x = x - 1;\n"
      "    if (x == 5) break;\n"
      "    continue;\n"
      "  }\n"
      "  return x;\n"
      "}\n");
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("IfStmt"), std::string::npos);
  EXPECT_NE(dump.find("WhileStmt"), std::string::npos);
  EXPECT_NE(dump.find("BreakStmt"), std::string::npos);
  EXPECT_NE(dump.find("ContinueStmt"), std::string::npos);
}

TEST(ParserFixtureTest, NestedBlocks) {
  auto ast = expectParse(
      "int main() {\n"
      "  int a = 1;\n"
      "  {\n"
      "    int b = 2;\n"
      "    {\n"
      "      int c = 3;\n"
      "    }\n"
      "  }\n"
      "  return a;\n"
      "}\n");
  auto dump = astDump(*ast);
  EXPECT_NE(dump.find("BlockStmt"), std::string::npos);
  EXPECT_NE(dump.find("LocalVarDecl name=a"), std::string::npos);
  EXPECT_NE(dump.find("LocalVarDecl name=b"), std::string::npos);
  EXPECT_NE(dump.find("LocalVarDecl name=c"), std::string::npos);
}

} // namespace toyc
