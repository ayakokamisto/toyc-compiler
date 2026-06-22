#include "codegen/RiscvBackend.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen backend smoke test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void expectAssembly(const std::string& actual, const std::string& expected) {
    if (actual != expected) {
        std::cerr << "actual assembly:\n" << actual << "\nexpected assembly:\n" << expected << '\n';
        fail("assembly mismatch");
    }
}

void testConstCopyReturnMain() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%x", 42},
                toyc::codegen::contract::CopyInst{"%ret", "%x"},
            },
            toyc::codegen::contract::ReturnInst{"%ret"},
        }},
    });

    const std::string expected =
        "    .data\n"
        "\n"
        "    .text\n"
        "\n"
        "    .global main\n"
        "main:\n"
        "    addi sp, sp, -16\n"
        "    sw ra, 12(sp)\n"
        "    sw s0, 8(sp)\n"
        "    addi s0, sp, 16\n"
        "    li t0, 42\n"
        "    sw t0, -12(s0)\n"
        "    lw t0, -12(s0)\n"
        "    sw t0, -16(s0)\n"
        "    lw a0, -16(s0)\n"
        "    j .Lmain__epilogue\n"
        ".Lmain__epilogue:\n"
        "    lw ra, 12(sp)\n"
        "    lw s0, 8(sp)\n"
        "    addi sp, sp, 16\n"
        "    ret\n";

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    expectAssembly(assembly, expected);
}

void testGlobalsAndParameterLanding() {
    toyc::codegen::contract::IRModule module;
    module.globalVars.push_back({"@counter", 7});
    module.functions.push_back({
        "id",
        toyc::codegen::contract::Type::Int,
        {{"x", "%p0"}},
        {{
            "entry",
            {},
            toyc::codegen::contract::ReturnInst{"%p0"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("counter:\n    .word 7\n") != std::string::npos,
            "global variable emission");
    require(assembly.find("    sw a0, -12(s0)\n") != std::string::npos,
            "first parameter lands into stack slot");
    require(assembly.find("    lw a0, -12(s0)\n") != std::string::npos,
            "return reloads parameter stack slot");
}

void testNinthParameterLanding() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "pick9",
        toyc::codegen::contract::Type::Int,
        {{"p0", "%p0"},
         {"p1", "%p1"},
         {"p2", "%p2"},
         {"p3", "%p3"},
         {"p4", "%p4"},
         {"p5", "%p5"},
         {"p6", "%p6"},
         {"p7", "%p7"},
         {"p8", "%p8"}},
        {{
            "entry",
            {},
            toyc::codegen::contract::ReturnInst{"%p8"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    lw t0, 0(s0)\n") != std::string::npos,
            "ninth parameter is read from incoming stack area");
    require(assembly.find("    sw t0, -44(s0)\n") != std::string::npos,
            "ninth parameter lands into managed vreg stack slot");
    require(assembly.find("    lw a0, -44(s0)\n") != std::string::npos,
            "return reloads ninth parameter stack slot");
}

void testArithmeticInstructionSelection() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "arith",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%x", 10},
                toyc::codegen::contract::ConstInst{"%y", 3},
                toyc::codegen::contract::AddInst{"%add", "%x", "%y"},
                toyc::codegen::contract::SubInst{"%sub", "%x", "%y"},
                toyc::codegen::contract::MulInst{"%mul", "%add", "%sub"},
                toyc::codegen::contract::DivInst{"%div", "%mul", "%y"},
                toyc::codegen::contract::ModInst{"%mod", "%div", "%y"},
                toyc::codegen::contract::NegInst{"%neg", "%mod"},
            },
            toyc::codegen::contract::ReturnInst{"%neg"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    add t0, t0, t1\n") != std::string::npos, "ADD lowers to add");
    require(assembly.find("    sub t0, t0, t1\n") != std::string::npos, "SUB lowers to sub");
    require(assembly.find("    mul t0, t0, t1\n") != std::string::npos, "MUL lowers to mul");
    require(assembly.find("    div t0, t0, t1\n") != std::string::npos, "DIV lowers to signed div");
    require(assembly.find("    rem t0, t0, t1\n") != std::string::npos, "MOD lowers to signed rem");
    require(assembly.find("    sub t0, zero, t0\n") != std::string::npos, "NEG lowers to zero-sub");
}

void testCompareAndLogicalInstructionSelection() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "cmp",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%x", 4},
                toyc::codegen::contract::ConstInst{"%y", 4},
                toyc::codegen::contract::EqInst{"%eq", "%x", "%y"},
                toyc::codegen::contract::NeInst{"%ne", "%x", "%y"},
                toyc::codegen::contract::LtInst{"%lt", "%x", "%y"},
                toyc::codegen::contract::LeInst{"%le", "%x", "%y"},
                toyc::codegen::contract::GtInst{"%gt", "%x", "%y"},
                toyc::codegen::contract::GeInst{"%ge", "%x", "%y"},
                toyc::codegen::contract::LNotInst{"%not", "%ne"},
            },
            toyc::codegen::contract::ReturnInst{"%not"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    seqz t0, t0\n") != std::string::npos,
            "EQ or LNOT normalizes through seqz");
    require(assembly.find("    snez t0, t0\n") != std::string::npos,
            "NE normalizes through snez");
    require(assembly.find("    slt t0, t0, t1\n") != std::string::npos,
            "LT or GE uses signed slt");
    require(assembly.find("    slt t0, t1, t0\n") != std::string::npos,
            "LE or GT uses reversed signed slt");
    require(assembly.find("    xori t0, t0, 1\n") != std::string::npos,
            "LE or GE inverts normalized slt result");
}

void testBranchJumpAndBlockLabels() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%cond", 1},
                },
                toyc::codegen::contract::BranchInst{"%cond", "if_then_0", "if_else_0"},
            },
            {
                "if_then_0",
                {
                    toyc::codegen::contract::ConstInst{"%result", 7},
                },
                toyc::codegen::contract::JumpInst{"if_end_0"},
            },
            {
                "if_else_0",
                {
                    toyc::codegen::contract::ConstInst{"%result", 9},
                },
                toyc::codegen::contract::JumpInst{"if_end_0"},
            },
            {
                "if_end_0",
                {},
                toyc::codegen::contract::ReturnInst{"%result"},
            },
        },
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("main__if_then_0:\n") != std::string::npos,
            "non-entry then block label is function-prefixed");
    require(assembly.find("main__if_else_0:\n") != std::string::npos,
            "non-entry else block label is function-prefixed");
    require(assembly.find("main__if_end_0:\n") != std::string::npos,
            "non-entry end block label is function-prefixed");
    require(assembly.find("    bnez t0, main__if_then_0\n") != std::string::npos,
            "BRANCH jumps to true label when condition is nonzero");
    require(assembly.find("    j main__if_else_0\n") != std::string::npos,
            "BRANCH falls through via explicit jump to false label");
    require(assembly.find("    j main__if_end_0\n") != std::string::npos,
            "JUMP lowers to prefixed target label");
    require(assembly.find("    lw a0, -16(s0)\n") != std::string::npos,
            "return reloads non-SSA result vreg after both branches");
}

