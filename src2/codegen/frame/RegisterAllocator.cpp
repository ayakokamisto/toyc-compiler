#include "codegen/frame/RegisterAllocator.h"

#include "codegen/abi/CallingConvention.h"
#include "codegen/frame/VRegAnalysis.h"
#include "codegen/frame/VRegAssignment.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace toyc::codegen {

namespace {

constexpr std::array<std::string_view, 11> kCalleeSavedPool = {
    "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
};

// Caller-saved registers available for allocation (t0/t1 reserved as scratch).
constexpr std::array<std::string_view, 5> kCallerSavedPool = {
    "t2", "t3", "t4", "t5", "t6",
};

constexpr int kCalleeSavedSaveRestoreCost = 2;
constexpr int kMinimumRepeatedUseCount = 2;
constexpr int kLoopWeightedProfitability = 30;
constexpr int kCallCrossingWeight = 25;
constexpr int kCallerSavedMaxSpillWeight = 5;  // Max weight for caller-saved allocation

struct ActiveInterval {
    LiveInterval interval;
    std::string reg;
};

bool isCallerSaved(std::string_view reg) {
    for (const std::string_view r : kCallerSavedPool) {
        if (r == reg) return true;
    }
    return false;
}

int registerOrder(std::string_view reg) {
    // Caller-saved registers come first (preferred for short-lived values).
    for (std::size_t i = 0; i < kCallerSavedPool.size(); ++i) {
        if (kCallerSavedPool[i] == reg) {
            return static_cast<int>(i);
        }
    }
    for (std::size_t i = 0; i < kCalleeSavedPool.size(); ++i) {
        if (kCalleeSavedPool[i] == reg) {
            return static_cast<int>(kCallerSavedPool.size() + i);
        }
    }
    return static_cast<int>(kCallerSavedPool.size() + kCalleeSavedPool.size());
}

void sortFreeRegisters(std::vector<std::string>& freeRegs) {
    std::sort(freeRegs.begin(), freeRegs.end(), [](const std::string& lhs, const std::string& rhs) {
        return registerOrder(lhs) < registerOrder(rhs);
    });
}

void sortActiveByEnd(std::vector<ActiveInterval>& active) {
    std::sort(active.begin(), active.end(), [](const ActiveInterval& lhs, const ActiveInterval& rhs) {
        if (lhs.interval.end != rhs.interval.end) {
            return lhs.interval.end < rhs.interval.end;
        }
        return lhs.interval.vreg < rhs.interval.vreg;
    });
}

bool isLessValuableForRegister(const LiveInterval& lhs, const LiveInterval& rhs) {
    const int lhsCost =
        lhs.spillWeight + lhs.callCrossingCount * kCallCrossingWeight -
        std::max(0, lhs.end - lhs.start) / 4;
    const int rhsCost =
        rhs.spillWeight + rhs.callCrossingCount * kCallCrossingWeight -
        std::max(0, rhs.end - rhs.start) / 4;
    if (lhsCost != rhsCost) {
        return lhsCost < rhsCost;
    }
    if (lhs.end != rhs.end) {
        return lhs.end > rhs.end;
    }
    return lhs.vreg > rhs.vreg;
}

bool functionHasCall(const contract::IRFunction& function) {
    for (const contract::BasicBlock& block : function.basicBlocks) {
        for (const contract::Instruction& instruction : block.instructions) {
            if (std::visit(
                    [](const auto& inst) {
                        using Inst = std::decay_t<decltype(inst)>;
                        return std::is_same_v<Inst, contract::CallInst> ||
                               std::is_same_v<Inst, contract::CallVoidInst>;
                    },
                    instruction)) {
                return true;
            }
        }
    }
    return false;
}

std::map<std::string, std::int32_t, std::less<>> collectRematerializableConstants(
    const contract::IRFunction& function) {
    std::map<std::string, std::int32_t, std::less<>> constants;
    std::set<std::string, std::less<>> rejected;

    auto noteDefinition = [&](std::string_view vreg, std::optional<std::int32_t> value) {
        if (vreg.empty()) {
            return;
        }
        const std::string key(vreg);
        if (!value.has_value()) {
            constants.erase(key);
            rejected.insert(key);
            return;
        }
        if (rejected.find(key) != rejected.end()) {
            return;
        }
        const auto [it, inserted] = constants.emplace(key, *value);
        if (!inserted && it->second != *value) {
            constants.erase(it);
            rejected.insert(key);
        }
    };

    for (const contract::BasicBlock& block : function.basicBlocks) {
        for (const contract::Instruction& instruction : block.instructions) {
            std::visit(
                [&](const auto& inst) {
                    using Inst = std::decay_t<decltype(inst)>;
                    if constexpr (std::is_same_v<Inst, contract::ConstInst>) {
                        noteDefinition(inst.dst, inst.value);
                    } else if constexpr (std::is_same_v<Inst, contract::CopyInst> ||
                                         std::is_same_v<Inst, contract::LoadGlobalInst> ||
                                         std::is_same_v<Inst, contract::CallInst> ||
                                         std::is_same_v<Inst, contract::AddInst> ||
                                         std::is_same_v<Inst, contract::SubInst> ||
                                         std::is_same_v<Inst, contract::MulInst> ||
                                         std::is_same_v<Inst, contract::DivInst> ||
                                         std::is_same_v<Inst, contract::ModInst> ||
                                         std::is_same_v<Inst, contract::NegInst> ||
                                         std::is_same_v<Inst, contract::EqInst> ||
                                         std::is_same_v<Inst, contract::NeInst> ||
                                         std::is_same_v<Inst, contract::LtInst> ||
                                         std::is_same_v<Inst, contract::LeInst> ||
                                         std::is_same_v<Inst, contract::GtInst> ||
                                         std::is_same_v<Inst, contract::GeInst> ||
                                         std::is_same_v<Inst, contract::LNotInst>) {
                        noteDefinition(inst.dst, std::nullopt);
                    }
                },
                instruction);
        }
    }
    return constants;
}

bool isProfitableForCalleeSavedRegister(const LiveInterval& interval) {
    if (interval.useCount <= 0) {
        return false;
    }
    if (interval.callCrossingCount > 0) {
        return true;
    }
    if (interval.spillWeight >= kLoopWeightedProfitability) {
        return true;
    }
    if (interval.spillWeight <= kCalleeSavedSaveRestoreCost) {
        return false;
    }
    return interval.useCount >= kMinimumRepeatedUseCount;
}

void expireOldIntervals(const LiveInterval& current,
                        std::vector<ActiveInterval>& active,
                        std::vector<std::string>& freeRegs) {
    auto it = active.begin();
    while (it != active.end()) {
        if (it->interval.end >= current.start) {
            ++it;
            continue;
        }
        freeRegs.push_back(it->reg);
        it = active.erase(it);
    }
    sortFreeRegisters(freeRegs);
}

VRegAssignment assignPhysicalRegistersLinearScan(const VRegAnalysis& analysis) {
    std::vector<LiveInterval> intervals;
    intervals.reserve(analysis.liveIntervals.size());
    for (const LiveInterval& interval : analysis.liveIntervals) {
        // Accept intervals profitable for callee-saved OR short-lived for caller-saved.
        // Caller-saved registers are only for non-loop, non-call-crossing, low-weight values.
        const bool profitableForCalleeSaved = isProfitableForCalleeSavedRegister(interval);
        const bool profitableForCallerSaved =
            interval.useCount >= 1 && interval.spillWeight <= kCallerSavedMaxSpillWeight &&
            interval.callCrossingCount == 0 && !interval.loopCarried;
        if (profitableForCalleeSaved || profitableForCallerSaved) {
            intervals.push_back(interval);
        }
    }
    std::sort(intervals.begin(), intervals.end(), [](const LiveInterval& lhs, const LiveInterval& rhs) {
        if (lhs.start != rhs.start) {
            return lhs.start < rhs.start;
        }
        if (lhs.end != rhs.end) {
            return lhs.end < rhs.end;
        }
        return lhs.vreg < rhs.vreg;
    });

    // Combined pool: caller-saved first (preferred for short-lived), then callee-saved.
    std::vector<std::string> freeRegs;
    freeRegs.reserve(kCallerSavedPool.size() + kCalleeSavedPool.size());
    for (const std::string_view reg : kCallerSavedPool) {
        freeRegs.emplace_back(reg);
    }
    for (const std::string_view reg : kCalleeSavedPool) {
        freeRegs.emplace_back(reg);
    }

    std::vector<ActiveInterval> active;
    std::map<std::string, std::string, std::less<>> assigned;

    for (const LiveInterval& interval : intervals) {
        expireOldIntervals(interval, active, freeRegs);

        if (!freeRegs.empty()) {
            // Prefer callee-saved for high-weight/call-crossing intervals.
            const bool needsCalleeSaved =
                interval.callCrossingCount > 0 || interval.loopCarried;
            std::string reg;
            if (needsCalleeSaved) {
                // Find first callee-saved register in the free pool.
                auto it = std::find_if(freeRegs.begin(), freeRegs.end(),
                    [](std::string_view r) { return !isCallerSaved(r); });
                if (it != freeRegs.end()) {
                    reg = *it;
                    freeRegs.erase(it);
                }
            }
            if (reg.empty()) {
                if (needsCalleeSaved) {
                    // No callee-saved register available — spill this interval
                    // rather than assigning a caller-saved register that would
                    // be clobbered by a call instruction.
                    continue;
                }
                reg = freeRegs.front();
                freeRegs.erase(freeRegs.begin());
            }
            assigned[interval.vreg] = reg;
            active.push_back(ActiveInterval{interval, reg});
            sortActiveByEnd(active);
            continue;
        }

        auto spillIt = std::min_element(
            active.begin(),
            active.end(),
            [](const ActiveInterval& lhs, const ActiveInterval& rhs) {
                return isLessValuableForRegister(lhs.interval, rhs.interval);
            });
        if (spillIt == active.end() ||
            !isLessValuableForRegister(spillIt->interval, interval)) {
            continue;
        }

        const std::string reg = spillIt->reg;
        assigned.erase(spillIt->interval.vreg);
        *spillIt = ActiveInterval{interval, reg};
        assigned[interval.vreg] = reg;
        sortActiveByEnd(active);
    }

    VRegAssignment assignment;
    for (const auto& [vreg, reg] : assigned) {
        assignment.assignPhysical(vreg, reg);
    }
    return assignment;
}

} // namespace

