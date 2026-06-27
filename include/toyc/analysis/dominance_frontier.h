#pragma once
/// Dominance frontier computation (needed for SSA construction / Mem2Reg).
/// This is a P0 placeholder — will be implemented in P6.

#include "toyc/support/ids.h"

#include <unordered_map>
#include <vector>

namespace toyc {

class DominatorTree;
class Function;

class DominanceFrontier {
public:
  DominanceFrontier(const Function& function, const DominatorTree& dominators);

  [[nodiscard]] const std::vector<BlockId>& frontier(BlockId block) const;

private:
  std::vector<BlockId> empty_;
  std::unordered_map<BlockId, std::vector<BlockId>> frontiers_;
};

} // namespace toyc
