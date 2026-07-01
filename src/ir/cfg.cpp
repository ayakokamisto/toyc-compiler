#include "toyc/ir/cfg.h"

#include <algorithm>
#include <queue>
#include <stdexcept>

// ===========================================================================
// successor_labels — extract outgoing Label targets from a block's terminator.
// ===========================================================================

std::vector<Label*> successor_labels(BasicBlock* bb) {
    Instr* term = bb->terminator();
    if (auto* br = dynamic_cast<BranchInstr*>(term)) {
        return {br->target()};
    }
    if (auto* cbr = dynamic_cast<CondBranchInstr*>(term)) {
        return {cbr->true_target(), cbr->false_target()};
    }
    return {};
}

static std::vector<Label*> successor_labels_const(const BasicBlock* bb) {
    Instr* term = bb->terminator();
    if (auto* br = dynamic_cast<BranchInstr*>(term)) {
        return {br->target()};
    }
    if (auto* cbr = dynamic_cast<CondBranchInstr*>(term)) {
        if (cbr->true_target() == cbr->false_target()) {
            return {cbr->true_target()};
        }
        return {cbr->true_target(), cbr->false_target()};
    }
    return {};
}

// ===========================================================================
// CFG::build
// ===========================================================================

CFG CFG::build(Function& function) {
    // Build Label → BasicBlock mapping.
    std::unordered_map<Label*, BasicBlock*> block_by_label;
    BlockMap succ, pred;

    for (auto& block : function.blocks()) {
        BasicBlock* bb = block.get();
        block_by_label[bb->label()] = bb;
        succ[bb] = {};
        pred[bb] = {};
    }

    // Resolve successors/predecessors.
    for (auto& block : function.blocks()) {
        BasicBlock* bb = block.get();
        for (Label* target : successor_labels(bb)) {
            auto it = block_by_label.find(target);
            // In Java this throws if target not found; for robustness we skip.
            if (it == block_by_label.end()) continue;

            BasicBlock* succ_block = it->second;
            auto& succ_list = succ[bb];
            if (std::find(succ_list.begin(), succ_list.end(), succ_block) == succ_list.end()) {
                succ_list.push_back(succ_block);
            }

            auto& pred_list = pred[succ_block];
            if (std::find(pred_list.begin(), pred_list.end(), bb) == pred_list.end()) {
                pred_list.push_back(bb);
            }
        }
    }

    BlockSet reachable = find_reachable(function.entry_block(), succ);
    return CFG(std::move(succ), std::move(pred), std::move(reachable));
}

// ===========================================================================
// CFG private helpers
// ===========================================================================

CFG::CFG(BlockMap successors, BlockMap predecessors, BlockSet reachable)
    : successors_(std::move(successors)),
      predecessors_(std::move(predecessors)),
      reachable_(std::move(reachable)) {}

const std::vector<BasicBlock*>& CFG::successors(BasicBlock* bb) const {
    static const std::vector<BasicBlock*> empty;
    auto it = successors_.find(bb);
    return it != successors_.end() ? it->second : empty;
}

const std::vector<BasicBlock*>& CFG::predecessors(BasicBlock* bb) const {
    static const std::vector<BasicBlock*> empty;
    auto it = predecessors_.find(bb);
    return it != predecessors_.end() ? it->second : empty;
}

bool CFG::is_reachable(BasicBlock* bb) const {
    return reachable_.count(bb) > 0;
}

CFG::BlockSet CFG::find_reachable(BasicBlock* entry, const BlockMap& succ) {
    BlockSet reachable;
    std::queue<BasicBlock*> worklist;
    worklist.push(entry);

    while (!worklist.empty()) {
        BasicBlock* bb = worklist.front();
        worklist.pop();
        if (!reachable.insert(bb).second) continue;

        auto it = succ.find(bb);
        if (it != succ.end()) {
            for (BasicBlock* s : it->second) {
                worklist.push(s);
            }
        }
    }
    return reachable;
}

ControlFlowGraph::ControlFlowGraph(const Function& function)
    : function_(function), entry_(function.entry_block()) {
    build();
}

const ControlFlowGraph::BlockList& ControlFlowGraph::successors(const BasicBlock& block) const {
    static const BlockList empty;
    auto it = successors_.find(&block);
    return it == successors_.end() ? empty : it->second;
}

const ControlFlowGraph::BlockList& ControlFlowGraph::predecessors(const BasicBlock& block) const {
    static const BlockList empty;
    auto it = predecessors_.find(&block);
    return it == predecessors_.end() ? empty : it->second;
}

bool ControlFlowGraph::isReachable(const BasicBlock& block) const {
    return reachable_.count(&block) != 0;
}

void ControlFlowGraph::build() {
    std::unordered_map<const Label*, const BasicBlock*> block_by_label;
    for (const auto& block : function_.blocks()) {
        const BasicBlock* bb = block.get();
        blocks_.push_back(bb);
        block_by_label[bb->label()] = bb;
        successors_[bb] = {};
        predecessors_[bb] = {};
    }

    for (const auto& block : function_.blocks()) {
        const BasicBlock* bb = block.get();
        for (const Label* target : successor_labels_const(bb)) {
            auto it = block_by_label.find(target);
            if (it == block_by_label.end()) {
                throw std::runtime_error("branch target does not exist: " + target->name());
            }
            auto& succ_list = successors_[bb];
            if (std::find(succ_list.begin(), succ_list.end(), it->second) == succ_list.end()) {
                succ_list.push_back(it->second);
            }
            auto& pred_list = predecessors_[it->second];
            if (std::find(pred_list.begin(), pred_list.end(), bb) == pred_list.end()) {
                pred_list.push_back(bb);
            }
        }
    }

    BlockSet visited;
    if (entry_ != nullptr) {
        dfsReachable(entry_, visited);
    }
    std::reverse(rpo_.begin(), rpo_.end());

    for (const BasicBlock* block : blocks_) {
        if (reachable_.count(block) == 0) {
            unreachable_.push_back(block);
        }
    }
}

void ControlFlowGraph::dfsReachable(const BasicBlock* block, BlockSet& visited) {
    if (!visited.insert(block).second) return;
    reachable_.insert(block);
    for (const BasicBlock* successor : successors(*block)) {
        dfsReachable(successor, visited);
    }
    rpo_.push_back(block);
}
