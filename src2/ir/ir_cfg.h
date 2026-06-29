#pragma once

#include "codegen/ContractIR.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace toyc::ir {

// Control-flow graph built over a function's basic blocks.
struct CFG {
    std::vector<std::vector<std::size_t>> successors;
    std::vector<std::vector<std::size_t>> predecessors;
    std::unordered_map<std::string, std::size_t> labelToIndex;
};

[[nodiscard]] CFG buildCFG(const codegen::contract::IRFunction& function);

// Dominator sets: dom[B] = set of blocks that dominate B.
using DominatorSets = std::vector<std::unordered_set<std::size_t>>;

[[nodiscard]] DominatorSets computeDominators(
    const codegen::contract::IRFunction& function,
    const CFG& cfg);

[[nodiscard]] bool dominates(const DominatorSets& dom,
                             std::size_t a,
                             std::size_t b);

// A natural loop identified by a back edge.
struct NaturalLoop {
    std::size_t header;                 // The loop header block index
    std::unordered_set<std::size_t> blocks; // All blocks in the loop (including header)
};

// Find all natural loops in the function.
[[nodiscard]] std::vector<NaturalLoop> findNaturalLoops(
    const codegen::contract::IRFunction& function,
    const CFG& cfg,
    const DominatorSets& dom);

// Ensure the loop has a preheader block. Returns the preheader block index.
// If a suitable outside predecessor already exists, returns it.
// Otherwise creates a new preheader block and inserts it.
std::size_t ensurePreheader(codegen::contract::IRFunction& function,
                            CFG& cfg,
                            const NaturalLoop& loop);

} // namespace toyc::ir
