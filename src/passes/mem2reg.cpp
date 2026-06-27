#include "toyc/passes/mem2reg.h"

#include "toyc/analysis/dominance_frontier.h"
#include "toyc/analysis/dominator_tree.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/opcode.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace toyc {

namespace {

using SlotSet = std::unordered_set<SlotId>;
using BlockSet = std::unordered_set<BlockId>;

BasicBlock* findBlock(Function& function, BlockId id) {
  for (auto& block : function.blocks()) {
    if (block->id() == id) return block.get();
  }
  return nullptr;
}

const BasicBlock* findBlock(const Function& function, BlockId id) {
  for (const auto& block : function.blocks()) {
    if (block->id() == id) return block.get();
  }
  return nullptr;
}

bool isPromotable(const Slot& slot) {
  return slot.type.isI32() &&
         (slot.kind == SlotKind::Parameter || slot.kind == SlotKind::LocalVariable ||
          slot.kind == SlotKind::Temporary);
}

ValueId resolve(ValueId value, const std::unordered_map<ValueId, ValueId>& replacements) {
  auto current = value;
  std::unordered_set<ValueId> seen;
  while (replacements.contains(current) && seen.insert(current).second) {
    current = replacements.at(current);
  }
  return current;
}

void rewriteValue(ValueId& value, const std::unordered_map<ValueId, ValueId>& replacements) {
  value = resolve(value, replacements);
}

void rewriteInst(Inst& inst, const std::unordered_map<ValueId, ValueId>& replacements) {
  switch (inst.opcode) {
    case Opcode::SlotStore:
    case Opcode::GlobalStore:
      rewriteValue(inst.lhs, replacements);
      break;
    case Opcode::Unary:
      rewriteValue(inst.unaryOperand, replacements);
      break;
    case Opcode::Binary:
      rewriteValue(inst.lhs, replacements);
      rewriteValue(inst.rhs, replacements);
      break;
    case Opcode::Compare:
      rewriteValue(inst.cmpLhs, replacements);
      rewriteValue(inst.cmpRhs, replacements);
      break;
    case Opcode::Call:
      for (auto& arg : inst.arguments) rewriteValue(arg, replacements);
      break;
    default:
      break;
  }
}

void rewriteTerminator(Terminator& term, const std::unordered_map<ValueId, ValueId>& replacements) {
  if (term.opcode == Opcode::CondBr) {
    rewriteValue(term.condCondition, replacements);
  } else if (term.opcode == Opcode::Ret && term.returnValue.has_value()) {
    auto value = *term.returnValue;
    rewriteValue(value, replacements);
    term.returnValue = value;
  }
}

bool hasUseBeforeDef(const BasicBlock& block, SlotId slot) {
  bool hasDef = false;
  for (const auto& inst : block.instructions()) {
    if (inst->opcode == Opcode::SlotStore && inst->slot == slot) {
      hasDef = true;
    } else if (inst->opcode == Opcode::SlotLoad && inst->slot == slot && !hasDef) {
      return true;
    }
  }
  return false;
}

} // namespace

