#include "toyc/analysis/dominance_frontier.h"

#include "toyc/analysis/dominator_tree.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"

#include <algorithm>

namespace toyc {

DominanceFrontier::DominanceFrontier(const Function& function, const DominatorTree& dominators) {
  for (const auto& blockPtr : function.blocks()) {
    const auto& block = *blockPtr;
    if (!dominators.isReachable(block.id()) || block.predecessors().size() < 2) continue;

    auto preds = block.predecessors();
    std::sort(preds.begin(), preds.end());
    for (auto pred : preds) {
      if (!dominators.isReachable(pred)) continue;
      auto runner = pred;
      auto stop = dominators.immediateDominator(block.id());
      while (!stop.has_value() || runner != *stop) {
        auto& frontier = frontiers_[runner];
        if (std::find(frontier.begin(), frontier.end(), block.id()) == frontier.end()) {
          frontier.push_back(block.id());
        }
        auto next = dominators.immediateDominator(runner);
        if (!next.has_value()) break;
        runner = *next;
      }
    }
  }

  for (auto& entryPair : frontiers_) {
    std::sort(entryPair.second.begin(), entryPair.second.end());
  }
}

const std::vector<BlockId>& DominanceFrontier::frontier(BlockId block) const {
  auto it = frontiers_.find(block);
  return it == frontiers_.end() ? empty_ : it->second;
}

} // namespace toyc
