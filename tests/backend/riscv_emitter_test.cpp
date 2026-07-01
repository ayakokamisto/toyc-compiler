#include <gtest/gtest.h>

#include "toyc/backend/riscv_emitter.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/frontend/semantic_analyzer.h"
#include "toyc/ir/ir_builder.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

class EmitterTestHelper {
public:
    static std::unique_ptr<IRProgram> build_ir(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        Program program = parser.parse_program();
        SemanticAnalyzer sema;
        bool ok = sema.analyze(program);
        if (!ok) return nullptr;
        return IRBuilder::build(program);
    }

    static std::string compile(const std::string& source) {
        auto ir = build_ir(source);
        if (!ir) return "";
        return RiscvEmitter::emit(*ir);
    }
};

static std::vector<std::string> labelDefinitions(const std::string& assembly) {
    std::vector<std::string> labels;
    std::istringstream in(assembly);
    std::string line;
    std::regex labelRe(R"(^([A-Za-z_.$][A-Za-z0-9_.$]*):$)");
    while (std::getline(in, line)) {
        std::smatch match;
        if (std::regex_match(line, match, labelRe)) labels.push_back(match[1].str());
    }
    return labels;
}

static std::vector<std::string> branchTargets(const std::string& assembly) {
    std::vector<std::string> targets;
    std::istringstream in(assembly);
    std::string line;
    std::regex jRe(R"(^\s*j\s+([A-Za-z_.$][A-Za-z0-9_.$]*)\s*$)");
    std::regex bnezRe(R"(^\s*bnez\s+\w+,\s*([A-Za-z_.$][A-Za-z0-9_.$]*)\s*$)");
    while (std::getline(in, line)) {
        std::smatch match;
        if (std::regex_match(line, match, jRe) || std::regex_match(line, match, bnezRe)) {
            targets.push_back(match[1].str());
        }
    }
    return targets;
}

struct Region {
    uint32_t begin;
    uint32_t end;
};

static bool overlaps(Region a, Region b) {
    return a.begin < b.end && b.begin < a.end;
}

static const Function* findFunction(const IRProgram& ir, const std::string& name) {
    for (const auto& fn : ir.module()->functions()) {
        if (fn->name() == name) return fn.get();
    }
    return nullptr;
}

TEST(RiscvEmitterTest, SimpleReturn42) {
    std::string a = EmitterTestHelper::compile("int main() { return 42; }");
    EXPECT_NE(a.find("sw ra,"), std::string::npos);
    EXPECT_NE(a.find("ret"), std::string::npos);
}

TEST(RiscvEmitterTest, ReturnZero) {
    std::string a = EmitterTestHelper::compile("int main() { return 0; }");
    EXPECT_NE(a.find("sw ra,"), std::string::npos);
}

TEST(RiscvEmitterTest, BinaryAdd) {
    std::string a = EmitterTestHelper::compile("int main() { return 2+3; }");
    EXPECT_NE(a.find("add"), std::string::npos);
}

TEST(RiscvEmitterTest, CompareLt) {
    std::string a = EmitterTestHelper::compile("int main() { return 3<5; }");
    EXPECT_NE(a.find("slt"), std::string::npos);
}

TEST(RiscvEmitterTest, UnaryNeg) {
    std::string a = EmitterTestHelper::compile("int main() { return -5; }");
    EXPECT_NE(a.find("neg"), std::string::npos);
}

TEST(RiscvEmitterTest, UnaryNot) {
    std::string a = EmitterTestHelper::compile("int main() { return !0; }");
    EXPECT_NE(a.find("seqz"), std::string::npos);
}

TEST(RiscvEmitterTest, HasTextSection) {
    std::string a = EmitterTestHelper::compile("int main() { return 1; }");
    EXPECT_NE(a.find(".text"), std::string::npos);
}

TEST(RiscvEmitterTest, ReturnUsesA0) {
    std::string a = EmitterTestHelper::compile("int main() { return 99; }");
    EXPECT_NE(a.find("a0"), std::string::npos);
}