void testGlobalLoadAndStoreInstructionSelection() {
    toyc::codegen::contract::IRModule module;
    module.globalVars.push_back({"@counter", 7});
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%value", 12},
                toyc::codegen::contract::StoreGlobalInst{"@counter", "%value"},
                toyc::codegen::contract::LoadGlobalInst{"%loaded", "@counter"},
            },
            toyc::codegen::contract::ReturnInst{"%loaded"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("counter:\n    .word 7\n") != std::string::npos,
            "global variable is emitted into data section");
    require(assembly.find("    lw t0, -12(s0)\n") != std::string::npos,
            "STORE_GLOBAL loads source vreg from stack slot");
    require(assembly.find("    la t1, counter\n") != std::string::npos,
            "STORE_GLOBAL materializes global label address");
    require(assembly.find("    sw t0, 0(t1)\n") != std::string::npos,
            "STORE_GLOBAL writes source value into global object");
    require(assembly.find("    la t0, counter\n") != std::string::npos,
            "LOAD_GLOBAL materializes global label address");
    require(assembly.find("    lw t1, 0(t0)\n") != std::string::npos,
            "LOAD_GLOBAL reads value from global object");
    require(assembly.find("    sw t1, -16(s0)\n") != std::string::npos,
            "LOAD_GLOBAL stores value into destination vreg slot");
    require(assembly.find("    lw a0, -16(s0)\n") != std::string::npos,
            "return reloads value loaded from global object");
}

