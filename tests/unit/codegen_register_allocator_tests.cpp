#include "codegen/frame/RegisterAllocator.h"
#include "codegen/frame/StackFrame.h"
#include "codegen/frame/VRegAnalysis.h"
#include "codegen/RiscvBackend.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void fail(std::string_view message) {
    std::cerr << "codegen register allocator test failure: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

toyc::codegen::contract::IRFunction hotLoopFunction() {
    return {
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
    };
}

const toyc::codegen::LiveInterval& findInterval(const toyc::codegen::VRegAnalysis& analysis,
                                                std::string_view vreg) {
    const auto it = std::find_if(analysis.liveIntervals.begin(),
                                 analysis.liveIntervals.end(),
                                 [&](const toyc::codegen::LiveInterval& interval) {
                                     return interval.vreg == vreg;
                                 });
    if (it == analysis.liveIntervals.end()) {
        fail("missing live interval");
    }
    return *it;
}

toyc::codegen::contract::IRFunction pressureFunction() {
    std::vector<toyc::codegen::contract::Instruction> instructions;
    for (int i = 0; i < 12; ++i) {
        instructions.push_back(toyc::codegen::contract::ConstInst{"%v" + std::to_string(i), i});
    }
    instructions.push_back(toyc::codegen::contract::AddInst{"%acc", "%v0", "%v1"});
    for (int i = 2; i < 12; ++i) {
        instructions.push_back(
            toyc::codegen::contract::AddInst{"%acc", "%acc", "%v" + std::to_string(i)});
    }

    return {
        "pressure",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            instructions,
            toyc::codegen::contract::ReturnInst{"%acc"},
        }},
    };
}

toyc::codegen::contract::IRFunction spillPrefersShorterIntervalFunction() {
    std::vector<toyc::codegen::contract::Instruction> instructions;
    for (int i = 0; i < 11; ++i) {
        instructions.push_back(toyc::codegen::contract::ConstInst{"%long" + std::to_string(i), i});
    }
    instructions.push_back(toyc::codegen::contract::ConstInst{"%short", 7});
    instructions.push_back(toyc::codegen::contract::AddInst{"%tmp", "%short", "%long0"});
    instructions.push_back(toyc::codegen::contract::AddInst{"%acc", "%tmp", "%long1"});
    for (int i = 2; i < 11; ++i) {
        instructions.push_back(
            toyc::codegen::contract::AddInst{"%acc", "%acc", "%long" + std::to_string(i)});
    }

    return {
        "spill_short",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            instructions,
            toyc::codegen::contract::ReturnInst{"%acc"},
        }},
    };
}

toyc::codegen::contract::IRFunction loopWeightedPressureFunction() {
    std::vector<toyc::codegen::contract::Instruction> entryInstructions;
    for (int i = 0; i < 11; ++i) {
        entryInstructions.push_back(
            toyc::codegen::contract::ConstInst{"%cold" + std::to_string(i), i + 1});
    }
    entryInstructions.push_back(toyc::codegen::contract::ConstInst{"%hot", 0});

    std::vector<toyc::codegen::contract::Instruction> exitInstructions;
    exitInstructions.push_back(toyc::codegen::contract::AddInst{"%ret", "%hot", "%cold0"});
    for (int i = 1; i < 11; ++i) {
        exitInstructions.push_back(
            toyc::codegen::contract::AddInst{"%ret", "%ret", "%cold" + std::to_string(i)});
    }

    return {
        "loop_weighted_pressure",
        toyc::codegen::contract::Type::Int,
        {},
        {
            {
                "entry",
                entryInstructions,
                toyc::codegen::contract::JumpInst{"while_cond"},
            },
            {
                "while_cond",
                {toyc::codegen::contract::LtInst{"%cond", "%hot", "%cold10"}},
                toyc::codegen::contract::BranchInst{"%cond", "while_body", "while_exit"},
            },
            {
                "while_body",
                {toyc::codegen::contract::AddInst{"%hot", "%hot", "%cold0"}},
                toyc::codegen::contract::JumpInst{"while_cond"},
            },
            {
                "while_exit",
                exitInstructions,
                toyc::codegen::contract::ReturnInst{"%ret"},
            },
        },
    };
}

