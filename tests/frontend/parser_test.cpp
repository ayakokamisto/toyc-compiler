#include <gtest/gtest.h>

#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/frontend/semantic_analyzer.h"

#include <sstream>

class ParserTestHelper {
public:
    static Program parse(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        return parser.parse_program();
    }

    static std::string semantic_errors(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        Program program = parser.parse_program();
        SemanticAnalyzer sema;
        sema.analyze(program);
        std::ostringstream os;
        sema.print_errors(os);
        return os.str();
    }

    static Expr* get_return_expr(const Program& prog) {
        auto& stmts = prog.functions[0].body->statements;
        if (stmts.empty()) return nullptr;
        auto* ret = dynamic_cast<ReturnStmt*>(stmts[0].get());
        return ret ? ret->value.get() : nullptr;
    }
};

TEST(ParserTest, SimpleReturn42) {
    Program prog = ParserTestHelper::parse("int main() { return 42; }");
    EXPECT_EQ(prog.functions[0].name, "main");
    Expr* expr = ParserTestHelper::get_return_expr(prog);
    ASSERT_NE(expr, nullptr);
    auto* int_lit = dynamic_cast<IntLiteralExpr*>(expr);
    ASSERT_NE(int_lit, nullptr);
    EXPECT_EQ(int_lit->value, 42);
}

TEST(ParserTest, ReturnZero) {
    Program prog = ParserTestHelper::parse("int main() { return 0; }");
    auto* int_lit = dynamic_cast<IntLiteralExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(int_lit, nullptr);
    EXPECT_EQ(int_lit->value, 0);
}

TEST(ParserTest, UnaryMinus) {
    Program prog = ParserTestHelper::parse("int main() { return -5; }");
    auto* unary = dynamic_cast<UnaryExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->op, UnaryOp::Minus);
}

TEST(ParserTest, UnaryNot) {
    Program prog = ParserTestHelper::parse("int main() { return !0; }");
    auto* unary = dynamic_cast<UnaryExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->op, UnaryOp::Not);
}

TEST(ParserTest, BinaryAdd) {
    Program prog = ParserTestHelper::parse("int main() { return 2+3; }");
    auto* binary = dynamic_cast<BinaryExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(binary, nullptr);
    EXPECT_EQ(binary->op, BinaryOp::Add);
}

TEST(ParserTest, PrecedenceAddMul) {
    Program prog = ParserTestHelper::parse("int main() { return 2+3*4; }");
    auto* add = dynamic_cast<BinaryExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->op, BinaryOp::Add);
    auto* mul = dynamic_cast<BinaryExpr*>(add->right.get());
    ASSERT_NE(mul, nullptr);
    EXPECT_EQ(mul->op, BinaryOp::Mul);
}

TEST(ParserTest, ParenthesesOverridePrecedence) {
    Program prog = ParserTestHelper::parse("int main() { return (2+3)*4; }");
    auto* mul = dynamic_cast<BinaryExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(mul, nullptr);
    EXPECT_EQ(mul->op, BinaryOp::Mul);
    auto* add = dynamic_cast<BinaryExpr*>(mul->left.get());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->op, BinaryOp::Add);
}

TEST(ParserTest, Comparison) {
    Program prog = ParserTestHelper::parse("int main() { return 3 < 5; }");
    auto* cmp = dynamic_cast<BinaryExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(cmp, nullptr);
    EXPECT_EQ(cmp->op, BinaryOp::Lt);
}

TEST(ParserTest, Equality) {
    Program prog = ParserTestHelper::parse("int main() { return 3 == 5; }");
    auto* eq = dynamic_cast<BinaryExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(eq, nullptr);
    EXPECT_EQ(eq->op, BinaryOp::Eq);
}

TEST(ParserTest, LogicalAndOr) {
    Program prog = ParserTestHelper::parse("int main() { return 1 && 0; }");
    auto* and_expr = dynamic_cast<BinaryExpr*>(
        ParserTestHelper::get_return_expr(prog));
    ASSERT_NE(and_expr, nullptr);
    EXPECT_EQ(and_expr->op, BinaryOp::And);
    Program prog2 = ParserTestHelper::parse("int main() { return 1 || 0; }");
    auto* or_expr = dynamic_cast<BinaryExpr*>(
        ParserTestHelper::get_return_expr(prog2));
    ASSERT_NE(or_expr, nullptr);
    EXPECT_EQ(or_expr->op, BinaryOp::Or);
}

TEST(ParserTest, VarDecl) {
    Program prog = ParserTestHelper::parse("int main() { int x = 5; return x; }");
    auto& stmts = prog.functions[0].body->statements;
    ASSERT_EQ(stmts.size(), 2u);
    auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->name, "x");
}

TEST(ParserTest, Assignment) {
    Program prog = ParserTestHelper::parse("int main() { int x = 0; x = 5; return x; }");
    auto& stmts = prog.functions[0].body->statements;
    ASSERT_EQ(stmts.size(), 3u);
    auto* assign = dynamic_cast<AssignStmt*>(stmts[1].get());
    ASSERT_NE(assign, nullptr);
    EXPECT_EQ(assign->name, "x");
}

TEST(ParserTest, IfElse) {
    Program prog = ParserTestHelper::parse(
        "int main() { if (1) return 7; else return 9; }");
    auto& stmts = prog.functions[0].body->statements;
    auto* ifs = dynamic_cast<IfStmt*>(stmts[0].get());
    ASSERT_NE(ifs, nullptr);
    EXPECT_NE(ifs->thenStmt, nullptr);
    EXPECT_NE(ifs->elseStmt, nullptr);
}

