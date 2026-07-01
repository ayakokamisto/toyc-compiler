#pragma once

#include "basic_block.h"
#include "function.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

// =============================================================================
// CFG — Control-Flow Graph for a single Function.
//
// Computed via CFG::build(function). Provides successors, predecessors, and
// reachability queries.
//
// Uses BasicBlock* as block identity (pointer identity).
// =============================================================================

class CFG {
public:
    using BlockSet = std::unordered_set<BasicBlock*>;
    using BlockMap = std::unordered_map<BasicBlock*, std::vector<BasicBlock*>>;

    // Build CFG from a function. Scans all blocks and resolves branch targets
    // via Label → BasicBlock mapping.
    static CFG build(Function& function);

    const std::vector<BasicBlock*>& successors(BasicBlock* bb) const;
    const std::vector<BasicBlock*>& predecessors(BasicBlock* bb) const;

    bool is_reachable(BasicBlock* bb) const;
    const BlockSet& reachable_blocks() const { return reachable_; }

private:
    CFG(BlockMap successors, BlockMap predecessors, BlockSet reachable);

    static BlockSet find_reachable(BasicBlock* entry, const BlockMap& succ);

    BlockMap successors_;
    BlockMap predecessors_;
    BlockSet reachable_;
};

// Extract successor Label targets from a block's terminator.
// Returns an empty vector for blocks with a Return terminator or no terminator.
std::vector<Label*> successor_labels(BasicBlock* bb);
