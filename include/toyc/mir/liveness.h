#pragma once
/// Liveness analysis for virtual registers.
/// This is a P0 placeholder — will be implemented in P8.

#include "toyc/support/ids.h"

#include <vector>

namespace toyc {

struct MIRFunction;

/// Liveness information for a MIR function.
struct LivenessInfo {
  /// For each block, the set of live-in virtual registers.
  std::vector<std::vector<VRegId>> liveIn;
  /// For each block, the set of live-out virtual registers.
  std::vector<std::vector<VRegId>> liveOut;

  /// Compute liveness for the given MIR function.
  /// P0 stub: does nothing.
  void compute(const MIRFunction& func);
};

} // namespace toyc