TEST(RiscvEmitterTest, ShortCircuitAnd) {
    std::string a = EmitterTestHelper::compile("int main() { return 0 && (1/0); }");
    EXPECT_NE(a.find("div"), std::string::npos);
    EXPECT_NE(a.find("land_rhs"), std::string::npos);
    EXPECT_NE(a.find("land_end"), std::string::npos);
}

TEST(RiscvEmitterTest, ShortCircuitOr) {
    std::string a = EmitterTestHelper::compile("int main() { return 1 || (1/0); }");
    EXPECT_NE(a.find("lor_rhs"), std::string::npos);
}

TEST(RiscvEmitterTest, MultiFunctionBlockLabelUniqueness) {
    std::string a = EmitterTestHelper::compile(
        "int add(int a, int b) { return a + b; }"
        "int main() { return add(17, 25); }");
    auto labels = labelDefinitions(a);
    std::set<std::string> unique(labels.begin(), labels.end());
    EXPECT_EQ(labels.size(), unique.size());
    EXPECT_NE(unique.find("add"), unique.end());
    EXPECT_NE(unique.find("main"), unique.end());
    EXPECT_EQ(unique.count("entry.0"), 0u);
}

TEST(RiscvEmitterTest, BranchTargetsResolveWithinFunction) {
    std::string a = EmitterTestHelper::compile(
        "int fact(int n) { if (n <= 1) return 1; return n * fact(n - 1); }"
        "int main() { return fact(5); }");
    auto labels = labelDefinitions(a);
    std::map<std::string, int> defs;
    for (const auto& label : labels) defs[label]++;
    for (const auto& target : branchTargets(a)) {
        EXPECT_EQ(defs[target], 1) << target;
    }
}

TEST(RiscvEmitterTest, AllAssemblyLabelsGloballyUnique) {
    std::string a = EmitterTestHelper::compile(
        "int add(int a, int b) { return a + b; }"
        "int fact(int n) { if (n <= 1) return 1; return n * fact(n - 1); }"
        "int sum9(int a,int b,int c,int d,int e,int f,int g,int h,int i) {"
        "return a+b+c+d+e+f+g+h+i; }"
        "int main() { return add(fact(3), sum9(1,1,1,1,1,1,1,1,1)); }");
    auto labels = labelDefinitions(a);
    std::set<std::string> unique(labels.begin(), labels.end());
    EXPECT_EQ(labels.size(), unique.size());
    EXPECT_NE(unique.find("add"), unique.end());
    EXPECT_NE(unique.find("fact"), unique.end());
    EXPECT_NE(unique.find("sum9"), unique.end());
    EXPECT_NE(unique.find("main"), unique.end());
}

TEST(FrameLayoutTest, OutgoingAreaStartsAtZero) {
    auto ir = EmitterTestHelper::build_ir(
        "int sum9(int a,int b,int c,int d,int e,int f,int g,int h,int i) {"
        "return a+b+c+d+e+f+g+h+i; }"
        "int main() { return sum9(1,2,3,4,5,6,7,8,9); }");
    ASSERT_NE(ir, nullptr);
    const Function* main = findFunction(*ir, "main");
    ASSERT_NE(main, nullptr);
    FrameLayout layout = FrameLayout::compute(*main);
    EXPECT_EQ(layout.outgoingArgBytes, 4u);
    EXPECT_EQ(layout.outgoingArgOffset(0), 0u);
}

TEST(FrameLayoutTest, ValueHomeAfterOutgoingArea) {
    auto ir = EmitterTestHelper::build_ir(
        "int sum9(int a,int b,int c,int d,int e,int f,int g,int h,int i) {"
        "return a+b+c+d+e+f+g+h+i; }"
        "int main() { return sum9(1,2,3,4,5,6,7,8,9); }");
    ASSERT_NE(ir, nullptr);
    const Function* main = findFunction(*ir, "main");
    ASSERT_NE(main, nullptr);
    FrameLayout layout = FrameLayout::compute(*main);
    for (const auto& [value, offset] : layout.valueHome) {
        EXPECT_GE(offset, layout.outgoingArgBytes);
    }
}

