/// AST model tests — P2 verification.

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
  return ast;
}

// ── 1. AST nodes can be created ─────────────────────────────────────────

TEST(ASTTest, CreateIntegerLiteral) {
  auto node = std::make_unique<IntegerLiteralExpr>("42");
  EXPECT_EQ(node->kind(), ASTKind::IntegerLiteralExpr);
  EXPECT_EQ(node->rawValue(), "42");
}

TEST(ASTTest, CreateIdentifier) {
  auto node = std::make_unique<IdentifierExpr>("x");
  EXPECT_EQ(node->kind(), ASTKind::IdentifierExpr);
  EXPECT_EQ(node->name(), "x");
}

TEST(ASTTest, CreateUnaryExpr) {
  auto operand = std::make_unique<IdentifierExpr>("x");
  auto node = std::make_unique<UnaryExpr>(UnaryOperator::Minus, std::move(operand));
  EXPECT_EQ(node->kind(), ASTKind::UnaryExpr);
  EXPECT_EQ(node->op(), UnaryOperator::Minus);
  EXPECT_NE(node->operand(), nullptr);
}

TEST(ASTTest, CreateBinaryExpr) {
  auto lhs = std::make_unique<IntegerLiteralExpr>("1");
  auto rhs = std::make_unique<IntegerLiteralExpr>("2");
  auto node = std::make_unique<BinaryExpr>(BinaryOperator::Add,
                                            std::move(lhs), std::move(rhs));
  EXPECT_EQ(node->kind(), ASTKind::BinaryExpr);
  EXPECT_EQ(node->op(), BinaryOperator::Add);
  EXPECT_NE(node->lhs(), nullptr);
  EXPECT_NE(node->rhs(), nullptr);
}

TEST(ASTTest, CreateCallExpr) {
  std::vector<std::unique_ptr<Expr>> args;
  args.push_back(std::make_unique<IntegerLiteralExpr>("1"));
  auto node = std::make_unique<CallExpr>("foo", std::move(args));
  EXPECT_EQ(node->kind(), ASTKind::CallExpr);
  EXPECT_EQ(node->calleeName(), "foo");
  EXPECT_EQ(node->arguments().size(), 1u);
}

TEST(ASTTest, CreateConstDecl) {
  auto init = std::make_unique<IntegerLiteralExpr>("42");
  auto node = std::make_unique<ConstDecl>("x", std::move(init));
  EXPECT_EQ(node->kind(), ASTKind::ConstDecl);
  EXPECT_EQ(node->name(), "x");
  EXPECT_NE(node->initializer(), nullptr);
}

TEST(ASTTest, CreateVarDecl) {
  auto init = std::make_unique<IntegerLiteralExpr>("0");
  auto node = std::make_unique<VarDecl>("x", std::move(init));
  EXPECT_EQ(node->kind(), ASTKind::VarDecl);
  EXPECT_EQ(node->name(), "x");
}

TEST(ASTTest, CreateBlockStmt) {
  auto node = std::make_unique<BlockStmt>();
  EXPECT_EQ(node->kind(), ASTKind::BlockStmt);
  EXPECT_TRUE(node->statements().empty());
}

TEST(ASTTest, CreateIfStmt) {
  auto cond = std::make_unique<IdentifierExpr>("x");
  auto then = std::make_unique<BreakStmt>();
  auto node = std::make_unique<IfStmt>(std::move(cond), std::move(then), nullptr);
  EXPECT_EQ(node->kind(), ASTKind::IfStmt);
  EXPECT_NE(node->condition(), nullptr);
  EXPECT_NE(node->thenBranch(), nullptr);
  EXPECT_EQ(node->elseBranch(), nullptr);
}

TEST(ASTTest, CreateWhileStmt) {
  auto cond = std::make_unique<IdentifierExpr>("x");
  auto body = std::make_unique<BreakStmt>();
  auto node = std::make_unique<WhileStmt>(std::move(cond), std::move(body));
  EXPECT_EQ(node->kind(), ASTKind::WhileStmt);
}

TEST(ASTTest, CreateFuncDef) {
  auto body = std::make_unique<BlockStmt>();
  std::vector<ParamDecl> params;
  auto node = std::make_unique<FuncDef>(TypeKind::Int, "main",
                                         std::move(params), std::move(body));
  EXPECT_EQ(node->kind(), ASTKind::FuncDef);
  EXPECT_EQ(node->name(), "main");
  EXPECT_EQ(node->returnType(), TypeKind::Int);
}

TEST(ASTTest, CreateCompUnit) {
  auto node = std::make_unique<CompUnit>();
  EXPECT_EQ(node->kind(), ASTKind::CompUnit);
  EXPECT_TRUE(node->items().empty());
}

// ── 2. unique_ptr ownership ─────────────────────────────────────────────

TEST(ASTTest, UniquePtrOwnership) {
  auto expr = std::make_unique<IntegerLiteralExpr>("99");
  auto* raw = expr.get();
  EXPECT_EQ(raw->rawValue(), "99");

  auto block = std::make_unique<BlockStmt>();
  block->addStatement(std::make_unique<ExprStmt>(std::move(expr)));
  EXPECT_EQ(block->statements().size(), 1u);
}

// ── 3. SourceRange can be saved ─────────────────────────────────────────