void testCallInstructionSelection() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%x", 2},
                toyc::codegen::contract::ConstInst{"%y", 5},
                toyc::codegen::contract::CallInst{"%sum", "add", {"%x", "%y"}},
            },
            toyc::codegen::contract::ReturnInst{"%sum"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    lw a0, -12(s0)\n") != std::string::npos,
            "CALL loads first argument into a0");
    require(assembly.find("    lw a1, -16(s0)\n") != std::string::npos,
            "CALL loads second argument into a1");
    require(assembly.find("    call add\n") != std::string::npos,
            "CALL emits call pseudo-instruction");
    require(assembly.find("    sw a0, -20(s0)\n") != std::string::npos,
            "CALL stores return value into destination vreg slot");
    require(assembly.find("    lw a0, -20(s0)\n") != std::string::npos,
            "return reloads call destination vreg");
}

void testCallVoidInstructionSelection() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%x", 2},
                toyc::codegen::contract::CallVoidInst{"side_effect", {"%x"}},
            },
            toyc::codegen::contract::ReturnInst{"%x"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    lw a0, -12(s0)\n") != std::string::npos,
            "CALL_VOID loads first argument into a0");
    require(assembly.find("    call side_effect\n") != std::string::npos,
            "CALL_VOID emits call pseudo-instruction");
    require(assembly.find("    sw a0") == std::string::npos,
            "CALL_VOID does not store a0 as a return value");
}

void testCallWithStackArguments() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%a0", 0},
                toyc::codegen::contract::ConstInst{"%a1", 1},
                toyc::codegen::contract::ConstInst{"%a2", 2},
                toyc::codegen::contract::ConstInst{"%a3", 3},
                toyc::codegen::contract::ConstInst{"%a4", 4},
                toyc::codegen::contract::ConstInst{"%a5", 5},
                toyc::codegen::contract::ConstInst{"%a6", 6},
                toyc::codegen::contract::ConstInst{"%a7", 7},
                toyc::codegen::contract::ConstInst{"%a8", 8},
                toyc::codegen::contract::ConstInst{"%a9", 9},
                toyc::codegen::contract::CallInst{
                    "%ret",
                    "sum10",
                    {"%a0", "%a1", "%a2", "%a3", "%a4",
                     "%a5", "%a6", "%a7", "%a8", "%a9"},
                },
            },
            toyc::codegen::contract::ReturnInst{"%ret"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    addi sp, sp, -16\n") != std::string::npos,
            "CALL reserves aligned outgoing stack argument area");
    require(assembly.find("    lw a7, -40(s0)\n") != std::string::npos,
            "CALL loads eighth argument into a7");
    require(assembly.find("    lw t0, -44(s0)\n") != std::string::npos,
            "CALL loads ninth argument from vreg slot");
    require(assembly.find("    sw t0, 0(sp)\n") != std::string::npos,
            "CALL writes ninth argument to 0(sp)");
    require(assembly.find("    lw t0, -48(s0)\n") != std::string::npos,
            "CALL loads tenth argument from vreg slot");
    require(assembly.find("    sw t0, 4(sp)\n") != std::string::npos,
            "CALL writes tenth argument to 4(sp)");
    require(assembly.find("    call sum10\n") != std::string::npos,
            "CALL emits target function call");
    require(assembly.find("    addi sp, sp, 16\n") != std::string::npos,
            "CALL recovers outgoing stack argument area");
    require(assembly.find("    sw a0, -52(s0)\n") != std::string::npos,
            "CALL stores return value after stack argument cleanup");
}

void testRecursiveCallUsesNormalCallPathAndSharedEpilogue() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "fact",
        toyc::codegen::contract::Type::Int,
        {{"n", "%n"}},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%one", 1},
                    toyc::codegen::contract::LeInst{"%is_base", "%n", "%one"},
                },
                toyc::codegen::contract::BranchInst{"%is_base", "base_0", "recur_0"},
            },
            {
                "base_0",
                {},
                toyc::codegen::contract::ReturnInst{"%one"},
            },
            {
                "recur_0",
                {
                    toyc::codegen::contract::SubInst{"%next", "%n", "%one"},
                    toyc::codegen::contract::CallInst{"%partial", "fact", {"%next"}},
                    toyc::codegen::contract::MulInst{"%result", "%n", "%partial"},
                },
                toyc::codegen::contract::ReturnInst{"%result"},
            },
        },
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    sw ra, ") != std::string::npos,
            "recursive-capable function saves return address in prologue");
    require(assembly.find("    call fact\n") != std::string::npos,
            "recursive call uses normal CALL lowering path");
    require(assembly.find("fact__base_0:\n") != std::string::npos,
            "recursive base block uses function-prefixed label");
    require(assembly.find("fact__recur_0:\n") != std::string::npos,
            "recursive step block uses function-prefixed label");
    require(assembly.find(".Lfact__epilogue:\n") != std::string::npos,
            "function emits a shared epilogue label");
    require(assembly.find("    j .Lfact__epilogue\n") != std::string::npos,
            "RETURN jumps to shared epilogue after preparing a0");
}

