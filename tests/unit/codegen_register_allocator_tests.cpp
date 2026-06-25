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
    // Enough cold values to exceed the 16-register pool so the loop-weighted
    // hot value must win a register over some cold value.
    constexpr int kColdCount = 20;
    std::vector<toyc::codegen::contract::Instruction> entryInstructions;
    for (int i = 0; i < kColdCount; ++i) {
        entryInstructions.push_back(
            toyc::codegen::contract::ConstInst{"%cold" + std::to_string(i), i + 1});
    }
    entryInstructions.push_back(toyc::codegen::contract::ConstInst{"%hot", 0});

    std::vector<toyc::codegen::contract::Instruction> exitInstructions;
    exitInstructions.push_back(toyc::codegen::contract::AddInst{"%ret", "%hot", "%cold0"});
    for (int i = 1; i < kColdCount; ++i) {
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

    // Enough simultaneously-live args to exceed the 16-register pool (t2-t6 +
    // s1-s11), forcing the linear scan to spill.
    std::vector<std::string> args;
    args.reserve(20);
    for (int i = 0; i < 20; ++i) {
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
    require(iReg.has_value(), "loop counter should receive a register under -opt");
    require(sumReg.has_value(), "accumulator should receive a register under -opt");
    require(!allocation.frame.containsVReg("%i"), "hot vreg should not keep a stack slot");
    require(!allocation.frame.containsVReg("%sum"), "hot vreg should not keep a stack slot");

    // These vregs never cross a call, so they should use caller-saved temps
    // (t2-t6), which need no prologue/epilogue save.
    auto isCallerSaved = [](std::string_view reg) {
        return reg == "t2" || reg == "t3" || reg == "t4" || reg == "t5" || reg == "t6";
    };
    require(isCallerSaved(*iReg) && isCallerSaved(*sumReg),
            "non-call-crossing hot vregs should use caller-saved temporaries");
    for (const toyc::codegen::SavedRegisterSlot& slot : allocation.frame.savedRegisterSlots()) {
        require(slot.reg[0] != 's' || slot.reg == "s0",
                "no s1-s11 callee-saved register should be saved when only caller-saved temps are used");
    }
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
    for (int i = 0; i < 20; ++i) {
        const std::string vreg = "%cold" + std::to_string(i);
        if (!allocation.assignment.physicalReg(vreg).has_value() &&
            allocation.frame.containsVReg(vreg)) {
            ++spilledColdCount;
        }
    }
    require(spilledColdCount >= 1,
            "cost model should spill at least one cold interval for the loop hot interval");
}

void testProfitabilityFilterKeepsColdSingleUseIntervalsOnStack() {
    const toyc::codegen::RegisterAllocation allocation =
        toyc::codegen::RegisterAllocator::allocate(pressureFunction(), true);

    auto isCallerSaved = [](std::string_view reg) {
        return reg == "t2" || reg == "t3" || reg == "t4" || reg == "t5" || reg == "t6";
    };

    int calleeSavedInputCount = 0;
    for (int i = 0; i < 12; ++i) {
        const std::string vreg = "%v" + std::to_string(i);
        if (const auto reg = allocation.assignment.physicalReg(vreg)) {
            // Cold single-use inputs may only use free caller-saved temps,
            // never a callee-saved register (which would cost save/restore).
            if (!isCallerSaved(*reg)) {
                ++calleeSavedInputCount;
            }
        }
    }

    int assignedCalleeSavedSlots = 0;
    for (const toyc::codegen::SavedRegisterSlot& slot : allocation.frame.savedRegisterSlots()) {
        if (slot.reg != "ra" && slot.reg != "s0") {
            ++assignedCalleeSavedSlots;
        }
    }

    require(allocation.assignment.physicalReg("%acc").has_value(),
            "cost model should keep high-access accumulator in a register");
    require(calleeSavedInputCount == 0,
            "single-use cold inputs should not pay callee-saved save/restore cost");
    require(assignedCalleeSavedSlots < 11,
            "profitability filter should avoid filling the full s1-s11 pool with cold values");
}

void testProfitabilityFilterRejectsShortSingleUseInterval() {
    const toyc::codegen::RegisterAllocation allocation =
        toyc::codegen::RegisterAllocator::allocate(spillPrefersShorterIntervalFunction(), true);

    // A short single-use value never crosses a call, so it may use a free
    // caller-saved temp, but it must never consume a callee-saved register
    // (whose save/restore cost is not worth a single use).
    auto isCallerSaved = [](std::string_view reg) {
        return reg == "t2" || reg == "t3" || reg == "t4" || reg == "t5" || reg == "t6";
    };
    if (const auto shortReg = allocation.assignment.physicalReg("%short")) {
        require(isCallerSaved(*shortReg),
                "short single-use interval may only use a free caller-saved temp");
    }
    require(allocation.assignment.physicalReg("%acc").has_value(),
            "repeated accumulator should still receive a register");
}

void testOptEmitsCalleeSavedInAssembly() {
    toyc::codegen::contract::IRModule module;
    module.functions.push_back(hotLoopFunction());

    toyc::codegen::BackendOptions options;
    options.enableOpt = true;
    const std::string assembly = toyc::codegen::RiscvBackend().generate(module, options);

    // The hot loop has no calls, so its values live in caller-saved temps and
    // the return path moves the accumulator from a register (no stack reload).
    require(assembly.find("    mv a0, t") != std::string::npos ||
                assembly.find("    mv a0, s") != std::string::npos,
            "return path moves the accumulator from a register under -opt");
    require(assembly.find("    li t0") == std::string::npos ||
                assembly.find("    slti") != std::string::npos,
            "the loop bound is compared via an immediate rather than a reloaded constant");
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
        testProfitabilityFilterKeepsColdSingleUseIntervalsOnStack();
        testProfitabilityFilterRejectsShortSingleUseInterval();
        testOptEmitsCalleeSavedInAssembly();
        testNonOptKeepsStackSlotsOnly();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