RegisterAllocation RegisterAllocator::allocate(const contract::IRFunction& function,
                                               const bool enableOpt) {
    const VRegAnalysis analysis = analyzeVRegs(function);
    VRegAssignment assignment;
    if (enableOpt) {
        assignment = assignPhysicalRegistersLinearScan(analysis);
        for (const auto& [vreg, value] : collectRematerializableConstants(function)) {
            if (assignment.isStackSlot(vreg)) {
                assignment.assignRematerializedConstant(vreg, value);
            }
        }
    }

    StackFrame frame;
    bool hasStackSlots = false;
    for (const std::string& vreg : analysis.discoveryOrder) {
        if (assignment.isStackSlot(vreg)) {
            frame.addVReg(vreg);
            hasStackSlots = true;
        }
    }
    if (enableOpt) {
        for (const std::string& vreg : analysis.discoveryOrder) {
            if (const std::optional<std::string_view> reg = assignment.physicalReg(vreg)) {
                // Only callee-saved registers need prologue/epilogue save/restore.
                if (!isCallerSaved(*reg)) {
                    frame.addCalleeSavedRegister(*reg);
                }
            }
        }
    }
    frame.setOutgoingArgBytes(analysis.maxOutgoingArgBytes);
    if (enableOpt) {
        frame.setSavesReturnAddress(functionHasCall(function));
        frame.setUsesFramePointer(
            hasStackSlots || function.params.size() > CallingConvention::kArgRegs.size());
    }
    frame.finalize();

    return RegisterAllocation{frame, assignment};
}

} // namespace toyc::codegen
