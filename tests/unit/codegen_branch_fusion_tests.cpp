#include "codegen/RiscvBackend.h"
#include "codegen/frame/RegisterAllocator.h"
#include "codegen/lower/BranchFusionAnalysis.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen branch fusion test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

toyc::codegen::contract::IRModule compareAfterSetupModule() {
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
                    toyc::codegen::contract::ConstInst{"%b", 2},
                    toyc::codegen::contract::LtInst{"%cond", "%a", "%b"},
                },
                toyc::codegen::contract::BranchInst{"%cond", "then_0", "else_0"},
            },
            {
                "then_0",
                {toyc::codegen::contract::ConstInst{"%ret", 11}},
                toyc::codegen::contract::ReturnInst{"%ret"},
            },
            {
                "else_0",
                {toyc::codegen::contract::ConstInst{"%ret", 22}},
                toyc::codegen::contract::ReturnInst{"%ret"},
            },
        },
    });
    return module;
}

void testFusionRunsAfterEarlierBlockInstructions() {
    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly =
        toyc::codegen::RiscvBackend().generate(compareAfterSetupModule(), options);

    const std::size_t firstConst = [&]() -> std::size_t {
        const std::size_t addiPos = assembly.find("    addi t0, zero, 1\n");
        if (addiPos != std::string::npos) {
            return addiPos;
        }
        const std::size_t physicalAddiPos = assembly.find(", zero, 1\n");
        if (physicalAddiPos != std::string::npos) {
            return physicalAddiPos;
        }
        return assembly.find("    li t0, 1\n");
    }();
    const std::size_t fusedCompare = assembly.find("    slt t0, t0, t1\n    bnez t0, main__then_0\n");
    require(firstConst != std::string::npos, "const materialization is emitted");
    require(fusedCompare != std::string::npos, "fused compare-branch is emitted");
    require(firstConst < fusedCompare,
            "fused compare must run after earlier block instructions");
}

void testFusionSkippedWhenCondUsedInTargetBlock() {
    const toyc::codegen::contract::IRModule module = compareAfterSetupModule();
    const auto& function = module.functions.front();
    const auto& entry = function.basicBlocks.front();
    const auto& branch = std::get<toyc::codegen::contract::BranchInst>(entry.terminator);

    toyc::codegen::contract::IRModule liveCondModule = module;
    auto& thenBlock = liveCondModule.functions.front().basicBlocks[1];
    thenBlock.instructions.clear();
    thenBlock.terminator = toyc::codegen::contract::ReturnInst{"%cond"};

    require(toyc::codegen::isBranchCondUsedInTargets(
                liveCondModule.functions.front(), branch, "entry"),
            "analysis detects cond use in target block");

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly =
        toyc::codegen::RiscvBackend().generate(liveCondModule, options);
    require(assembly.find("    slt t0, t0, t1\n    bnez t0, main__then_0\n") == std::string::npos,
            "fusion is disabled when branch cond is read by a successor block");
    require(assembly.find("    slt ") != std::string::npos,
            "compare is still materialized without fusion");
}

void testFusionSkippedWhenCondUsedInReachableJoinBlock() {
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
                    toyc::codegen::contract::ConstInst{"%b", 2},
                    toyc::codegen::contract::LtInst{"%cond", "%a", "%b"},
                },
                toyc::codegen::contract::BranchInst{"%cond", "then_0", "else_0"},
            },
            {
                "then_0",
                {toyc::codegen::contract::ConstInst{"%x", 11}},
                toyc::codegen::contract::JumpInst{"join_0"},
            },
            {
                "else_0",
                {toyc::codegen::contract::ConstInst{"%x", 22}},
                toyc::codegen::contract::JumpInst{"join_0"},
            },
            {
                "join_0",
                {
                    toyc::codegen::contract::AddInst{"%ret", "%cond", "%x"},
                },
                toyc::codegen::contract::ReturnInst{"%ret"},
            },
        },
    });

    const auto& function = module.functions.front();
    const auto& branch =
        std::get<toyc::codegen::contract::BranchInst>(function.basicBlocks.front().terminator);
    require(toyc::codegen::isBranchCondUsedInTargets(function, branch, "entry"),
            "analysis detects cond use in a reachable join block");

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);
    require(assembly.find("    slt t0, t0, t1\n    bnez t0, main__then_0\n") ==
                std::string::npos,
            "fusion is disabled when branch cond is read after target successors");
    require(assembly.find("main__join_0:\n") != std::string::npos,
            "join block is emitted");
    require(assembly.find("    slt ") != std::string::npos,
            "compare result is materialized for the later join block read");
}

void testFusionAllowedWhenSuccessorOnlyReachesSourceBlockRecompute() {
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
                    toyc::codegen::contract::ConstInst{"%b", 2},
                    toyc::codegen::contract::LtInst{"%cond", "%a", "%b"},
                },
                toyc::codegen::contract::BranchInst{"%cond", "loop_body", "loop_exit"},
            },
            {
                "loop_body",
                {toyc::codegen::contract::ConstInst{"%a", 3}},
                toyc::codegen::contract::JumpInst{"entry"},
            },
            {
                "loop_exit",
                {toyc::codegen::contract::ConstInst{"%ret", 0}},
                toyc::codegen::contract::ReturnInst{"%ret"},
            },
        },
    });

    const auto& function = module.functions.front();
    const auto& branch =
        std::get<toyc::codegen::contract::BranchInst>(function.basicBlocks.front().terminator);
    require(!toyc::codegen::isBranchCondUsedInTargets(function, branch, "entry"),
            "analysis ignores source block recomputation reached through a loop backedge");

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);
    require(assembly.find("    slt t0, t0, t1\n    bnez t0, main__loop_body\n") !=
                std::string::npos,
            "fusion remains enabled when no reachable successor reads the old cond value");
}

} // namespace

int main() {
    try {
        testFusionRunsAfterEarlierBlockInstructions();
        testFusionSkippedWhenCondUsedInTargetBlock();
        testFusionSkippedWhenCondUsedInReachableJoinBlock();
        testFusionAllowedWhenSuccessorOnlyReachesSourceBlockRecompute();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
