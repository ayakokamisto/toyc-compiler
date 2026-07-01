#include <gtest/gtest.h>

#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/frontend/semantic_analyzer.h"
#include "toyc/ir/ir_builder.h"
#include "toyc/ir/ir_printer.h"

class TestHelper {
public:
    static IRProgram* build(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        Program program = parser.parse_program();
        SemanticAnalyzer sema;
        bool ok = sema.analyze(program);
        if (!ok) return nullptr;
        return programs_.emplace_back(IRBuilder::build(program)).get();
    }
private:
    static std::vector<std::unique_ptr<IRProgram>> programs_;
};
std::vector<std::unique_ptr<IRProgram>> TestHelper::programs_;

TEST(IRBuilderTest, SimpleReturn42) {
    auto* ir = TestHelper::build("int main() { return 42; }");
    ASSERT_NE(ir, nullptr);
    std::string out = IRPrinter::print(*ir);
    EXPECT_NE(out.find("imm 42"), std::string::npos);
    EXPECT_NE(out.find("ret"), std::string::npos);
}

TEST(IRBuilderTest, ReturnZero) {
    auto* ir = TestHelper::build("int main() { return 0; }");
    ASSERT_NE(ir, nullptr);
    EXPECT_NE(IRPrinter::print(*ir).find("imm 0"), std::string::npos);
}

TEST(IRBuilderTest, BinaryAdd) {
    auto* ir = TestHelper::build("int main() { return 2+3; }");
    ASSERT_NE(ir, nullptr);
    EXPECT_NE(IRPrinter::print(*ir).find("add"), std::string::npos);
}

TEST(IRBuilderTest, Compare) {
    auto* ir = TestHelper::build("int main() { return 3<5; }");
    ASSERT_NE(ir, nullptr);
    EXPECT_NE(IRPrinter::print(*ir).find("cmp lt"), std::string::npos);
}

TEST(IRBuilderTest, UnaryMinus) {
    auto* ir = TestHelper::build("int main() { return -42; }");
    ASSERT_NE(ir, nullptr);
    EXPECT_NE(IRPrinter::print(*ir).find("neg"), std::string::npos);
}

TEST(IRBuilderTest, UnaryNot) {
    auto* ir = TestHelper::build("int main() { return !0; }");
    ASSERT_NE(ir, nullptr);
    EXPECT_NE(IRPrinter::print(*ir).find("not"), std::string::npos);
}

TEST(IRBuilderTest, PrinterStability) {
    auto* ir = TestHelper::build("int main() { return 1+2*3; }");
    ASSERT_NE(ir, nullptr);
    EXPECT_EQ(IRPrinter::print(*ir), IRPrinter::print(*ir));
}

TEST(IRBuilderTest, LogicalAnd) {
    auto* ir = TestHelper::build("int main() { return 1 && 0; }");
    ASSERT_NE(ir, nullptr);
    std::string out = IRPrinter::print(*ir);
    EXPECT_NE(out.find("land.rhs"), std::string::npos);
    EXPECT_NE(out.find("land.end"), std::string::npos);
    EXPECT_NE(out.find("alloca"), std::string::npos);
}

TEST(IRBuilderTest, LogicalOr) {
    auto* ir = TestHelper::build("int main() { return 1 || 0; }");
    ASSERT_NE(ir, nullptr);
    EXPECT_NE(IRPrinter::print(*ir).find("lor.rhs"), std::string::npos);
}

TEST(IRBuilderTest, ShortCircuitAndDivZero) {
    auto* ir = TestHelper::build("int main() { return 0 && (1/0); }");
    ASSERT_NE(ir, nullptr);
    EXPECT_NE(IRPrinter::print(*ir).find("land.rhs"), std::string::npos);
}

TEST(IRBuilderTest, ShortCircuitOrDivZero) {
    auto* ir = TestHelper::build("int main() { return 1 || (1/0); }");
    ASSERT_NE(ir, nullptr);
    EXPECT_NE(IRPrinter::print(*ir).find("lor.rhs"), std::string::npos);
}

TEST(IRBuilderTest, VariableDeclaration) {
    auto* ir = TestHelper::build("int main() { int x = 42; return x; }");
    ASSERT_NE(ir, nullptr);
    std::string out = IRPrinter::print(*ir);
    EXPECT_NE(out.find("alloca"), std::string::npos);
    EXPECT_NE(out.find("store"), std::string::npos);
    EXPECT_NE(out.find("load"), std::string::npos);
}

TEST(IRBuilderTest, Assignment) {
    auto* ir = TestHelper::build("int main() { int x = 0; x = 99; return x; }");
    ASSERT_NE(ir, nullptr);
    std::string out = IRPrinter::print(*ir);
    EXPECT_NE(out.find("store"), std::string::npos);
}

TEST(IRBuilderTest, IfElse) {
    auto* ir = TestHelper::build(
        "int main() { int x = 1; if (x) return 7; else return 9; }");
    ASSERT_NE(ir, nullptr);
    std::string out = IRPrinter::print(*ir);
    EXPECT_NE(out.find("cbr"), std::string::npos);
    EXPECT_NE(out.find("if.then"), std::string::npos);
    EXPECT_NE(out.find("if.else"), std::string::npos);
}

TEST(IRBuilderTest, WhileLoop) {
    auto* ir = TestHelper::build(
        "int main() { while (0) return 1; return 0; }");
    ASSERT_NE(ir, nullptr);
    std::string out = IRPrinter::print(*ir);
    EXPECT_NE(out.find("while.cond"), std::string::npos);
    EXPECT_NE(out.find("while.body"), std::string::npos);
    EXPECT_NE(out.find("while.end"), std::string::npos);
}
