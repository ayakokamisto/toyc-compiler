#include "toyc/passes/simplify_cfg.h"

#include "toyc/analysis/cfg.h"
#include "toyc/passes/ir_utils.h"

#include <algorithm>

namespace toyc {

static bool hasPhi(const BasicBlock& block) {
  for (const auto& inst : block.instructions()) {
    if (inst->opcode == Opcode::Phi) return true;
  }
  return false;
}

static bool simplifyBranches(Function& function) {
  bool changed = false;
  for (auto& block : function.blocks()) {
    auto* term = block->mutableTerminator();
    if (!term || term->opcode != Opcode::CondBr) continue;
    if (term->condTrueTarget == term->condFalseTarget) {
      Terminator br;
      br.opcode = Opcode::Br;
      br.branchTarget = term->condTrueTarget;
      block->setTerminator(br);
      changed = true;
      continue;
    }
    auto cond = constValueOf(function, term->condCondition);
    if (!cond.has_value()) continue;
    Terminator br;
    br.opcode = Opcode::Br;
    br.branchTarget = *cond != 0 ? term->condTrueTarget : term->condFalseTarget;
    block->setTerminator(br);
    changed = true;
  }
  if (changed) rebuildCFG(function);
  return changed;
}

static bool removeTrampolines(Function& function) {
  rebuildCFG(function);
  bool changed = false;
  for (auto& blockPtr : function.blocks()) {
    auto& block = *blockPtr;
    if (&block == function.entryBlock()) continue;
    if (!block.instructions().empty() || !block.hasTerminator()) continue;
    const auto& term = *block.terminator();
    if (term.opcode != Opcode::Br) continue;
    auto* target = findBlock(function, term.branchTarget);
    if (!target || hasPhi(*target)) continue;
    auto preds = block.predecessors();
    if (preds.empty()) continue;
    for (auto predId : preds) {
      auto* pred = findBlock(function, predId);
      if (!pred || !pred->mutableTerminator()) continue;
      rewriteTerminatorTarget(*pred->mutableTerminator(), block.id(), term.branchTarget);
    }
    changed = true;
  }
  if (changed) {
    rebuildCFG(function);
    changed |= removeUnreachableBlocks(function);
  }
  return changed;
}

static bool mergeLinearBlocks(Function& function) {
  rebuildCFG(function);
  for (auto& blockPtr : function.blocks()) {
    auto& block = *blockPtr;
    if (!block.hasTerminator()) continue;
    auto* term = block.mutableTerminator();
    if (term->opcode != Opcode::Br) continue;
    auto* succ = findBlock(function, term->branchTarget);
    if (!succ || succ == &block || succ == function.entryBlock()) continue;
    if (block.successors().size() != 1 || succ->predecessors().size() != 1 || hasPhi(*succ)) continue;

    block.clearTerminator();
    auto& src = succ->mutableInstructions();
    auto& dst = block.mutableInstructions();
    for (auto& inst : src) {
      dst.push_back(std::move(inst));
    }
    src.clear();
    if (succ->hasTerminator()) block.setTerminator(*succ->terminator());
    function.eraseBlock(succ->id());
    rebuildCFG(function);
    return true;
  }
  return false;
}

PassResult SimplifyCFGPass::run(Function& function) {
  if (function.form() != IRForm::SSA) return {};
  bool changed = false;
  changed |= removeUnreachableBlocks(function);
  changed |= simplifyBranches(function);
  changed |= removeUnreachableBlocks(function);
  changed |= removeTrampolines(function);
  while (mergeLinearBlocks(function)) changed = true;
  changed |= removeUnreachableBlocks(function);
  return {changed};
}

} // namespace toyc
