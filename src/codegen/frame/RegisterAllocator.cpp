#include "codegen/frame/RegisterAllocator.h"

#include "codegen/frame/VRegAnalysis.h"
#include "codegen/frame/VRegAssignment.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace toyc::codegen {

namespace {

// Callee-saved registers: survive across calls but must be saved/restored in
// the prologue/epilogue. Used for values whose live range crosses a call.
constexpr std::array<std::string_view, 11> kCalleeSavedPool = {
    "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
};

// Caller-saved temporaries: free to use with no save/restore, but clobbered by
// any call. Used only for values whose live range does NOT cross a call.
// t0/t1 are reserved as instruction-selection scratch and excluded here.
constexpr std::array<std::string_view, 5> kCallerSavedPool = {
    "t2", "t3", "t4", "t5", "t6",
};

constexpr int kCalleeSavedSaveRestoreCost = 2;
constexpr int kMinimumRepeatedUseCount = 2;
constexpr int kLoopWeightedProfitability = 10;

bool isCallerSaved(std::string_view reg) {
    for (const std::string_view r : kCallerSavedPool) {
        if (r == reg) {
            return true;
        }
    }
    return false;
}

struct ActiveInterval {
    LiveInterval interval;
    std::string reg;
};

int registerOrder(std::string_view reg) {
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
    if (lhs.spillWeight != rhs.spillWeight) {
        return lhs.spillWeight < rhs.spillWeight;
    }
    if (lhs.end != rhs.end) {
        return lhs.end > rhs.end;
    }
    return lhs.vreg > rhs.vreg;
}

// A call-crossing value must live in a callee-saved register (survives calls)
// and pays a save/restore cost, so it must clear the profitability gate.
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

// A value that never crosses a call can use a caller-saved temp for free (no
// save/restore), so any used value is worth a register if one is available.
bool isProfitableForCallerSavedRegister(const LiveInterval& interval) {
    return interval.useCount > 0 && interval.callCrossingCount == 0;
}

bool isIntervalCandidate(const LiveInterval& interval) {
    if (interval.callCrossingCount > 0) {
        return isProfitableForCalleeSavedRegister(interval);
    }
    return isProfitableForCallerSavedRegister(interval) ||
           isProfitableForCalleeSavedRegister(interval);
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

// Pick a free register for `interval` from `freeRegs`. A call-crossing
// interval may only use callee-saved registers. A non-crossing interval
// prefers a caller-saved temp (free); it falls back to a callee-saved register
// only if it is profitable enough to pay the save/restore cost.
// Returns the chosen register, or empty if none is usable.
std::string pickFreeRegister(const LiveInterval& interval, std::vector<std::string>& freeRegs) {
    const bool crossesCall = interval.callCrossingCount > 0;
    // freeRegs is kept sorted caller-saved first, then callee-saved.
    if (!crossesCall) {
        for (auto it = freeRegs.begin(); it != freeRegs.end(); ++it) {
            if (isCallerSaved(*it)) {
                const std::string reg = *it;
                freeRegs.erase(it);
                return reg;
            }
        }
        // No caller-saved temp left. Only consume a callee-saved register if
        // this value is worth its save/restore cost.
        if (!isProfitableForCalleeSavedRegister(interval)) {
            return {};
        }
    }
    for (auto it = freeRegs.begin(); it != freeRegs.end(); ++it) {
        if (!isCallerSaved(*it)) {
            const std::string reg = *it;
            freeRegs.erase(it);
            return reg;
        }
    }
    return {};
}

VRegAssignment assignPhysicalRegistersLinearScan(
    const VRegAnalysis& analysis,
    const std::unordered_map<std::string, std::int32_t>& excluded) {
    std::vector<LiveInterval> intervals;
    intervals.reserve(analysis.liveIntervals.size());
    for (const LiveInterval& interval : analysis.liveIntervals) {
        if (excluded.count(interval.vreg) != 0) {
            continue; // foldable immediate constant: never needs a register
        }
        if (isIntervalCandidate(interval)) {
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

        if (std::string reg = pickFreeRegister(interval, freeRegs); !reg.empty()) {
            assigned[interval.vreg] = reg;
            active.push_back(ActiveInterval{interval, std::move(reg)});
            sortActiveByEnd(active);
            continue;
        }

        // No free register usable for this interval. Try to steal from a less
        // valuable active interval that holds a register this one can use.
        const bool calleeProfitable = isProfitableForCalleeSavedRegister(interval);
        ActiveInterval* spillTarget = nullptr;
        for (ActiveInterval& candidate : active) {
            if (isCallerSaved(candidate.reg)) {
                // A call-crossing interval cannot live in a caller-saved temp.
                if (interval.callCrossingCount > 0) {
                    continue;
                }
            } else if (!calleeProfitable) {
                // This interval is not worth a callee-saved register's
                // save/restore cost, so it may not steal one.
                continue;
            }
            if (spillTarget == nullptr ||
                isLessValuableForRegister(candidate.interval, spillTarget->interval)) {
                spillTarget = &candidate;
            }
        }
        if (spillTarget == nullptr ||
            !isLessValuableForRegister(spillTarget->interval, interval)) {
            continue;
        }

        const std::string reg = spillTarget->reg;
        assigned.erase(spillTarget->interval.vreg);
        *spillTarget = ActiveInterval{interval, reg};
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

RegisterAllocation RegisterAllocator::allocate(
    const contract::IRFunction& function,
    const bool enableOpt,
    const std::unordered_map<std::string, std::int32_t>& excluded) {
    const VRegAnalysis analysis = analyzeVRegs(function);
    VRegAssignment assignment;
    if (enableOpt) {
        assignment = assignPhysicalRegistersLinearScan(analysis, excluded);
    }

    StackFrame frame;
    for (const std::string& vreg : analysis.discoveryOrder) {
        if (excluded.count(vreg) != 0) {
            continue; // foldable immediate constant: never materialized
        }
        if (assignment.isStackSlot(vreg)) {
            frame.addVReg(vreg);
        }
    }
    if (enableOpt) {
        for (const std::string& vreg : analysis.discoveryOrder) {
            if (excluded.count(vreg) != 0) {
                continue;
            }
            if (const std::optional<std::string_view> reg = assignment.physicalReg(vreg)) {
                // Caller-saved temporaries (t2-t6) need no prologue/epilogue
                // save; only callee-saved registers are recorded in the frame.
                if (!isCallerSaved(*reg)) {
                    frame.addCalleeSavedRegister(*reg);
                }
            }
        }
    }
    frame.setOutgoingArgBytes(analysis.maxOutgoingArgBytes);
    frame.finalize();

    return RegisterAllocation{frame, assignment};
}

} // namespace toyc::codegen
