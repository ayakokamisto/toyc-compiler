#pragma once
/// Loop information analysis (for LICM and other loop optimizations).
/// This is a P0 placeholder — will be implemented in P8.

#include "toyc/support/ids.h"

#include <vector>

namespace toyc {

class Function;

/// Information about a single natural loop.
struct LoopInfo {
  BlockId header;                 ///< Loop header block.
  std::vector<BlockId> blocks;    ///< All blocks in the loop.
  std::vector<BlockId> latches;   ///< Back-edge source blocks.
};

/// Loop analysis result for a function.
struct LoopAnalysis {
  std::vector<LoopInfo> loops;

  /// Analyze loops in the function.
  /// P0 stub: does nothing.
  void analyze(const Function& func);
};

} // namespace toyc
