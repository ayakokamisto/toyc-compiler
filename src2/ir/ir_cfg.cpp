#include "ir/ir_cfg.h"

#include <algorithm>
#include <queue>
#include <string>
#include <variant>
#include <vector>

namespace toyc::ir {
namespace {

namespace contract = toyc::codegen::contract;

// Extract successor labels from a terminator.
std::vector<std::string> terminatorSuccessors(const contract::Terminator& term) {
    return std::visit(
        [](const auto& t) -> std::vector<std::string> {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, contract::JumpInst>) {
                return {t.targetLabel};
            } else if constexpr (std::is_same_v<T, contract::BranchInst>) {
                if (t.trueLabel == t.falseLabel) {
                    return {t.trueLabel};
                }
                return {t.trueLabel, t.falseLabel};
            }
            // ReturnInst — no successors.
            return {};
        },
        term);
}

} // namespace

// ─── CFG Construction ──────────────────────────────────────────────────────

CFG buildCFG(const contract::IRFunction& function) {
    CFG cfg;
    const std::size_t n = function.basicBlocks.size();
    cfg.successors.resize(n);
    cfg.predecessors.resize(n);

    for (std::size_t i = 0; i < n; ++i) {
        cfg.labelToIndex[function.basicBlocks[i].label] = i;
    }

    for (std::size_t i = 0; i < n; ++i) {
        const auto& block = function.basicBlocks[i];
        for (const auto& label : terminatorSuccessors(block.terminator)) {
            auto it = cfg.labelToIndex.find(label);
            if (it != cfg.labelToIndex.end()) {
                const std::size_t succ = it->second;
                cfg.successors[i].push_back(succ);
                cfg.predecessors[succ].push_back(i);
            }
        }
    }

    return cfg;
}

// ─── Dominator Analysis ────────────────────────────────────────────────────

DominatorSets computeDominators(const contract::IRFunction& function,
                                const CFG& cfg) {
    const std::size_t n = function.basicBlocks.size();
    DominatorSets dom(n);

    // Initialize: dom(entry) = {entry}; dom(other) = all blocks.
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            dom[i].insert(j);
        }
    }

    if (n > 0) {
        dom[0] = {0};
    }

    // Iterate until fixed point.
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t i = 1; i < n; ++i) {
            std::unordered_set<std::size_t> newDom = {i};

            // Intersect dominator sets of all predecessors.
            if (!cfg.predecessors[i].empty()) {
                // Start with the first predecessor's dom set.
                std::unordered_set<std::size_t> intersection;
                bool first = true;
                for (const std::size_t pred : cfg.predecessors[i]) {
                    if (first) {
                        intersection = dom[pred];
                        first = false;
                    } else {
                        // Keep only blocks present in both sets.
                        std::unordered_set<std::size_t> temp;
                        for (const std::size_t b : intersection) {
                            if (dom[pred].count(b) > 0) {
                                temp.insert(b);
                            }
                        }
                        intersection = std::move(temp);
                    }
                }
                for (const std::size_t b : intersection) {
                    newDom.insert(b);
                }
            }

            if (newDom != dom[i]) {
                dom[i] = std::move(newDom);
                changed = true;
            }
        }
    }

    return dom;
}

bool dominates(const DominatorSets& dom, std::size_t a, std::size_t b) {
    if (b >= dom.size()) return false;
    return dom[b].count(a) > 0;
}

// ─── Natural Loop Detection ───────────────────────────────────────────────

