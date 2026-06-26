/// Analysis scaffold — P0 stub implementations.

#include "toyc/analysis/cfg.h"
#include "toyc/analysis/dominator_tree.h"
#include "toyc/analysis/dominance_frontier.h"
#include "toyc/analysis/loop_info.h"

namespace toyc {

void buildCFG(Function& /*func*/) {
  // P0 stub — CFG construction not yet implemented.
}

void DominatorTree::compute(const Function& /*func*/) {
  // P0 stub — dominator tree computation not yet implemented.
}

void DominanceFrontier::compute(const DominatorTree& /*domTree*/) {
  // P0 stub — dominance frontier not yet implemented.
}

void LoopAnalysis::analyze(const Function& /*func*/) {
  // P0 stub — loop analysis not yet implemented.
}

} // namespace toyc