toyc::codegen::contract::IRFunction callCrossingPressureFunction() {
    std::vector<toyc::codegen::contract::Instruction> instructions;
    instructions.push_back(toyc::codegen::contract::ConstInst{"%keep", 42});

    std::vector<std::string> args;
    args.reserve(12);
    for (int i = 0; i < 12; ++i) {
        const std::string vreg = "%arg" + std::to_string(i);
        instructions.push_back(toyc::codegen::contract::ConstInst{vreg, i});
        args.push_back(vreg);
    }
    instructions.push_back(toyc::codegen::contract::CallVoidInst{"sink", args});

    return {
        "call_pressure",
        toyc::codegen::contract::Type::Int,
        {},
        {{
            "entry",
            instructions,
            toyc::codegen::contract::ReturnInst{"%keep"},
        }},
    };
}

void testAnalysisBuildsCfgLiveIntervals() {
    const toyc::codegen::VRegAnalysis analysis = toyc::codegen::analyzeVRegs(hotLoopFunction());

    require(analysis.liveOuts.at("entry").find("%i") != analysis.liveOuts.at("entry").end(),
            "loop counter should be live out of entry");
    require(analysis.liveIns.at("while_cond").find("%i") != analysis.liveIns.at("while_cond").end(),
            "loop counter should be live into condition block");
    require(analysis.liveOuts.at("while_body").find("%sum") !=
                analysis.liveOuts.at("while_body").end(),
            "accumulator should stay live across loop backedge");

    const toyc::codegen::LiveInterval& i = findInterval(analysis, "%i");
    const toyc::codegen::LiveInterval& sum = findInterval(analysis, "%sum");
    require(i.start < i.end, "loop counter interval should span multiple program points");
    require(sum.start < sum.end, "accumulator interval should span multiple program points");
}

void testAnalysisWeightsLoopBlocksHigherThanColdBlocks() {
    const toyc::codegen::VRegAnalysis analysis =
        toyc::codegen::analyzeVRegs(loopWeightedPressureFunction());

    require(analysis.blockLoopDepths.at("entry") == 0, "entry should not be loop weighted");
    require(analysis.blockLoopDepths.at("while_cond") > 0,
            "loop condition should receive loop depth");
    require(analysis.blockLoopDepths.at("while_body") > 0,
            "loop body should receive loop depth");
    require(analysis.accessWeights.at("%hot") > analysis.accessWeights.at("%cold1"),
            "loop-carried hot vreg should have higher spill weight than cold long vreg");
}

void testAnalysisWeightsCallCrossingIntervalsHigher() {
    const toyc::codegen::VRegAnalysis analysis =
        toyc::codegen::analyzeVRegs(callCrossingPressureFunction());

    const toyc::codegen::LiveInterval& keep = findInterval(analysis, "%keep");
    const toyc::codegen::LiveInterval& arg0 = findInterval(analysis, "%arg0");
    require(keep.callCrossingCount == 1, "vreg live across call should record call crossing");
    require(arg0.callCrossingCount == 0, "call-only argument should not cross the call");
    require(keep.spillWeight > arg0.spillWeight,
            "call-crossing vreg should receive higher spill weight than call-only argument");
}

void testOptAssignsLiveIntervalsToCalleeSaved() {
    const toyc::codegen::RegisterAllocation allocation =
        toyc::codegen::RegisterAllocator::allocate(hotLoopFunction(), true);

    const auto iReg = allocation.assignment.physicalReg("%i");
    const auto sumReg = allocation.assignment.physicalReg("%sum");
    require(iReg.has_value(), "loop counter should receive a callee-saved register under -opt");
    require(sumReg.has_value(), "accumulator should receive a callee-saved register under -opt");
    require(!allocation.frame.containsVReg("%i"), "hot vreg should not keep a stack slot");
    require(!allocation.frame.containsVReg("%sum"), "hot vreg should not keep a stack slot");

    bool sawS1 = false;
    for (const toyc::codegen::SavedRegisterSlot& slot : allocation.frame.savedRegisterSlots()) {
        if (slot.reg == "s1" || slot.reg == "s2") {
            sawS1 = true;
        }
    }
    require(sawS1, "frame prologue should save assigned callee-saved registers");
}

void testLinearScanKeepsCallCrossingIntervalUnderPressure() {
    const toyc::codegen::RegisterAllocation allocation =
        toyc::codegen::RegisterAllocator::allocate(callCrossingPressureFunction(), true);

    require(allocation.assignment.physicalReg("%keep").has_value(),
            "call-crossing vreg should receive a callee-saved register under pressure");

    int spilledArgCount = 0;
    for (int i = 0; i < 12; ++i) {
        const std::string vreg = "%arg" + std::to_string(i);
        if (!allocation.assignment.physicalReg(vreg).has_value() &&
            allocation.frame.containsVReg(vreg)) {
            ++spilledArgCount;
        }
    }
    require(spilledArgCount >= 1,
            "linear scan should spill a call-only argument before the call-crossing vreg");
}

