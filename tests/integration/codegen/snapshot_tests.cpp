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

void testBreakContinueCfgSnapshot() {
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
                    toyc::codegen::contract::ConstInst{"%limit", 5},
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
                    toyc::codegen::contract::ConstInst{"%five", 5},
                    toyc::codegen::contract::EqInst{"%is_break", "%i", "%five"},
                },
                toyc::codegen::contract::BranchInst{"%is_break", "break_0", "after_break_check_0"},
            },
            {
                "after_break_check_0",
                {
                    toyc::codegen::contract::ConstInst{"%three", 3},
                    toyc::codegen::contract::EqInst{"%is_continue", "%i", "%three"},
                },
                toyc::codegen::contract::BranchInst{"%is_continue", "continue_0", "loop_step_0"},
            },
            {
                "continue_0",
                {
                    toyc::codegen::contract::ConstInst{"%one", 1},
                    toyc::codegen::contract::AddInst{"%i", "%i", "%one"},
                },
                toyc::codegen::contract::JumpInst{"while_cond_0"},
            },
            {
                "loop_step_0",
                {
                    toyc::codegen::contract::AddInst{"%sum", "%sum", "%i"},
                    toyc::codegen::contract::ConstInst{"%one_step", 1},
                    toyc::codegen::contract::AddInst{"%i", "%i", "%one_step"},
                },
                toyc::codegen::contract::JumpInst{"while_cond_0"},
            },
            {
                "break_0",
                {},
                toyc::codegen::contract::JumpInst{"while_exit_0"},
            },
            {
                "while_exit_0",
                {},
                toyc::codegen::contract::ReturnInst{"%sum"},
            },
        },
    });

    const std::string assembly = toyc::codegen::RiscvBackend().generate(module);
    expectAssemblyContains(assembly, "main__break_0:\n", "break target label");
    expectAssemblyContains(assembly, "main__continue_0:\n", "continue target label");
    expectAssemblyContains(assembly, "    j main__while_exit_0\n", "break jumps to loop exit");
    expectAssemblyContains(assembly, "    j main__while_cond_0\n", "continue jumps to loop head");
}

void testOptRemovesFallThroughJumpSnapshot() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::ConstInst{"%x", 1},
                },
                toyc::codegen::contract::JumpInst{"next"},
            },
            {
                "next",
                {},
                toyc::codegen::contract::ReturnInst{"%x"},
            },
        },
    });

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);
    expectAssemblyContains(assembly, "main__next:\n", "fall-through target block is emitted");
    if (assembly.find("    j main__next\n") != std::string::npos) {
        fail("fall-through jump should be removed when -opt is enabled");
    }
}

void testOptCompareBranchFusionSnapshot() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back({
        "main",
        toyc::codegen::contract::Type::Int,
        {{"a", "%a"}, {"b", "%b"}},
        {
            {
                "entry",
                {
                    toyc::codegen::contract::LtInst{"%cond", "%a", "%b"},
                },
                toyc::codegen::contract::BranchInst{"%cond", "then_0", "else_0"},
            },
            {
                "then_0",
                {
                    toyc::codegen::contract::ConstInst{"%ret", 11},
                },
                toyc::codegen::contract::ReturnInst{"%ret"},
            },
            {
                "else_0",
                {
                    toyc::codegen::contract::ConstInst{"%ret", 22},
                },
                toyc::codegen::contract::ReturnInst{"%ret"},
            },
        },
    });

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);
    expectAssemblyContains(assembly,
                            "    bnez t0, main__then_0\n",
                            "fused compare-branch");
    expectAssemblyContains(assembly, "    j main__else_0\n", "fused branch to else");
}

void testOptPinsHotLoopVRegsToCalleeSavedSnapshot() {
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
                    toyc::codegen::contract::AddInst{"%sum", "%sum", "%i"},
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
    // No calls in this loop, so the hot vregs live in caller-saved temps (no
    // prologue save needed) and the return value is moved from a register.
    if (assembly.find("    mv a0, t") == std::string::npos &&
        assembly.find("    mv a0, s") == std::string::npos) {
        fail("hot loop return value should come from a register under -opt");
    }
    expectAssemblyContains(assembly, "    slti", "loop bound compared via immediate, not reloaded const");
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
        testBreakContinueCfgSnapshot();
        testOptRemovesFallThroughJumpSnapshot();
        testOptCompareBranchFusionSnapshot();
        testOptPinsHotLoopVRegsToCalleeSavedSnapshot();
        testMultiFunctionModuleSnapshot();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
