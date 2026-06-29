#pragma once
/// MIR Liveness Analysis — backward dataflow + live intervals for register allocation.

#include "toyc/mir/mir.h"

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace toyc {

struct LiveInterval {
  uint32_t vreg;       // VRegId.value
  int start = 0;
  int end = 0;
  int useCount = 0;
  int spillWeight = 0;   // loop-depth-weighted use count + call penalty
  int callCrossingCount = 0;
  bool loopCarried = false;
};

struct MIRLiveness {
  std::vector<LiveInterval> intervals;
  std::unordered_map<uint32_t, int> blockLoopDepths;  // BlockId.value → loop depth
  std::unordered_set<uint32_t> loopCarriedVRegs;
  std::unordered_set<uint32_t> allVRegs;
  int maxOutgoingArgCount = 0;
};

/// Analyze liveness for a MIR function, producing live intervals.
MIRLiveness analyzeMIRLiveness(const MIRFunction& func);

} // namespace toyc
