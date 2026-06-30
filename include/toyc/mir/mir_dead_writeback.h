#pragma once
/// Block-local frame-slot dead writeback suppression.
///
/// Removes StoreFrame instructions to known local frame slots when a later
/// StoreFrame in the same basic block overwrites the same slot before any
/// conservative boundary that could observe the earlier write.

#include "toyc/mir/mir.h"

#include <string>
#include <vector>

namespace toyc {

struct DeadWritebackRemoval {
  std::string functionName;
  std::string blockLabel;
  int frameSlot = -1;
  int removedIndex = -1;
  int coveringIndex = -1;
};

struct BlockLocalDeadWritebackStats {
  int deadWritebacksRemoved = 0;
  int vregHomeWritebacksSuppressed = 0;
  std::vector<DeadWritebackRemoval> removals;
};

bool eliminateBlockLocalDeadWritebacks(MIRFunction& function,
                                       BlockLocalDeadWritebackStats* stats = nullptr);
bool eliminateBlockLocalDeadWritebacks(MIRModule& module,
                                       BlockLocalDeadWritebackStats* stats = nullptr);

} // namespace toyc
