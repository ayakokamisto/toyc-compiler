#include "codegen/frame/RegisterAllocator.h"

#include "codegen/frame/VRegAnalysis.h"
#include "codegen/frame/VRegAssignment.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace toyc::codegen {

namespace {

constexpr std::array<std::string_view, 11> kCalleeSavedPool = {
    "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
};

struct ActiveInterval {
    LiveInterval interval;
    std::string reg;
};

int registerOrder(std::string_view reg) {
    for (std::size_t i = 0; i < kCalleeSavedPool.size(); ++i) {
        if (kCalleeSavedPool[i] == reg) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(kCalleeSavedPool.size());
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
        if (interval.useCount > 0) {
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
    freeRegs.reserve(kCalleeSavedPool.size());
    for (const std::string_view reg : kCalleeSavedPool) {
        freeRegs.emplace_back(reg);
    }

    std::vector<ActiveInterval> active;
    std::map<std::string, std::string, std::less<>> assigned;

    for (const LiveInterval& interval : intervals) {
        expireOldIntervals(interval, active, freeRegs);

        if (!freeRegs.empty()) {
            const std::string reg = freeRegs.front();
            freeRegs.erase(freeRegs.begin());
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
    }

    StackFrame frame;
    for (const std::string& vreg : analysis.discoveryOrder) {
        if (assignment.isStackSlot(vreg)) {
            frame.addVReg(vreg);
        }
    }
    if (enableOpt) {
        for (const std::string& vreg : analysis.discoveryOrder) {
            if (const std::optional<std::string_view> reg = assignment.physicalReg(vreg)) {
                frame.addCalleeSavedRegister(*reg);
            }
        }
    }
    frame.setOutgoingArgBytes(analysis.maxOutgoingArgBytes);
    frame.finalize();

    return RegisterAllocation{frame, assignment};
}

} // namespace toyc::codegen
