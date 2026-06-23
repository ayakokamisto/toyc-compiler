#include "codegen/frame/VRegAnalysis.h"

#include "codegen/abi/CallingConvention.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace toyc::codegen {

namespace {

struct VRegAccess {
    std::vector<std::string> defs;
    std::vector<std::string> uses;
};

struct BlockFacts {
    std::set<std::string, std::less<>> defs;
    std::set<std::string, std::less<>> uses;
    std::set<std::string, std::less<>> liveIn;
    std::set<std::string, std::less<>> liveOut;
    std::vector<std::string> successors;
    int startPosition = 0;
    int endPosition = 0;
};

constexpr int kLoopWeightMultiplier = 10;
constexpr int kCallCrossingWeight = 25;

void appendUnique(std::vector<std::string>& values, std::string_view vreg) {
    if (vreg.empty()) {
        return;
    }
    const std::string key(vreg);
    if (std::find(values.begin(), values.end(), key) == values.end()) {
        values.push_back(key);
    }
}

int blockWeightForDepth(int depth) {
    int weight = 1;
    for (int i = 0; i < depth; ++i) {
        weight *= kLoopWeightMultiplier;
    }
    return weight;
}

void noteWeightedAccess(VRegAnalysis& analysis,
                        const VRegAccess& access,
                        int blockWeight) {
    for (const std::string& use : access.uses) {
        analysis.accessWeights[use] += blockWeight;
    }
    for (const std::string& def : access.defs) {
        analysis.accessWeights[def] += blockWeight;
    }
}

void noteDiscovery(VRegAnalysis& analysis, std::string_view vreg) {
    if (vreg.empty()) {
        return;
    }
    const std::string key(vreg);
    if (analysis.useCounts.find(key) == analysis.useCounts.end()) {
        analysis.discoveryOrder.push_back(key);
        analysis.useCounts.emplace(key, 0);
    }
}

void noteUse(VRegAnalysis& analysis, std::string_view vreg) {
    if (vreg.empty()) {
        return;
    }
    noteDiscovery(analysis, vreg);
    const std::string key(vreg);
    ++analysis.useCounts[key];
}

void noteCallInstruction(const contract::Instruction& instruction, VRegAnalysis& analysis) {
    std::visit(
        [&](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::CallInst>) {
                analysis.maxOutgoingArgBytes = std::max(
                    analysis.maxOutgoingArgBytes,
                    CallingConvention::stackArgBytesFor(inst.args.size()));
            } else if constexpr (std::is_same_v<Inst, contract::CallVoidInst>) {
                analysis.maxOutgoingArgBytes = std::max(
                    analysis.maxOutgoingArgBytes,
                    CallingConvention::stackArgBytesFor(inst.args.size()));
            }
        },
        instruction);
}

bool isCallInstruction(const contract::Instruction& instruction) {
    return std::visit(
        [](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            return std::is_same_v<Inst, contract::CallInst> ||
                   std::is_same_v<Inst, contract::CallVoidInst>;
        },
        instruction);
}

template <typename Inst>
void noteBinaryAccess(const Inst& inst, VRegAccess& access) {
    appendUnique(access.uses, inst.src1);
    appendUnique(access.uses, inst.src2);
    appendUnique(access.defs, inst.dst);
}

template <typename Inst>
void noteUnaryAccess(const Inst& inst, VRegAccess& access) {
    appendUnique(access.uses, inst.src);
    appendUnique(access.defs, inst.dst);
}

VRegAccess instructionAccess(const contract::Instruction& instruction) {
    VRegAccess access;
    std::visit(
        [&](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::ConstInst> ||
                          std::is_same_v<Inst, contract::LoadGlobalInst>) {
                appendUnique(access.defs, inst.dst);
            } else if constexpr (std::is_same_v<Inst, contract::CopyInst>) {
                appendUnique(access.uses, inst.src);
                appendUnique(access.defs, inst.dst);
            } else if constexpr (std::is_same_v<Inst, contract::StoreGlobalInst>) {
                appendUnique(access.uses, inst.src);
            } else if constexpr (std::is_same_v<Inst, contract::CallInst>) {
                for (const std::string& arg : inst.args) {
                    appendUnique(access.uses, arg);
                }
                appendUnique(access.defs, inst.dst);
            } else if constexpr (std::is_same_v<Inst, contract::CallVoidInst>) {
                for (const std::string& arg : inst.args) {
                    appendUnique(access.uses, arg);
                }
            } else if constexpr (std::is_same_v<Inst, contract::AddInst> ||
                                 std::is_same_v<Inst, contract::SubInst> ||
                                 std::is_same_v<Inst, contract::ModInst> ||
                                 std::is_same_v<Inst, contract::MulInst> ||
                                 std::is_same_v<Inst, contract::DivInst> ||
                                 std::is_same_v<Inst, contract::EqInst> ||
                                 std::is_same_v<Inst, contract::NeInst> ||
                                 std::is_same_v<Inst, contract::LtInst> ||
                                 std::is_same_v<Inst, contract::LeInst> ||
                                 std::is_same_v<Inst, contract::GtInst> ||
                                 std::is_same_v<Inst, contract::GeInst>) {
                noteBinaryAccess(inst, access);
            } else if constexpr (std::is_same_v<Inst, contract::NegInst> ||
                                 std::is_same_v<Inst, contract::LNotInst>) {
                noteUnaryAccess(inst, access);
            }
        },
        instruction);
    return access;
}