void testLinearScanCostModelKeepsLoopHotInterval() {
    const toyc::codegen::RegisterAllocation allocation =
        toyc::codegen::RegisterAllocator::allocate(loopWeightedPressureFunction(), true);

    require(allocation.assignment.physicalReg("%hot").has_value(),
            "loop-weighted hot interval should receive a register under pressure");

    int spilledColdCount = 0;
    for (int i = 0; i < 11; ++i) {
        const std::string vreg = "%cold" + std::to_string(i);
        if (!allocation.assignment.physicalReg(vreg).has_value() &&
            allocation.frame.containsVReg(vreg)) {
            ++spilledColdCount;
        }
    }
    require(spilledColdCount >= 1,
            "cost model should spill at least one cold interval for the loop hot interval");
}

void testLinearScanSpillsWhenPressureExceedsRegisterPool() {
    const toyc::codegen::RegisterAllocation allocation =
        toyc::codegen::RegisterAllocator::allocate(pressureFunction(), true);

    int physicalInputCount = 0;
    int stackInputCount = 0;
    for (int i = 0; i < 12; ++i) {
        const std::string vreg = "%v" + std::to_string(i);
        if (allocation.assignment.physicalReg(vreg).has_value()) {
            ++physicalInputCount;
        }
        if (allocation.frame.containsVReg(vreg)) {
            ++stackInputCount;
        }
    }

    int assignedRegisterSlots = 0;
    for (const toyc::codegen::SavedRegisterSlot& slot : allocation.frame.savedRegisterSlots()) {
        if (slot.reg != "ra" && slot.reg != "s0") {
            ++assignedRegisterSlots;
        }
    }

    require(assignedRegisterSlots == 11,
            "linear scan should use the full s1-s11 register pool under pressure");
    require(allocation.assignment.physicalReg("%acc").has_value(),
            "cost model should keep high-access accumulator in a register");
    require(physicalInputCount < 12, "at least one overlapping input interval should spill");
    require(stackInputCount >= 1, "spilled overlapping intervals should keep stack slots");
}

void testLinearScanSpillsLongerActiveIntervalForShorterCurrent() {
    const toyc::codegen::RegisterAllocation allocation =
        toyc::codegen::RegisterAllocator::allocate(spillPrefersShorterIntervalFunction(), true);

    require(allocation.assignment.physicalReg("%short").has_value(),
            "short interval should receive a register under pressure");

    int spilledLongCount = 0;
    for (int i = 0; i < 11; ++i) {
        const std::string vreg = "%long" + std::to_string(i);
        if (!allocation.assignment.physicalReg(vreg).has_value() &&
            allocation.frame.containsVReg(vreg)) {
            ++spilledLongCount;
        }
    }
    require(spilledLongCount >= 1,
            "linear scan should evict a longer active interval for the shorter interval");
}

void testOptEmitsCalleeSavedInAssembly() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back(hotLoopFunction());

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);

    require(assembly.find("    sw s1") != std::string::npos,
            "generated assembly saves callee-saved s1 in prologue");
    require(assembly.find("    mv a0, s") != std::string::npos,
            "return path can move from callee-saved register");
}

void testNonOptKeepsStackSlotsOnly() {
    const toyc::codegen::RegisterAllocation allocation =
        toyc::codegen::RegisterAllocator::allocate(hotLoopFunction(), false);

    require(!allocation.assignment.physicalReg("%i").has_value(),
            "without -opt, vregs stay on stack");
    require(allocation.frame.containsVReg("%i"), "without -opt, vregs keep stack slots");
    require(allocation.frame.savedRegisterSlots().size() == 2,
            "without -opt, only ra/s0 are saved");
}

} // namespace

int main() {
    try {
        testAnalysisBuildsCfgLiveIntervals();
        testAnalysisWeightsLoopBlocksHigherThanColdBlocks();
        testAnalysisWeightsCallCrossingIntervalsHigher();
        testOptAssignsLiveIntervalsToCalleeSaved();
        testLinearScanKeepsCallCrossingIntervalUnderPressure();
        testLinearScanCostModelKeepsLoopHotInterval();
        testLinearScanSpillsWhenPressureExceedsRegisterPool();
        testLinearScanSpillsLongerActiveIntervalForShorterCurrent();
        testOptEmitsCalleeSavedInAssembly();
        testNonOptKeepsStackSlotsOnly();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
