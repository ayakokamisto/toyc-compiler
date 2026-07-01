#include "toyc/ir/dominator_tree.h"

#include <algorithm>

DominatorTree::DominatorTree(const ControlFlowGraph& cfg) : cfg_(cfg) {
    const auto& rpo = cfg_.reversePostOrder();
    for (std::size_t i = 0; i < rpo.size(); ++i) {
        rpo_index_[rpo[i]] = i;
    }
    for (const BasicBlock* block : cfg_.blocks()) {
        idom_[block] = nullptr;
        children_[block] = {};
        frontier_[block] = {};
    }
    computeIdoms();
    computeChildren();
    computeFrontier();
    if (cfg_.entry() != nullptr && cfg_.isReachable(*cfg_.entry())) {
        computePreorder(cfg_.entry());
    }
}

bool DominatorTree::isReachable(const BasicBlock& block) const {
    return cfg_.isReachable(block);
}

const BasicBlock* DominatorTree::immediateDominator(const BasicBlock& block) const {
    auto it = idom_.find(&block);
    return it == idom_.end() ? nullptr : it->second;
}

bool DominatorTree::dominates(const BasicBlock& dominator, const BasicBlock& block) const {
    if (!isReachable(dominator) || !isReachable(block)) return false;
    const BasicBlock* current = &block;
    while (current != nullptr) {
        if (current == &dominator) return true;
        current = immediateDominator(*current);
    }
    return false;
}

const std::vector<const BasicBlock*>& DominatorTree::children(const BasicBlock& block) const {
    static const BlockList empty;
    auto it = children_.find(&block);
    return it == children_.end() ? empty : it->second;
}

const std::vector<const BasicBlock*>& DominatorTree::dominanceFrontier(const BasicBlock& block) const {
    static const BlockList empty;
    auto it = frontier_.find(&block);
    return it == frontier_.end() ? empty : it->second;
}

void DominatorTree::computeIdoms() {
    const auto& rpo = cfg_.reversePostOrder();
    if (rpo.empty()) return;
    const BasicBlock* entry = cfg_.entry();
    idom_[entry] = nullptr;

    bool changed = true;
    while (changed) {
        changed = false;
        for (const BasicBlock* block : rpo) {
            if (block == entry) continue;
            const BasicBlock* new_idom = nullptr;
            for (const BasicBlock* pred : cfg_.predecessors(*block)) {
                if (!cfg_.isReachable(*pred)) continue;
                if (pred != entry && idom_[pred] == nullptr) continue;
                new_idom = new_idom == nullptr ? pred : intersect(pred, new_idom);
            }
            if (idom_[block] != new_idom) {
                idom_[block] = new_idom;
                changed = true;
            }
        }
    }
}

const BasicBlock* DominatorTree::intersect(const BasicBlock* left, const BasicBlock* right) const {
    const BasicBlock* finger1 = left;
    const BasicBlock* finger2 = right;
    while (finger1 != finger2) {
        while (rpo_index_.at(finger1) > rpo_index_.at(finger2)) {
            finger1 = idom_.at(finger1);
        }
        while (rpo_index_.at(finger2) > rpo_index_.at(finger1)) {
            finger2 = idom_.at(finger2);
        }
    }
    return finger1;
}

void DominatorTree::computeChildren() {
    for (const BasicBlock* block : cfg_.reversePostOrder()) {
        const BasicBlock* parent = idom_[block];
        if (parent != nullptr) {
            children_[parent].push_back(block);
        }
    }
}

void DominatorTree::computeFrontier() {
    for (const BasicBlock* block : cfg_.reversePostOrder()) {
        std::size_t reachable_preds = 0;
        for (const BasicBlock* pred : cfg_.predecessors(*block)) {
            if (cfg_.isReachable(*pred)) ++reachable_preds;
        }
        if (reachable_preds < 2) continue;

        for (const BasicBlock* pred : cfg_.predecessors(*block)) {
            if (!cfg_.isReachable(*pred)) continue;
            const BasicBlock* runner = pred;
            while (runner != nullptr && runner != idom_[block]) {
                auto& list = frontier_[runner];
                if (std::find(list.begin(), list.end(), block) == list.end()) {
                    list.push_back(block);
                }
                runner = idom_[runner];
            }
        }
    }
}

void DominatorTree::computePreorder(const BasicBlock* block) {
    preorder_.push_back(block);
    for (const BasicBlock* child : children_[block]) {
        computePreorder(child);
    }
}