VRegAccess terminatorAccess(const contract::Terminator& terminator) {
    VRegAccess access;
    std::visit(
        [&](const auto& inst) {
            using Terminator = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Terminator, contract::BranchInst>) {
                appendUnique(access.uses, inst.cond);
            } else if constexpr (std::is_same_v<Terminator, contract::ReturnInst>) {
                if (inst.src.has_value()) {
                    appendUnique(access.uses, *inst.src);
                }
            }
        },
        terminator);
    return access;
}

std::vector<std::string> terminatorSuccessors(const contract::Terminator& terminator) {
    std::vector<std::string> successors;
    std::visit(
        [&](const auto& inst) {
            using Terminator = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Terminator, contract::JumpInst>) {
                successors.push_back(inst.targetLabel);
            } else if constexpr (std::is_same_v<Terminator, contract::BranchInst>) {
                successors.push_back(inst.trueLabel);
                if (inst.falseLabel != inst.trueLabel) {
                    successors.push_back(inst.falseLabel);
                }
            }
        },
        terminator);
    return successors;
}

void noteBlockAccess(BlockFacts& facts, const VRegAccess& access) {
    for (const std::string& use : access.uses) {
        if (facts.defs.find(use) == facts.defs.end()) {
            facts.uses.insert(use);
        }
    }
    for (const std::string& def : access.defs) {
        facts.defs.insert(def);
    }
}

std::set<std::string, std::less<>> setUnionWithoutDefs(
    const std::set<std::string, std::less<>>& uses,
    const std::set<std::string, std::less<>>& liveOut,
    const std::set<std::string, std::less<>>& defs) {
    std::set<std::string, std::less<>> result = uses;
    for (const std::string& vreg : liveOut) {
        if (defs.find(vreg) == defs.end()) {
            result.insert(vreg);
        }
    }
    return result;
}

struct IntervalBuilder {
    int minPosition = std::numeric_limits<int>::max();
    int maxPosition = std::numeric_limits<int>::min();

    void include(int position) {
        minPosition = std::min(minPosition, position);
        maxPosition = std::max(maxPosition, position);
    }

    [[nodiscard]] bool seen() const {
        return minPosition != std::numeric_limits<int>::max();
    }
};

std::vector<int> computeLoopDepths(
    const std::vector<contract::BasicBlock>& blocks,
    const std::vector<BlockFacts>& blockFacts,
    const std::map<std::string, std::size_t, std::less<>>& blockIndexByLabel) {
    std::vector<int> depths(blocks.size(), 0);
    for (std::size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
        for (const std::string& successor : blockFacts[blockIndex].successors) {
            const auto targetIt = blockIndexByLabel.find(successor);
            if (targetIt == blockIndexByLabel.end()) {
                continue;
            }
            const std::size_t targetIndex = targetIt->second;
            if (targetIndex > blockIndex) {
                continue;
            }
            for (std::size_t loopIndex = targetIndex; loopIndex <= blockIndex; ++loopIndex) {
                ++depths[loopIndex];
            }
        }
    }
    return depths;
}

} // namespace