TEST(ASTTest, SourceRangeOnNodes) {
  auto node = std::make_unique<IntegerLiteralExpr>("42");
  SourceRange range(SourceLocation{0, 1, 1}, SourceLocation{2, 1, 3});
  node->setRange(range);
  EXPECT_EQ(node->range().begin.offset, 0u);
  EXPECT_EQ(node->range().end.offset, 2u);
  EXPECT_EQ(node->range().begin.line, 1u);
  EXPECT_EQ(node->range().begin.column, 1u);
}

// ── 4. Enum name stability ─────────────────────────────────────────────

TEST(ASTTest, TypeKindNames) {
  EXPECT_STREQ(typeKindName(TypeKind::Int), "int");
  EXPECT_STREQ(typeKindName(TypeKind::Void), "void");
}

TEST(ASTTest, UnaryOperatorNames) {
  EXPECT_STREQ(unaryOpName(UnaryOperator::Plus), "Plus");
  EXPECT_STREQ(unaryOpName(UnaryOperator::Minus), "Minus");
  EXPECT_STREQ(unaryOpName(UnaryOperator::LogicalNot), "LogicalNot");
}

TEST(ASTTest, BinaryOperatorNames) {
  EXPECT_STREQ(binaryOpName(BinaryOperator::LogicalOr), "LogicalOr");
  EXPECT_STREQ(binaryOpName(BinaryOperator::LogicalAnd), "LogicalAnd");
  EXPECT_STREQ(binaryOpName(BinaryOperator::Equal), "Equal");
  EXPECT_STREQ(binaryOpName(BinaryOperator::NotEqual), "NotEqual");
  EXPECT_STREQ(binaryOpName(BinaryOperator::Less), "Less");
  EXPECT_STREQ(binaryOpName(BinaryOperator::LessEqual), "LessEqual");
  EXPECT_STREQ(binaryOpName(BinaryOperator::Greater), "Greater");
  EXPECT_STREQ(binaryOpName(BinaryOperator::GreaterEqual), "GreaterEqual");
  EXPECT_STREQ(binaryOpName(BinaryOperator::Add), "Add");
  EXPECT_STREQ(binaryOpName(BinaryOperator::Subtract), "Subtract");
  EXPECT_STREQ(binaryOpName(BinaryOperator::Multiply), "Multiply");
  EXPECT_STREQ(binaryOpName(BinaryOperator::Divide), "Divide");
  EXPECT_STREQ(binaryOpName(BinaryOperator::Modulo), "Modulo");
}

// ── 5. ASTPrinter output stability ─────────────────────────────────────

TEST(ASTTest, PrinterMinimalMain) {
  auto ast = expectParse("int main() { return 0; }");
  std::ostringstream out;
  dumpAst(*ast, out);
  std::string result = out.str();

  EXPECT_NE(result.find("CompUnit"), std::string::npos);
  EXPECT_NE(result.find("FuncDef return=int name=main"), std::string::npos);
  EXPECT_NE(result.find("Params"), std::string::npos);
  EXPECT_NE(result.find("BlockStmt"), std::string::npos);
  EXPECT_NE(result.find("ReturnStmt"), std::string::npos);
  EXPECT_NE(result.find("IntegerLiteral value=0"), std::string::npos);
}

TEST(ASTTest, PrinterGlobalVarDecl) {
  auto ast = expectParse("int x = 42;");
  std::ostringstream out;
  dumpAst(*ast, out);
  std::string result = out.str();

  EXPECT_NE(result.find("GlobalVarDecl name=x"), std::string::npos);
  EXPECT_NE(result.find("Initializer"), std::string::npos);
  EXPECT_NE(result.find("IntegerLiteral value=42"), std::string::npos);
}

TEST(ASTTest, PrinterGlobalConstDecl) {
  auto ast = expectParse("const int N = 10;");
  std::ostringstream out;
  dumpAst(*ast, out);
  std::string result = out.str();

  EXPECT_NE(result.find("GlobalConstDecl name=N"), std::string::npos);
  EXPECT_NE(result.find("IntegerLiteral value=10"), std::string::npos);
}

TEST(ASTTest, PrinterBinaryExpr) {
  auto ast = expectParse("int x = 1 + 2 * 3;");
  std::ostringstream out;
  dumpAst(*ast, out);
  std::string result = out.str();

  EXPECT_NE(result.find("BinaryExpr op=Add"), std::string::npos);
  EXPECT_NE(result.find("BinaryExpr op=Multiply"), std::string::npos);
}

TEST(ASTTest, PrinterCallExpr) {
  auto ast = expectParse("int main() { return foo(1, 2); }");
  std::ostringstream out;
  dumpAst(*ast, out);
  std::string result = out.str();

  EXPECT_NE(result.find("CallExpr callee=foo"), std::string::npos);
  EXPECT_NE(result.find("Arguments"), std::string::npos);
}

TEST(ASTTest, PrinterIfStmt) {
  auto ast = expectParse("int main() { if (1) return 1; else return 0; }");
  std::ostringstream out;
  dumpAst(*ast, out);
  std::string result = out.str();

  EXPECT_NE(result.find("IfStmt"), std::string::npos);
  EXPECT_NE(result.find("Condition"), std::string::npos);
  EXPECT_NE(result.find("Then"), std::string::npos);
  EXPECT_NE(result.find("Else"), std::string::npos);
}

TEST(ASTTest, PrinterWhileStmt) {
  auto ast = expectParse("int main() { while (1) break; }");
  std::ostringstream out;
  dumpAst(*ast, out);
  std::string result = out.str();

  EXPECT_NE(result.find("WhileStmt"), std::string::npos);
  EXPECT_NE(result.find("BreakStmt"), std::string::npos);
}

} // namespace toyc
