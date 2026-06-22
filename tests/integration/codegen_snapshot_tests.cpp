#include "codegen/RiscvBackend.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen integration snapshot failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void expectAssemblyContains(const std::string& assembly,
                            std::string_view fragment,
                            std::string_view message) {
    if (assembly.find(fragment) == std::string::npos) {
        std::cerr << "assembly snapshot:\n" << assembly << "\nmissing fragment: \""
                  << fragment << "\"\n";
        fail(message);
    }
}

void testWhileLoopCfgSnapshot() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%i", 0},
                    toyc::codegen::contract::ConstInst{"%sum", 0},
                    toyc::codegen::contract::ConstInst{"%limit", 3},
                },
                toyc::codegen::contract::JumpInst{"while_cond_0"},
            },
            {
                "while_cond_0",
                {
                    toyc::codegen::contract::LtInst{"%cond", "%i", "%limit"},
                },
                toyc::codegen::contract::BranchInst{"%cond", "while_body_0", "while_exit_0"},
            },
            {
                "while_body_0",
                {
                    toyc::codegen::contract::AddInst{"%sum", "%sum", "%i"},
                    toyc::codegen::contract::ConstInst{"%one", 1},
                    toyc::codegen::contract::AddInst{"%i", "%i", "%one"},
                },
                toyc::codegen::contract::JumpInst{"while_cond_0"},
            },
            {
                "while_exit_0",
                {},
                toyc::codegen::contract::ReturnInst{"%sum"},
            },
        },
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    expectAssemblyContains(assembly, "main__while_cond_0:\n", "while condition label");
    expectAssemblyContains(assembly, "main__while_body_0:\n", "while body label");
    expectAssemblyContains(assembly, "main__while_exit_0:\n", "while exit label");
    expectAssemblyContains(assembly, "    j main__while_cond_0\n", "while back edge");
    expectAssemblyContains(assembly, "    bnez t0, main__while_body_0\n", "while branch to body");
    expectAssemblyContains(assembly, "    j main__while_exit_0\n", "while branch to exit");
}

void testNestedCallSnapshot() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::ConstInst{"%x", 2},
                toyc::codegen::contract::ConstInst{"%y", 3},
                toyc::codegen::contract::CallInst{"%a", "inc", {"%x"}},
                toyc::codegen::contract::CallInst{"%b", "add", {"%a", "%y"}},
            },
            toyc::codegen::contract::ReturnInst{"%b"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    const std::size_t firstCall = assembly.find("    call inc\n");
    const std::size_t secondCall = assembly.find("    call add\n");
    require(firstCall != std::string::npos, "first nested call is emitted");
    require(secondCall != std::string::npos, "second nested call is emitted");
    require(firstCall < secondCall, "nested calls preserve program order");
    expectAssemblyContains(assembly, "    sw a0, ", "first call result is stored before second call");
}

void testShadowScopeDistinctVRegsSnapshot() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%x_0", 1},
                },
                toyc::codegen::contract::JumpInst{"block_1"},
            },
            {
                "block_1",
                {
                    toyc::codegen::contract::ConstInst{"%x_1", 2},
                    toyc::codegen::contract::AddInst{"%sum", "%x_0", "%x_1"},
                },
                toyc::codegen::contract::ReturnInst{"%sum"},
            },
        },
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    expectAssemblyContains(assembly, "    li t0, 1\n", "outer scoped vreg constant");
    expectAssemblyContains(assembly, "    li t0, 2\n", "inner scoped vreg constant");
    expectAssemblyContains(assembly, "    add t0, t0, t1\n", "shadowed names are independent slots");
}

void testStackPressureSnapshot() {
    toyc::codegen::contract::IRModule module;
    toyc::codegen::contract::IRFunction function{
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {},
            toyc::codegen::contract::ReturnInst{"%v19"},
        }},
    };

    std::vector<toyc::codegen::contract::Instruction>& instructions =
        function.basicBlocks[0].instructions;
    for (int i = 0; i < 20; ++i) {
        instructions.push_back(
            toyc::codegen::contract::ConstInst{"%v" + std::to_string(i), i});
    }
    module.functions.push_back(std::move(function));

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    expectAssemblyContains(assembly, "    addi sp, sp, -96\n", "twenty vregs expand frame size");
    expectAssemblyContains(assembly, "    lw a0, -88(s0)\n", "last vreg slot is returned");
}

void testMultiFunctionModuleSnapshot() {
    toyc::codegen::contract::IRModule module;
    module.globalVars.push_back({"@g", 5});
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
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            {
                toyc::codegen::contract::LoadGlobalInst{"%loaded", "@g"},
                toyc::codegen::contract::CallInst{"%ret", "helper", {"%loaded"}},
            },
            toyc::codegen::contract::ReturnInst{"%ret"},
        }},
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    expectAssemblyContains(assembly, "g:\n    .word 5\n", "module-level global data");
    expectAssemblyContains(assembly, "helper:\n", "first function symbol");
    expectAssemblyContains(assembly, ".Lhelper__epilogue:\n", "first function epilogue");
    expectAssemblyContains(assembly, "main:\n", "second function symbol");
    expectAssemblyContains(assembly, "    call helper\n", "cross-function call");
}

} // namespace

int main() {
    try {
        testWhileLoopCfgSnapshot();
        testNestedCallSnapshot();
        testShadowScopeDistinctVRegsSnapshot();
        testStackPressureSnapshot();
        testMultiFunctionModuleSnapshot();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
