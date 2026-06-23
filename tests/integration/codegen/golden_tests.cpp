#include "codegen/RiscvBackend.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen golden test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void expectExactAssembly(const std::string& actual,
                         const std::string& expected,
                         std::string_view message) {
    if (actual != expected) {
        std::cerr << message << '\n';
        std::cerr << "actual assembly:\n" << actual << "\nexpected assembly:\n" << expected
                  << '\n';
        fail("golden assembly mismatch");
    }
}

std::size_t countSubstring(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

toyc::codegen::contract::IRModule constCopyReturnModule() {
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
    return module;
}

void testGoldenConstCopyReturnDefaultPath() {
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

    const std::string assembly = toyc::codegen::RiscvBackend().generate(constCopyReturnModule());
    expectExactAssembly(assembly, expected, "default-path const/copy/return golden");
}

void testGoldenSharedEpilogueAndDataSection() {
    toyc::codegen::contract::IRModule module;
    module.globalVars.push_back({"@g", 3});
    module.functions.push_back({
        "helper",
        toyc::codegen::contract::Type::Int,
        {{"v", "%p0"}},
        {{
            "entry",
            {},
            toyc::codegen::contract::ReturnInst{"%p0"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    require(assembly.find("g:\n    .word 3\n") != std::string::npos, "global data section");
    require(assembly.find(".Lhelper__epilogue:\n") != std::string::npos, "per-function epilogue");
    require(assembly.find("    sw a0, -12(s0)\n") != std::string::npos,
            "parameter landing from a0");
}

void testOptPreservesControlFlowSkeleton() {
    const toyc::codegen::contract::IRModule module = constCopyReturnModule();
    const std::string baseline = toyc::codegen::RiscvBackend().generate(module);

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string optimized = toyc::codegen::RiscvBackend().generate(module, options);

    for (const std::string_view label :
         {"main:\n", ".Lmain__epilogue:\n", "    .global main\n", "    ret\n"}) {
        require(baseline.find(label) != std::string::npos, "baseline skeleton label");
        require(optimized.find(label) != std::string::npos, "optimized skeleton label");
    }
}

void testOptReducesLoadsForSelfAdd() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%a", 5},
                toyc::codegen::contract::AddInst{"%b", "%a", "%a"},
            },
            toyc::codegen::contract::ReturnInst{"%b"},
        }},
    });

    const std::string baseline = toyc::codegen::RiscvBackend().generate(module);
    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string optimized = toyc::codegen::RiscvBackend().generate(module, options);

    require(countSubstring(optimized, "    lw ") < countSubstring(baseline, "    lw "),
            "-opt should reduce stack loads for duplicate operands");
}

void testOptPhysicalCopyUsesRegisterMove() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "sum_loop",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%i", 0},
                    toyc::codegen::contract::ConstInst{"%sum", 0},
                    toyc::codegen::contract::ConstInst{"%limit", 4},
                },
                toyc::codegen::contract::JumpInst{"while_cond"},
            },
            {
                "while_cond",
                {toyc::codegen::contract::LtInst{"%cond", "%i", "%limit"}},
                toyc::codegen::contract::BranchInst{"%cond", "while_body", "while_exit"},
            },
            {
                "while_body",
                {
                    toyc::codegen::contract::CopyInst{"%sum", "%i"},
                    toyc::codegen::contract::ConstInst{"%one", 1},
                    toyc::codegen::contract::AddInst{"%i", "%i", "%one"},
                },
                toyc::codegen::contract::JumpInst{"while_cond"},
            },
            {
                "while_exit",
                {},
                toyc::codegen::contract::ReturnInst{"%sum"},
            },
        },
    });

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);

    const std::size_t bodyStart = assembly.find("sum_loop__while_body:");
    require(bodyStart != std::string::npos, "while body label");
    const std::string body = assembly.substr(bodyStart);
    require(body.find("    mv s") != std::string::npos,
            "hot vreg copy should move between callee-saved registers under -opt");
    require(body.find("    lw t0, -") == std::string::npos ||
                body.find("    mv s") < body.find("    lw t0, -"),
            "physical copy should not reload from stack before mv between s-regs");
}

} // namespace

int main() {
    try {
        testGoldenConstCopyReturnDefaultPath();
        testGoldenSharedEpilogueAndDataSection();
        testOptPreservesControlFlowSkeleton();
        testOptReducesLoadsForSelfAdd();
        testOptPhysicalCopyUsesRegisterMove();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