std::vector<NaturalLoop> findNaturalLoops(const contract::IRFunction& function,
                                          const CFG& cfg,
                                          const DominatorSets& dom) {
    std::vector<NaturalLoop> loops;

    // Find back edges: tail → header where header dominates tail.
    for (std::size_t tail = 0; tail < function.basicBlocks.size(); ++tail) {
        for (const std::size_t header : cfg.successors[tail]) {
            if (!dominates(dom, header, tail)) {
                continue;
            }

            // Found a back edge. Build the natural loop.
            NaturalLoop loop;
            loop.header = header;
            loop.blocks.insert(header);
            loop.blocks.insert(tail);

            // Backward BFS from tail, stopping at header.
            if (tail != header) {
                std::queue<std::size_t> worklist;
                worklist.push(tail);

                while (!worklist.empty()) {
                    const std::size_t current = worklist.front();
                    worklist.pop();

                    for (const std::size_t pred : cfg.predecessors[current]) {
                        if (loop.blocks.count(pred) == 0) {
                            loop.blocks.insert(pred);
                            worklist.push(pred);
                        }
                    }
                }
            }

            loops.push_back(std::move(loop));
        }
    }

    return loops;
}

// ─── Preheader Creation ───────────────────────────────────────────────────

std::size_t ensurePreheader(contract::IRFunction& function,
                            CFG& cfg,
                            const NaturalLoop& loop) {
    const std::size_t header = loop.header;

    // Find predecessors of the header that are outside the loop.
    std::vector<std::size_t> outsidePreds;
    for (const std::size_t pred : cfg.predecessors[header]) {
        if (loop.blocks.count(pred) == 0) {
            outsidePreds.push_back(pred);
        }
    }

    if (outsidePreds.size() == 1) {
        // Single outside predecessor — use it as preheader.
        return outsidePreds[0];
    }

    // Multiple outside predecessors — create a new preheader block.
    const std::string preheaderLabel = "licm_preheader_" + std::to_string(header);
    const std::string& headerLabel = function.basicBlocks[header].label;

    // Create the preheader block with a jump to the header.
    contract::BasicBlock preheader;
    preheader.label = preheaderLabel;
    preheader.instructions = {};
    preheader.terminator = contract::JumpInst{headerLabel};

    // Insert the preheader just before the header in block order.
    const std::size_t preheaderIndex = header;
    function.basicBlocks.insert(
        function.basicBlocks.begin() + static_cast<std::ptrdiff_t>(preheaderIndex),
        std::move(preheader));

    // Rebuild the CFG after insertion (all indices shift).
    // Update outside predecessors to jump to preheader instead of header.
    for (auto& block : function.basicBlocks) {
        std::visit(
            [&](auto& t) {
                using T = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<T, contract::JumpInst>) {
                    if (t.targetLabel == headerLabel &&
                        // Don't redirect the preheader itself or loop-internal blocks.
                        &block != &function.basicBlocks[preheaderIndex]) {
                        // Check if this block is outside the loop.
                        // After insertion, loop block indices shifted by 1 if >= preheaderIndex.
                        bool isOutside = true;
                        for (const std::size_t origIdx : loop.blocks) {
                            const std::size_t shiftedIdx =
                                origIdx >= preheaderIndex ? origIdx + 1 : origIdx;
                            if (&block == &function.basicBlocks[shiftedIdx]) {
                                isOutside = false;
                                break;
                            }
                        }
                        if (isOutside) {
                            t.targetLabel = preheaderLabel;
                        }
                    }
                } else if constexpr (std::is_same_v<T, contract::BranchInst>) {
                    // Redirect true/false targets.
                    auto redirect = [&](std::string& target) {
                        if (target == headerLabel) {
                            bool isOutside = true;
                            for (const std::size_t origIdx : loop.blocks) {
                                const std::size_t shiftedIdx =
                                    origIdx >= preheaderIndex ? origIdx + 1 : origIdx;
                                if (&block == &function.basicBlocks[shiftedIdx]) {
                                    isOutside = false;
                                    break;
                                }
                            }
                            if (isOutside) {
                                target = preheaderLabel;
                            }
                        }
                    };
                    redirect(t.trueLabel);
                    redirect(t.falseLabel);
                }
            },
            block.terminator);
    }

    // Rebuild CFG after modification.
    cfg = buildCFG(function);

    return preheaderIndex;
}

} // namespace toyc::ir
