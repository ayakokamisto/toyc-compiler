#pragma once
/// Dominator tree computation.
/// This is a P0 placeholder — will be implemented in P6.

#include "toyc/support/ids.h"

#include <optional>
#include <unordered_map>
#include <vector>

namespace toyc {

class Function;

std::vector<BlockId> computeReversePostOrder(const Function& function);

class DominatorTree {
public:
  explicit DominatorTree(const Function& function);

  [[nodiscard]] bool isReachable(BlockId block) const;
  [[nodiscard]] std::optional<BlockId> immediateDominator(BlockId block) const;
  [[nodiscard]] bool dominates(BlockId dominator, BlockId block) const;
  [[nodiscard]] const std::vector<BlockId>& children(BlockId block) const;
  [[nodiscard]] const std::vector<BlockId>& reversePostOrder() const { return rpo_; }

private:
  std::vector<BlockId> empty_;
  std::vector<BlockId> rpo_;
  std::unordered_map<BlockId, int> rpoIndex_;
  std::unordered_map<BlockId, std::optional<BlockId>> idom_;
  std::unordered_map<BlockId, std::vector<BlockId>> children_;
};

} // namespace toyc
