#pragma once

#include "alloca_promotability.h"
#include "control_flow_graph.h"
#include "def_use.h"
#include "dominator_tree.h"

#include <vector>

struct Mem2RegResult {
    bool changed = false;
    std::vector<const Value*> promotedAllocas;
    std::vector<const Value*> skippedAllocas;
    std::vector<std::string> diagnostics;
};

Mem2RegResult promoteMemToReg(
    Function& function,
    const ControlFlowGraph& cfg,
    const DominatorTree& domTree,
    const DefUseIndex& defUse,
    const AllocaPromotabilityAnalysis& promotability);
