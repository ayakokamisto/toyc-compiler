#pragma once
/// Dominator tree computation.
/// This is a P0 placeholder — will be implemented in P6.

#include "toyc/support/ids.h"

#include <vector>

namespace toyc {

class Function;

/// Dominator tree for a function.
/// P0 stub: empty structure.
struct DominatorTree {
  /// For each block, its immediate dominator.
  std::vector<BlockId> idom;

  /// Compute dominator tree for the given function.
  /// P0 stub: does nothing.
  void compute(const Function& func);
};

} // namespace toyc
