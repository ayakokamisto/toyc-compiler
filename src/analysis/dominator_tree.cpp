#include "toyc/analysis/dominator_tree.h"

#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"

#include <algorithm>
#include <functional>
#include <unordered_set>

namespace toyc {

static const BasicBlock* findBlock(const Function& function, BlockId id) {
  for (const auto& block : function.blocks()) {
    if (block->id() == id) return block.get();
  }
  return nullptr;
}

std::vector<BlockId> computeReversePostOrder(const Function& function) {
  std::vector<BlockId> postorder;
  std::unordered_set<BlockId> visited;
  const auto* entry = function.entryBlock();
  if (!entry) return postorder;

  std::function<void(BlockId)> dfs = [&](BlockId id) {
    if (!visited.insert(id).second) return;
    const auto* block = findBlock(function, id);
    if (!block) return;
    auto successors = block->successors();
    std::sort(successors.begin(), successors.end());
    for (auto succ : successors) {
      dfs(succ);
    }
    postorder.push_back(id);
  };

  dfs(entry->id());
  std::reverse(postorder.begin(), postorder.end());
  return postorder;
}

static BlockId intersectIdom(BlockId lhs, BlockId rhs,
                             const std::unordered_map<BlockId, BlockId>& idom,
                             const std::unordered_map<BlockId, int>& rpoIndex) {
  while (lhs != rhs) {
    while (rpoIndex.at(lhs) > rpoIndex.at(rhs)) lhs = idom.at(lhs);
    while (rpoIndex.at(rhs) > rpoIndex.at(lhs)) rhs = idom.at(rhs);
  }
  return lhs;
}

DominatorTree::DominatorTree(const Function& function) {
  rpo_ = computeReversePostOrder(function);
  for (int i = 0; i < static_cast<int>(rpo_.size()); ++i) {
    rpoIndex_[rpo_[i]] = i;
  }
  if (rpo_.empty()) return;

  std::unordered_map<BlockId, BlockId> idomValues;
  BlockId entry = rpo_.front();
  idomValues[entry] = entry;

  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 1; i < rpo_.size(); ++i) {
      BlockId blockId = rpo_[i];
      const auto* block = findBlock(function, blockId);
      if (!block) continue;

      std::optional<BlockId> newIdom;
      auto preds = block->predecessors();
      std::sort(preds.begin(), preds.end());
      for (auto pred : preds) {
        if (!rpoIndex_.contains(pred) || !idomValues.contains(pred)) continue;
        if (!newIdom.has_value()) {
          newIdom = pred;
        } else {
          newIdom = intersectIdom(pred, *newIdom, idomValues, rpoIndex_);
        }
      }

      if (newIdom.has_value() &&
          (!idomValues.contains(blockId) || idomValues[blockId] != *newIdom)) {
        idomValues[blockId] = *newIdom;
        changed = true;
      }
    }
  }

  for (auto block : rpo_) {
    if (block == entry) {
      idom_[block] = std::nullopt;
    } else if (idomValues.contains(block)) {
      idom_[block] = idomValues[block];
      children_[idomValues[block]].push_back(block);
    }
  }
  for (auto& entryPair : children_) {
    std::sort(entryPair.second.begin(), entryPair.second.end());
  }
}

bool DominatorTree::isReachable(BlockId block) const {
  return rpoIndex_.contains(block);
}

std::optional<BlockId> DominatorTree::immediateDominator(BlockId block) const {
  auto it = idom_.find(block);
  if (it == idom_.end()) return std::nullopt;
  return it->second;
}

bool DominatorTree::dominates(BlockId dominator, BlockId block) const {
  if (!isReachable(dominator) || !isReachable(block)) return false;
  if (dominator == block) return true;
  auto cur = immediateDominator(block);
  while (cur.has_value()) {
    if (*cur == dominator) return true;
    cur = immediateDominator(*cur);
  }
  return false;
}

const std::vector<BlockId>& DominatorTree::children(BlockId block) const {
  auto it = children_.find(block);
  return it == children_.end() ? empty_ : it->second;
}

} // namespace toyc