TEST(ParserTest, DanglingElse) {
    Program prog = ParserTestHelper::parse(
        "int main() { int x = 0; if (1) if (0) x = 1; else x = 2; return x; }");
    auto& stmts = prog.functions[0].body->statements;
    auto* outer_if = dynamic_cast<IfStmt*>(stmts[1].get());
    ASSERT_NE(outer_if, nullptr);
    EXPECT_TRUE(dynamic_cast<IfStmt*>(outer_if->thenStmt.get()));
}

// Error cases
TEST(ParserTest, ErrorMissingSemicolon) {
    EXPECT_THROW(ParserTestHelper::parse("int main() { return 42 }"), ParseError);
}

TEST(ParserTest, ErrorMissingParen) {
    EXPECT_THROW(ParserTestHelper::parse("int main( { return 42; }"), ParseError);
}

TEST(ParserTest, ErrorExtraTokens) {
    EXPECT_THROW(ParserTestHelper::parse("int main() { return 42; } extra"), ParseError);
}

TEST(ParserTest, ErrorNonMain) {
    
    Lexer lexer("int foo() { return 0; }");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    Program program = parser.parse_program();
    SemanticAnalyzer sema;
    EXPECT_FALSE(sema.analyze(program));
}

TEST(ParserTest, SemanticMissingReturn) {
    Lexer lexer("int main() { int x = 42; }");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    Program program = parser.parse_program();
    SemanticAnalyzer sema;
    EXPECT_FALSE(sema.analyze(program));
}

TEST(ParserTest, SemanticRejectsUninitializedGlobal) {
    std::string errors = ParserTestHelper::semantic_errors("int g; int main() { return 0; }");
    EXPECT_NE(errors.find("global variable 'g' requires an initializer"), std::string::npos);
}

TEST(ParserTest, SemanticRejectsVoidCallInReturnValue) {
    std::string errors = ParserTestHelper::semantic_errors(
        "void sink() { return; } int main() { return sink(); }");
    EXPECT_NE(errors.find("void function call cannot be used as a value expression"), std::string::npos);
}

TEST(ParserTest, SemanticRejectsVoidCallInBinaryLeft) {
    std::string errors = ParserTestHelper::semantic_errors(
        "void sink() { return; } int main() { return sink() + 1; }");
    EXPECT_NE(errors.find("void function call cannot be used as a value expression"), std::string::npos);
}

TEST(ParserTest, SemanticRejectsVoidCallInBinaryRight) {
    std::string errors = ParserTestHelper::semantic_errors(
        "void sink() { return; } int main() { return 1 + sink(); }");
    EXPECT_NE(errors.find("void function call cannot be used as a value expression"), std::string::npos);
}

TEST(ParserTest, SemanticRejectsVoidCallInInitializer) {
    std::string errors = ParserTestHelper::semantic_errors(
        "void sink() { return; } int main() { int x = sink(); return 0; }");
    EXPECT_NE(errors.find("void function call cannot be used as a value expression"), std::string::npos);
}

TEST(ParserTest, SemanticRejectsVoidCallAsIntArgument) {
    std::string errors = ParserTestHelper::semantic_errors(
        "void sink() { return; } int foo(int x) { return x; } int main() { return foo(sink()); }");
    EXPECT_NE(errors.find("void function call cannot be used as a value expression"), std::string::npos);
}

TEST(ParserTest, SemanticAllowsVoidCallExpressionStatement) {
    Lexer lexer("void sink() { return; } int main() { sink(); return 42; }");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    Program program = parser.parse_program();
    SemanticAnalyzer sema;
    EXPECT_TRUE(sema.analyze(program));
}

TEST(ParserTest, ConstGlobalDeclaration) {
    Program prog = ParserTestHelper::parse("const int g = 1 || (1 / 0); int main() { return g; }");
    ASSERT_EQ(prog.globals.size(), 1u);
    EXPECT_EQ(prog.globals[0].name, "g");
    EXPECT_TRUE(prog.globals[0].isConst);
    EXPECT_NE(dynamic_cast<BinaryExpr*>(prog.globals[0].initializer.get()), nullptr);
}

TEST(ParserTest, ConstLocalDeclaration) {
    Program prog = ParserTestHelper::parse("int main() { const int x = 7; return x; }");
    auto& stmts = prog.functions[0].body->statements;
    ASSERT_EQ(stmts.size(), 2u);
    auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->name, "x");
    EXPECT_TRUE(decl->isConst);
}

TEST(ParserTest, SemanticAllowsConstShortCircuitInitializer) {
    std::string errors = ParserTestHelper::semantic_errors(
        "const int g = 1 || (1 / 0); int main() { const int x = 0 && (1 / 0); return g + x; }");
    EXPECT_TRUE(errors.empty()) << errors;
}

TEST(ParserTest, SemanticRejectsConstAssignment) {
    std::string errors = ParserTestHelper::semantic_errors(
        "const int g = 1; int main() { g = 2; return g; }");
    EXPECT_NE(errors.find("assignment to const variable 'g'"), std::string::npos);
}

TEST(ParserTest, SemanticRejectsLocalConstAssignment) {
    std::string errors = ParserTestHelper::semantic_errors(
        "int main() { const int x = 1; x = 2; return x; }");
    EXPECT_NE(errors.find("assignment to const variable 'x'"), std::string::npos);
}

TEST(ParserTest, SemanticRejectsNonconstantConstInitializer) {
    std::string errors = ParserTestHelper::semantic_errors(
        "int f() { return 7; } int main() { const int x = f(); return x; }");
    EXPECT_NE(errors.find("const initializer is not a constant integer expression"), std::string::npos);
}