VRegAnalysis analyzeVRegs(const contract::IRFunction& function) {
    VRegAnalysis analysis;
    std::vector<BlockFacts> blockFacts(function.basicBlocks.size());
    std::map<std::string, std::size_t, std::less<>> blockIndexByLabel;
    std::vector<std::pair<std::string, VRegAccess>> pointAccesses;
    std::vector<int> callPositions;
    int nextPosition = 0;

    for (const contract::Param& param : function.params) {
        noteDiscovery(analysis, param.vreg);
    }

    for (std::size_t blockIndex = 0; blockIndex < function.basicBlocks.size(); ++blockIndex) {
        const contract::BasicBlock& block = function.basicBlocks[blockIndex];
        blockIndexByLabel.emplace(block.label, blockIndex);
        BlockFacts& facts = blockFacts[blockIndex];
        facts.startPosition = nextPosition;

        for (const contract::Instruction& instruction : block.instructions) {
            const VRegAccess access = instructionAccess(instruction);
            for (const std::string& def : access.defs) {
                noteDiscovery(analysis, def);
            }
            for (const std::string& use : access.uses) {
                noteUse(analysis, use);
            }
            analysis.programPoints.push_back(
                VRegProgramPoint{nextPosition, block.label, access.defs, access.uses});
            pointAccesses.emplace_back(block.label, access);
            noteBlockAccess(facts, access);
            noteCallInstruction(instruction, analysis);
            if (isCallInstruction(instruction)) {
                callPositions.push_back(nextPosition);
            }
            ++nextPosition;
        }

        const VRegAccess terminator = terminatorAccess(block.terminator);
        for (const std::string& def : terminator.defs) {
            noteDiscovery(analysis, def);
        }
        for (const std::string& use : terminator.uses) {
            noteUse(analysis, use);
        }
        analysis.programPoints.push_back(
            VRegProgramPoint{nextPosition, block.label, terminator.defs, terminator.uses});
        pointAccesses.emplace_back(block.label, terminator);
        noteBlockAccess(facts, terminator);
        facts.endPosition = nextPosition;
        facts.successors = terminatorSuccessors(block.terminator);
        ++nextPosition;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t reverseIndex = blockFacts.size(); reverseIndex > 0; --reverseIndex) {
            const std::size_t blockIndex = reverseIndex - 1;
            BlockFacts& facts = blockFacts[blockIndex];

            std::set<std::string, std::less<>> newLiveOut;
            for (const std::string& successor : facts.successors) {
                const auto succIt = blockIndexByLabel.find(successor);
                if (succIt == blockIndexByLabel.end()) {
                    continue;
                }
                const BlockFacts& successorFacts = blockFacts[succIt->second];
                newLiveOut.insert(successorFacts.liveIn.begin(), successorFacts.liveIn.end());
            }
            const std::set<std::string, std::less<>> newLiveIn =
                setUnionWithoutDefs(facts.uses, newLiveOut, facts.defs);

            if (newLiveOut != facts.liveOut || newLiveIn != facts.liveIn) {
                facts.liveOut = std::move(newLiveOut);
                facts.liveIn = std::move(newLiveIn);
                changed = true;
            }
        }
    }

    const std::vector<int> loopDepths =
        computeLoopDepths(function.basicBlocks, blockFacts, blockIndexByLabel);
    for (std::size_t blockIndex = 0; blockIndex < function.basicBlocks.size(); ++blockIndex) {
        analysis.blockLoopDepths[function.basicBlocks[blockIndex].label] = loopDepths[blockIndex];
    }
    for (const auto& [blockLabel, access] : pointAccesses) {
        const auto depthIt = analysis.blockLoopDepths.find(blockLabel);
        const int depth = depthIt == analysis.blockLoopDepths.end() ? 0 : depthIt->second;
        noteWeightedAccess(analysis, access, blockWeightForDepth(depth));
    }

    std::map<std::string, IntervalBuilder, std::less<>> intervals;
    for (const std::string& vreg : analysis.discoveryOrder) {
        intervals.emplace(vreg, IntervalBuilder{});
    }
    for (const contract::Param& param : function.params) {
        intervals[param.vreg].include(0);
    }
    for (const VRegProgramPoint& point : analysis.programPoints) {
        for (const std::string& use : point.uses) {
            intervals[use].include(point.position);
        }
        for (const std::string& def : point.defs) {
            intervals[def].include(point.position);
        }
    }

    for (std::size_t blockIndex = 0; blockIndex < function.basicBlocks.size(); ++blockIndex) {
        const contract::BasicBlock& block = function.basicBlocks[blockIndex];
        const BlockFacts& facts = blockFacts[blockIndex];
        analysis.liveIns[block.label] = facts.liveIn;
        analysis.liveOuts[block.label] = facts.liveOut;
        for (const std::string& vreg : facts.liveIn) {
            intervals[vreg].include(facts.startPosition);
        }
        for (const std::string& vreg : facts.liveOut) {
            intervals[vreg].include(facts.endPosition);
        }
    }

    for (const std::string& vreg : analysis.discoveryOrder) {
        const auto it = intervals.find(vreg);
        if (it == intervals.end() || !it->second.seen()) {
            continue;
        }
        const auto countIt = analysis.useCounts.find(vreg);
        const int useCount = countIt == analysis.useCounts.end() ? 0 : countIt->second;
        const auto weightIt = analysis.accessWeights.find(vreg);
        const int accessWeight =
            weightIt == analysis.accessWeights.end() ? useCount : weightIt->second;
        int callCrossingCount = 0;
        for (const int callPosition : callPositions) {
            if (it->second.minPosition < callPosition && callPosition < it->second.maxPosition) {
                ++callCrossingCount;
            }
        }
        const int spillWeight = accessWeight + callCrossingCount * kCallCrossingWeight;
        analysis.liveIntervals.push_back(
            LiveInterval{
                vreg,
                it->second.minPosition,
                it->second.maxPosition,
                useCount,
                spillWeight,
                callCrossingCount});
    }

    return analysis;
}

} // namespace toyc::codegen