TEST(FrameLayoutTest, AllocaHomeAfterValueHome) {
    auto ir = EmitterTestHelper::build_ir(
        "int sum9(int a,int b,int c,int d,int e,int f,int g,int h,int i) {"
        "return a+b+c+d+e+f+g+h+i; }"
        "int main() { int x = 9; return sum9(1,2,3,4,5,6,7,8,x); }");
    ASSERT_NE(ir, nullptr);
    const Function* main = findFunction(*ir, "main");
    ASSERT_NE(main, nullptr);
    FrameLayout layout = FrameLayout::compute(*main);
    uint32_t maxValueEnd = layout.outgoingArgBytes;
    for (const auto& [value, offset] : layout.valueHome) maxValueEnd = std::max(maxValueEnd, offset + 4);
    for (const auto& [value, offset] : layout.allocaHome) {
        EXPECT_GE(offset, maxValueEnd);
    }
}

TEST(FrameLayoutTest, SaveAreaNoOverlap) {
    auto ir = EmitterTestHelper::build_ir(
        "int main() { int x = 1; return x + 41; }");
    ASSERT_NE(ir, nullptr);
    const Function* main = findFunction(*ir, "main");
    ASSERT_NE(main, nullptr);
    FrameLayout layout = FrameLayout::compute(*main);
    EXPECT_EQ(layout.s0Offset + 4, layout.raOffset);
    EXPECT_LE(layout.raOffset + 4, layout.frameSize);
    for (const auto& [value, offset] : layout.valueHome) {
        EXPECT_FALSE(overlaps({offset, offset + 4}, {layout.s0Offset, layout.s0Offset + 4}));
        EXPECT_FALSE(overlaps({offset, offset + 4}, {layout.raOffset, layout.raOffset + 4}));
    }
    for (const auto& [value, offset] : layout.allocaHome) {
        EXPECT_FALSE(overlaps({offset, offset + 4}, {layout.s0Offset, layout.s0Offset + 4}));
        EXPECT_FALSE(overlaps({offset, offset + 4}, {layout.raOffset, layout.raOffset + 4}));
    }
}

TEST(FrameLayoutTest, AllRegionsNonOverlap) {
    auto ir = EmitterTestHelper::build_ir(
        "int sum12(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k,int l) {"
        "return a+b+c+d+e+f+g+h+i+j+k+l; }"
        "int main() { int x = 12; return sum12(1,2,3,4,5,6,7,8,9,10,11,x); }");
    ASSERT_NE(ir, nullptr);
    const Function* main = findFunction(*ir, "main");
    ASSERT_NE(main, nullptr);
    FrameLayout layout = FrameLayout::compute(*main);
    std::vector<Region> regions;
    regions.push_back({0, layout.outgoingArgBytes});
    for (const auto& [value, offset] : layout.valueHome) regions.push_back({offset, offset + 4});
    for (const auto& [value, offset] : layout.allocaHome) regions.push_back({offset, offset + 4});
    regions.push_back({layout.s0Offset, layout.s0Offset + 4});
    regions.push_back({layout.raOffset, layout.raOffset + 4});
    for (size_t i = 0; i < regions.size(); ++i) {
        if (regions[i].begin == regions[i].end) continue;
        for (size_t j = i + 1; j < regions.size(); ++j) {
            if (regions[j].begin == regions[j].end) continue;
            EXPECT_FALSE(overlaps(regions[i], regions[j])) << i << " overlaps " << j;
        }
    }
}

TEST(FrameLayoutTest, NestedCallOutgoingArea) {
    auto ir = EmitterTestHelper::build_ir(
        "int id(int x) { return x; }"
        "int sum9(int a,int b,int c,int d,int e,int f,int g,int h,int i) {"
        "return a+b+c+d+e+f+g+h+i; }"
        "int main() { return sum9(id(1),id(2),id(3),id(4),id(5),id(6),id(7),id(8),id(9)); }");
    ASSERT_NE(ir, nullptr);
    const Function* main = findFunction(*ir, "main");
    ASSERT_NE(main, nullptr);
    FrameLayout layout = FrameLayout::compute(*main);
    EXPECT_EQ(layout.outgoingArgBytes, 4u);
    for (const auto& [value, offset] : layout.valueHome) EXPECT_GE(offset, 4u);
    for (const auto& [value, offset] : layout.allocaHome) EXPECT_GE(offset, 4u);
}