void testShortCircuitLogicalAndPattern() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%a", 1},
                    toyc::codegen::contract::ConstInst{"%b", 0},
                },
                toyc::codegen::contract::BranchInst{"%a", "and_rhs_0", "and_false_0"},
            },
            {
                "and_rhs_0",
                {},
                toyc::codegen::contract::BranchInst{"%b", "and_true_0", "and_false_0"},
            },
            {
                "and_true_0",
                {
                    toyc::codegen::contract::ConstInst{"%result_0", 1},
                },
                toyc::codegen::contract::JumpInst{"and_end_0"},
            },
            {
                "and_false_0",
                {
                    toyc::codegen::contract::ConstInst{"%result_0", 0},
                },
                toyc::codegen::contract::JumpInst{"and_end_0"},
            },
            {
                "and_end_0",
                {},
                toyc::codegen::contract::ReturnInst{"%result_0"},
            },
        },
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    bnez t0, main__and_rhs_0\n") != std::string::npos,
            "logical-and entry branches to rhs when lhs is nonzero");
    require(assembly.find("main__and_rhs_0:\n") != std::string::npos,
            "logical-and rhs block uses function-prefixed label");
    require(assembly.find("    bnez t0, main__and_true_0\n") != std::string::npos,
            "logical-and rhs branches to true block when rhs is nonzero");
    require(assembly.find("main__and_false_0:\n") != std::string::npos,
            "logical-and false block uses function-prefixed label");
    require(assembly.find("    li t0, 1\n") != std::string::npos,
            "logical-and true path materializes result 1");
    require(assembly.find("    li t0, 0\n") != std::string::npos,
            "logical-and false path materializes result 0");
    require(assembly.find("main__and_end_0:\n") != std::string::npos,
            "logical-and merge block uses function-prefixed label");
}

void testShortCircuitLogicalOrPattern() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%a", 0},
                    toyc::codegen::contract::ConstInst{"%b", 1},
                },
                toyc::codegen::contract::BranchInst{"%a", "or_true_0", "or_rhs_0"},
            },
            {
                "or_rhs_0",
                {},
                toyc::codegen::contract::BranchInst{"%b", "or_true_0", "or_false_0"},
            },
            {
                "or_true_0",
                {
                    toyc::codegen::contract::ConstInst{"%result_0", 1},
                },
                toyc::codegen::contract::JumpInst{"or_end_0"},
            },
            {
                "or_false_0",
                {
                    toyc::codegen::contract::ConstInst{"%result_0", 0},
                },
                toyc::codegen::contract::JumpInst{"or_end_0"},
            },
            {
                "or_end_0",
                {},
                toyc::codegen::contract::ReturnInst{"%result_0"},
            },
        },
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("    bnez t0, main__or_true_0\n") != std::string::npos,
            "logical-or entry branches to true block when lhs is nonzero");
    require(assembly.find("main__or_rhs_0:\n") != std::string::npos,
            "logical-or rhs block uses function-prefixed label");
    require(assembly.find("    bnez t0, main__or_true_0\n") != std::string::npos,
            "logical-or rhs branches to true block when rhs is nonzero");
    require(assembly.find("main__or_false_0:\n") != std::string::npos,
            "logical-or false block uses function-prefixed label");
    require(assembly.find("main__or_end_0:\n") != std::string::npos,
            "logical-or merge block uses function-prefixed label");
}

} // namespace

int main() {
    try {
        testConstCopyReturnMain();
        testGlobalsAndParameterLanding();
        testNinthParameterLanding();
        testArithmeticInstructionSelection();
        testCompareAndLogicalInstructionSelection();
        testBranchJumpAndBlockLabels();
        testGlobalLoadAndStoreInstructionSelection();
        testCallInstructionSelection();
        testCallVoidInstructionSelection();
        testCallWithStackArguments();
        testRecursiveCallUsesNormalCallPathAndSharedEpilogue();
        testShortCircuitLogicalAndPattern();
        testShortCircuitLogicalOrPattern();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