Mem2RegResult Mem2RegPass::run(Function& function) {
  Mem2RegResult result;
  if (function.form() == IRForm::SSA) return result;

  DominatorTree dom(function);
  DominanceFrontier df(function, dom);

  SlotSet promotable;
  for (const auto& slot : function.slots()) {
    if (isPromotable(slot)) promotable.insert(slot.id);
  }
  if (promotable.empty()) {
    function.setForm(IRForm::SSA);
    return result;
  }

  std::unordered_map<SlotId, BlockSet> defBlocks;
  std::unordered_map<SlotId, BlockSet> useBlocks;
  for (const auto& blockPtr : function.blocks()) {
    const auto& block = *blockPtr;
    if (!dom.isReachable(block.id())) continue;
    for (const auto& inst : block.instructions()) {
      if (inst->opcode == Opcode::SlotStore && promotable.contains(inst->slot)) {
        defBlocks[inst->slot].insert(block.id());
      } else if (inst->opcode == Opcode::SlotLoad && promotable.contains(inst->slot)) {
        useBlocks[inst->slot].insert(block.id());
      }
    }
  }

  std::unordered_map<SlotId, BlockSet> liveIn;
  for (auto slot : promotable) {
    std::vector<BlockId> worklist;
    for (auto useBlock : useBlocks[slot]) {
      const auto* block = findBlock(function, useBlock);
      if (block && hasUseBeforeDef(*block, slot)) {
        liveIn[slot].insert(useBlock);
        worklist.push_back(useBlock);
      }
    }
    while (!worklist.empty()) {
      auto blockId = worklist.back();
      worklist.pop_back();
      const auto* block = findBlock(function, blockId);
      if (!block) continue;
      auto preds = block->predecessors();
      std::sort(preds.begin(), preds.end());
      for (auto pred : preds) {
        if (!dom.isReachable(pred) || defBlocks[slot].contains(pred)) continue;
        if (liveIn[slot].insert(pred).second) {
          worklist.push_back(pred);
        }
      }
    }
  }

  IRBuilder builder;
  builder.setFunction(&function);
  std::unordered_map<BlockId, std::unordered_map<SlotId, Inst*>> blockPhis;

  for (auto slot : promotable) {
    if (useBlocks[slot].empty()) continue;
    std::vector<BlockId> worklist(defBlocks[slot].begin(), defBlocks[slot].end());
    std::sort(worklist.begin(), worklist.end());
    BlockSet hasAlready;
    while (!worklist.empty()) {
      auto blockId = worklist.back();
      worklist.pop_back();
      for (auto frontierBlock : df.frontier(blockId)) {
        if (!liveIn[slot].contains(frontierBlock) || hasAlready.contains(frontierBlock)) continue;
        auto* phi = builder.createPhi(frontierBlock, I32Type);
        blockPhis[frontierBlock][slot] = phi;
        hasAlready.insert(frontierBlock);
        ++result.insertedPhiCount;
        if (!defBlocks[slot].contains(frontierBlock)) {
          worklist.push_back(frontierBlock);
        }
      }
    }
  }

  std::unordered_map<SlotId, std::vector<ValueId>> stacks;
  std::unordered_map<ValueId, ValueId> replacements;

  auto pushValue = [&](SlotId slot, ValueId value) {
    stacks[slot].push_back(resolve(value, replacements));
  };

  auto stackTop = [&](SlotId slot) -> ValueId {
    auto it = stacks.find(slot);
    if (it == stacks.end() || it->second.empty()) {
      throw std::runtime_error("Mem2Reg read from undefined promoted slot");
    }
    return it->second.back();
  };

  auto rename = [&](auto&& self, BlockId blockId) -> void {
    auto* block = findBlock(function, blockId);
    if (!block) return;
    std::vector<SlotId> pushed;

    if (blockPhis.contains(blockId)) {
      std::vector<SlotId> slots;
      for (const auto& entry : blockPhis[blockId]) slots.push_back(entry.first);
      std::sort(slots.begin(), slots.end());
      for (auto slot : slots) {
        auto* phi = blockPhis[blockId][slot];
        pushValue(slot, *phi->result);
        pushed.push_back(slot);
      }
    }

    auto& insts = block->mutableInstructions();
    std::vector<std::unique_ptr<Inst>> kept;
    kept.reserve(insts.size());
    for (auto& inst : insts) {
      if (inst->opcode == Opcode::Phi) {
        kept.push_back(std::move(inst));
        continue;
      }
      rewriteInst(*inst, replacements);
      if (inst->opcode == Opcode::SlotLoad && promotable.contains(inst->slot)) {
        replacements[*inst->result] = stackTop(inst->slot);
        ++result.removedLoadCount;
        continue;
      }
      if (inst->opcode == Opcode::SlotStore && promotable.contains(inst->slot)) {
        pushValue(inst->slot, inst->lhs);
        pushed.push_back(inst->slot);
        ++result.removedStoreCount;
        continue;
      }
      kept.push_back(std::move(inst));
    }
    insts = std::move(kept);

    if (auto* term = block->mutableTerminator()) {
      rewriteTerminator(*term, replacements);
    }

    auto succs = block->successors();
    std::sort(succs.begin(), succs.end());
    for (auto succ : succs) {
      if (!blockPhis.contains(succ)) continue;
      std::vector<SlotId> slots;
      for (const auto& entry : blockPhis[succ]) slots.push_back(entry.first);
      std::sort(slots.begin(), slots.end());
      for (auto slot : slots) {
        builder.addPhiIncoming(*blockPhis[succ][slot], blockId, stackTop(slot));
      }
    }

    for (auto child : dom.children(blockId)) {
      self(self, child);
    }

    for (auto it = pushed.rbegin(); it != pushed.rend(); ++it) {
      stacks[*it].pop_back();
    }
  };

  if (function.entryBlock()) {
    rename(rename, function.entryBlock()->id());
  }

  std::vector<SlotId> promotedSlots(promotable.begin(), promotable.end());
  std::sort(promotedSlots.begin(), promotedSlots.end());
  function.eraseSlots(promotedSlots);
  function.setForm(IRForm::SSA);

  result.promotedSlotCount = promotedSlots.size();
  result.changed = result.promotedSlotCount > 0 || result.insertedPhiCount > 0 ||
                   result.removedLoadCount > 0 || result.removedStoreCount > 0;
  return result;
}

} // namespace toyc
