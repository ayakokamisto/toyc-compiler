#include "codegen/opt/PeepholeOptimizer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen peephole test failure: " << message << '\n';
    std::exit(1);
}

void expectAbsent(const std::string& text, std::string_view fragment, std::string_view message) {
    if (text.find(fragment) != std::string::npos) {
        std::cerr << "unexpected fragment in output:\n" << text << "\nfragment: \"" << fragment
                  << "\"\n";
        fail(message);
    }
}

void expectPresent(const std::string& text, std::string_view fragment, std::string_view message) {
    if (text.find(fragment) == std::string::npos) {
        std::cerr << "missing fragment in output:\n" << text << "\nfragment: \"" << fragment
                  << "\"\n";
        fail(message);
    }
}

void testRemovesRedundantMove() {
    const std::string input = "    mv t0, t0\n    ret\n";
    const std::string output = toyc::codegen::PeepholeOptimizer::optimize(input);
    expectAbsent(output, "mv t0, t0", "redundant mv is removed");
    expectPresent(output, "ret", "following instruction is preserved");
}

void testRemovesFallThroughJump() {
    const std::string input =
        "    j target\n"
        "target:\n"
        "    ret\n";
    const std::string output = toyc::codegen::PeepholeOptimizer::optimize(input);
    expectAbsent(output, "    j target\n", "fall-through jump is removed");
    expectPresent(output, "target:\n", "target label is preserved");
}

void testPreservesNonFallThroughJump() {
    const std::string input =
        "    j far\n"
        "near:\n"
        "    ret\n"
        "far:\n"
        "    ret\n";
    const std::string output = toyc::codegen::PeepholeOptimizer::optimize(input);
    expectPresent(output, "    j far\n", "non-fall-through jump is kept");
}

void testPreservesDistinctMove() {
    const std::string input = "    mv t1, t0\n    ret\n";
    const std::string output = toyc::codegen::PeepholeOptimizer::optimize(input);
    expectPresent(output, "mv t1, t0", "distinct mv is kept");
}

void testFoldsZeroStore() {
    const std::string input =
        "    li t0, 0\n"
        "    sw t0, -12(s0)\n";
    const std::string output = toyc::codegen::PeepholeOptimizer::optimize(input);
    expectPresent(output, "    sw zero, -12(s0)\n", "zero store is folded");
    expectAbsent(output, "li t0, 0", "li 0 is removed");
}

} // namespace

int main() {
    testRemovesRedundantMove();
    testRemovesFallThroughJump();
    testPreservesNonFallThroughJump();
    testPreservesDistinctMove();
    testFoldsZeroStore();
    return 0;
}
