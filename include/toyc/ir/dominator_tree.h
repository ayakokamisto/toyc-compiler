#pragma once

#include "control_flow_graph.h"

#include <unordered_map>
#include <vector>

class DominatorTree {
public:
    explicit DominatorTree(const ControlFlowGraph& cfg);

    bool isReachable(const BasicBlock& block) const;
    const BasicBlock* immediateDominator(const BasicBlock& block) const;
    bool dominates(const BasicBlock& dominator, const BasicBlock& block) const;
    const std::vector<const BasicBlock*>& children(const BasicBlock& block) const;
    const std::vector<const BasicBlock*>& dominanceFrontier(const BasicBlock& block) const;
    const std::vector<const BasicBlock*>& dominatorTreePreorder() const { return preorder_; }

private:
    using BlockList = std::vector<const BasicBlock*>;

    void computeIdoms();
    void computeChildren();
    void computeFrontier();
    void computePreorder(const BasicBlock* block);
    const BasicBlock* intersect(const BasicBlock* left, const BasicBlock* right) const;

    const ControlFlowGraph& cfg_;
    std::unordered_map<const BasicBlock*, const BasicBlock*> idom_;
    std::unordered_map<const BasicBlock*, BlockList> children_;
    std::unordered_map<const BasicBlock*, BlockList> frontier_;
    std::unordered_map<const BasicBlock*, std::size_t> rpo_index_;
    BlockList preorder_;
};
