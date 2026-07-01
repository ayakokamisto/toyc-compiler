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

class ControlFlowGraph {
public:
    using BlockList = std::vector<const BasicBlock*>;
    using BlockSet = std::unordered_set<const BasicBlock*>;
    using BlockMap = std::unordered_map<const BasicBlock*, BlockList>;

    explicit ControlFlowGraph(const Function& function);

    const Function& function() const { return function_; }
    const BasicBlock* entry() const { return entry_; }
    const BlockList& blocks() const { return blocks_; }
    const BlockList& successors(const BasicBlock& block) const;
    const BlockList& predecessors(const BasicBlock& block) const;
    bool isReachable(const BasicBlock& block) const;
    const BlockList& reversePostOrder() const { return rpo_; }
    const BlockList& unreachableBlocks() const { return unreachable_; }

private:
    void build();
    void dfsReachable(const BasicBlock* block, BlockSet& visited);

    const Function& function_;
    const BasicBlock* entry_ = nullptr;
    BlockList blocks_;
    BlockMap successors_;
    BlockMap predecessors_;
    BlockSet reachable_;
    BlockList rpo_;
    BlockList unreachable_;
};
