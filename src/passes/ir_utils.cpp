#include "toyc/passes/ir_utils.h"

#include "toyc/analysis/cfg.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <unordered_map>

namespace toyc {

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

static bool fitsI32(int64_t value) {
  return value >= static_cast<int64_t>(INT32_MIN) && value <= static_cast<int64_t>(INT32_MAX);
}

FoldResult foldUnary(UnaryOpcode op, int32_t operand) {
  switch (op) {
    case UnaryOpcode::Negate: {
      int64_t value = -static_cast<int64_t>(operand);
      if (!fitsI32(value)) return {};
      return {true, static_cast<int32_t>(value)};
    }
    case UnaryOpcode::LogicalNot:
      return {true, operand == 0 ? 1 : 0};
  }
  return {};
}

FoldResult foldBinary(BinaryOpcode op, int32_t lhs, int32_t rhs) {
  int64_t l = lhs;
  int64_t r = rhs;
  switch (op) {
    case BinaryOpcode::Add: {
      auto value = l + r;
      return fitsI32(value) ? FoldResult{true, static_cast<int32_t>(value)} : FoldResult{};
    }
    case BinaryOpcode::Subtract: {
      auto value = l - r;
      return fitsI32(value) ? FoldResult{true, static_cast<int32_t>(value)} : FoldResult{};
    }
    case BinaryOpcode::Multiply: {
      auto value = l * r;
      return fitsI32(value) ? FoldResult{true, static_cast<int32_t>(value)} : FoldResult{};
    }
    case BinaryOpcode::Divide:
      if (rhs == 0 || (lhs == INT32_MIN && rhs == -1)) return {};
      return {true, static_cast<int32_t>(l / r)};
    case BinaryOpcode::Modulo:
      if (rhs == 0 || (lhs == INT32_MIN && rhs == -1)) return {};
      return {true, static_cast<int32_t>(l % r)};
  }
  return {};
}

FoldResult foldCompare(ComparePredicate pred, int32_t lhs, int32_t rhs) {
  switch (pred) {
    case ComparePredicate::Equal:        return {true, lhs == rhs ? 1 : 0};
    case ComparePredicate::NotEqual:     return {true, lhs != rhs ? 1 : 0};
    case ComparePredicate::Less:         return {true, lhs < rhs ? 1 : 0};
    case ComparePredicate::LessEqual:    return {true, lhs <= rhs ? 1 : 0};
    case ComparePredicate::Greater:      return {true, lhs > rhs ? 1 : 0};
    case ComparePredicate::GreaterEqual: return {true, lhs >= rhs ? 1 : 0};
  }
  return {};
}

std::vector<ValueId> instructionOperands(const Inst& inst) {
  switch (inst.opcode) {
    case Opcode::Phi: {
      std::vector<ValueId> values;
      values.reserve(inst.phiIncoming.size());
      for (const auto& incoming : inst.phiIncoming) values.push_back(incoming.value);
      return values;
    }
    case Opcode::SlotStore:
    case Opcode::GlobalStore:
      return {inst.lhs};
    case Opcode::Unary:
      return {inst.unaryOperand};
    case Opcode::Binary:
      return {inst.lhs, inst.rhs};
    case Opcode::Compare:
      return {inst.cmpLhs, inst.cmpRhs};
    case Opcode::Call:
      return inst.arguments;
    default:
      return {};
  }
}

static bool rewriteValue(ValueId& value, const std::unordered_map<ValueId, ValueId>& replacements) {
  auto current = value;
  std::unordered_set<ValueId> seen;
  while (replacements.contains(current) && seen.insert(current).second) {
    current = replacements.at(current);
  }
  if (current == value) return false;
  value = current;
  return true;
}

static bool rewriteInst(Inst& inst, const std::unordered_map<ValueId, ValueId>& replacements) {
  bool changed = false;
  switch (inst.opcode) {
    case Opcode::Phi:
      for (auto& incoming : inst.phiIncoming) changed |= rewriteValue(incoming.value, replacements);
      break;
    case Opcode::SlotStore:
    case Opcode::GlobalStore:
      changed |= rewriteValue(inst.lhs, replacements);
      break;
    case Opcode::Unary:
      changed |= rewriteValue(inst.unaryOperand, replacements);
      break;
    case Opcode::Binary:
      changed |= rewriteValue(inst.lhs, replacements);
      changed |= rewriteValue(inst.rhs, replacements);
      break;
    case Opcode::Compare:
      changed |= rewriteValue(inst.cmpLhs, replacements);
      changed |= rewriteValue(inst.cmpRhs, replacements);
      break;
    case Opcode::Call:
      for (auto& arg : inst.arguments) changed |= rewriteValue(arg, replacements);
      break;
    default:
      break;
  }
  return changed;
}

static bool rewriteTerminator(Terminator& term, const std::unordered_map<ValueId, ValueId>& replacements) {
  if (term.opcode == Opcode::CondBr) {
    return rewriteValue(term.condCondition, replacements);
  }
  if (term.opcode == Opcode::Ret && term.returnValue.has_value()) {
    auto value = *term.returnValue;
    bool changed = rewriteValue(value, replacements);
    term.returnValue = value;
    return changed;
  }
  return false;
}

bool replaceAllUses(Function& function, const std::unordered_map<ValueId, ValueId>& replacements) {
  bool changed = false;
  for (auto& block : function.blocks()) {
    for (auto& inst : block->mutableInstructions()) {
      changed |= rewriteInst(*inst, replacements);
    }
    if (auto* term = block->mutableTerminator()) {
      changed |= rewriteTerminator(*term, replacements);
    }
  }
  return changed;
}

bool replaceUse(Function& function, ValueId oldValue, ValueId newValue) {
  return replaceAllUses(function, {{oldValue, newValue}});
}

bool instructionHasSideEffects(const Inst& inst) {
  return inst.opcode == Opcode::SlotStore || inst.opcode == Opcode::GlobalStore ||
         inst.opcode == Opcode::Call;
}

bool instructionIsRemovable(const Inst& inst) {
  switch (inst.opcode) {
    case Opcode::Phi:
    case Opcode::ConstInt:
    case Opcode::Unary:
    case Opcode::Binary:
    case Opcode::Compare:
    case Opcode::GlobalLoad:
      return true;
    default:
      return false;
  }
}

std::unordered_set<ValueId> collectUsedValues(const Function& function) {
  std::unordered_set<ValueId> used;
  for (const auto& block : function.blocks()) {
    for (const auto& inst : block->instructions()) {
      for (auto value : instructionOperands(*inst)) used.insert(value);
    }
    if (block->hasTerminator()) {
      const auto& term = *block->terminator();
      if (term.opcode == Opcode::CondBr) used.insert(term.condCondition);
      if (term.opcode == Opcode::Ret && term.returnValue.has_value()) used.insert(*term.returnValue);
    }
  }
  return used;
}

std::optional<int32_t> constValueOf(const Function& function, ValueId value) {
  for (const auto& block : function.blocks()) {
    for (const auto& inst : block->instructions()) {
      if (inst->opcode == Opcode::ConstInt && inst->result == value) return inst->constValue;
    }
  }
  return std::nullopt;
}

ValueId appendConst(BasicBlock& block, Function& function, int32_t value) {
  auto result = function.createInstValue();
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::ConstInt;
  inst->resultType = I32Type;
  inst->result = result;
  inst->constValue = value;
  block.appendInst(std::move(inst));
  return result;
}

bool foldGlobalConstantLoads(Module& module) {
  bool changed = false;
  for (const auto& func : module.functions()) {
    for (auto& block : func->blocks()) {
      for (auto& inst : block->mutableInstructions()) {
        if (inst->opcode != Opcode::GlobalLoad || !inst->result.has_value()) continue;
        const auto* global = module.findGlobal(inst->global);
        if (!global || global->kind != GlobalKind::Constant ||
            global->initKind != IRGlobalInitKind::Static) {
          continue;
        }
        inst->opcode = Opcode::ConstInt;
        inst->constValue = global->staticInitialValue;
        changed = true;
      }
    }
  }
  return changed;
}

namespace {

struct SlotUseInfo {
  int stores = 0;
  int loads = 0;
  BasicBlock* storeBlock = nullptr;
  BasicBlock* loadBlock = nullptr;
  std::size_t storeIndex = 0;
  std::size_t loadIndex = 0;
  ValueId storedValue;
  ValueId loadedValue;
};

static std::unordered_map<uint32_t, SlotUseInfo> collectSlotUseInfo(Function& function) {
  std::unordered_map<uint32_t, SlotUseInfo> slots;
  for (auto& block : function.blocks()) {
    auto& insts = block->mutableInstructions();
    for (std::size_t i = 0; i < insts.size(); ++i) {
      auto& inst = *insts[i];
      if (inst.opcode == Opcode::SlotStore) {
        auto& info = slots[inst.slot.value];
        ++info.stores;
        info.storeBlock = block.get();
        info.storeIndex = i;
        info.storedValue = inst.lhs;
      } else if (inst.opcode == Opcode::SlotLoad && inst.result.has_value()) {
        auto& info = slots[inst.slot.value];
        ++info.loads;
        info.loadBlock = block.get();
        info.loadIndex = i;
        info.loadedValue = *inst.result;
      }
    }
  }
  return slots;
}

static bool eraseInstructionsForSlots(Function& function, const std::unordered_set<uint32_t>& slotIds) {
  bool changed = false;
  for (auto& block : function.blocks()) {
    auto& insts = block->mutableInstructions();
    const auto oldSize = insts.size();
    insts.erase(std::remove_if(insts.begin(), insts.end(), [&](const auto& inst) {
                  return (inst->opcode == Opcode::SlotStore || inst->opcode == Opcode::SlotLoad) &&
                         slotIds.contains(inst->slot.value);
                }),
                insts.end());
    changed = changed || insts.size() != oldSize;
  }
  return changed;
}

static bool eraseDeadPureInstructions(Function& function) {
  bool changed = false;
  bool localChanged = true;
  while (localChanged) {
    localChanged = false;
    auto used = collectUsedValues(function);
    std::vector<ValueId> erased;
    for (auto& block : function.blocks()) {
      auto& insts = block->mutableInstructions();
      const auto oldSize = insts.size();
      insts.erase(std::remove_if(insts.begin(), insts.end(), [&](const auto& inst) {
                    if (!inst->result.has_value()) return false;
                    if (used.contains(*inst->result)) return false;
                    if (!instructionIsRemovable(*inst)) return false;
                    erased.push_back(*inst->result);
                    return true;
                  }),
                  insts.end());
      localChanged = localChanged || insts.size() != oldSize;
    }
    if (!erased.empty()) function.eraseValues(erased);
    changed = changed || localChanged;
  }
  return changed;
}

static bool isTemporarySlot(const Function& function, uint32_t slotId) {
  for (const auto& slot : function.slots()) {
    if (slot.id.value == slotId) return slot.kind == SlotKind::Temporary;
  }
  return false;
}

static std::unique_ptr<Inst> makeSlotStore(SlotId slot, ValueId value) {
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::SlotStore;
  inst->resultType = VoidIRType;
  inst->slot = slot;
  inst->lhs = value;
  return inst;
}

} // namespace

bool cleanupCanonicalSlots(Function& function) {
  if (function.form() != IRForm::CanonicalSlot) return false;

  bool changed = false;
  bool localChanged = true;
  while (localChanged) {
    localChanged = false;
    auto info = collectSlotUseInfo(function);
    std::unordered_map<ValueId, ValueId> replacements;
    std::unordered_set<uint32_t> eraseSlots;
    std::vector<ValueId> eraseValues;
    std::vector<SlotId> eraseSlotIds;

    for (const auto& [slotId, use] : info) {
      if (!isTemporarySlot(function, slotId)) continue;
      if (use.loads == 0 && use.stores > 0) {
        eraseSlots.insert(slotId);
        eraseSlotIds.push_back(SlotId(slotId));
        continue;
      }
      if (use.loads == 1 && use.stores == 1 && use.loadBlock == use.storeBlock &&
          use.storeIndex < use.loadIndex) {
        replacements[use.loadedValue] = use.storedValue;
        eraseValues.push_back(use.loadedValue);
        eraseSlots.insert(slotId);
        eraseSlotIds.push_back(SlotId(slotId));
      }
    }

    if (!replacements.empty()) {
      localChanged |= replaceAllUses(function, replacements);
      function.eraseValues(eraseValues);
    }
    if (!eraseSlots.empty()) {
      localChanged |= eraseInstructionsForSlots(function, eraseSlots);
      function.eraseSlots(eraseSlotIds);
    }
    localChanged |= eraseDeadPureInstructions(function);
    changed = changed || localChanged;
  }
  rebuildCFG(function);
  return changed;
}

bool eliminateSelfTailRecursion(Function& function) {
  if (function.form() != IRForm::CanonicalSlot || function.params().empty()) return false;

  std::vector<std::pair<BasicBlock*, std::size_t>> tailCalls;
  for (auto& block : function.blocks()) {
    auto* term = block->mutableTerminator();
    if (!term || term->opcode != Opcode::Ret || !term->returnValue.has_value()) continue;
    auto& insts = block->mutableInstructions();
    if (insts.empty()) continue;
    for (std::size_t i = insts.size(); i > 0; --i) {
      auto& inst = *insts[i - 1];
      if (!inst.result.has_value()) continue;
      if (*inst.result != *term->returnValue) continue;
      if (inst.opcode == Opcode::Call && inst.callee == function.id() &&
          inst.arguments.size() == function.params().size()) {
        tailCalls.push_back({block.get(), i - 1});
      }
      break;
    }
  }
  if (tailCalls.empty()) return false;

  auto* entry = function.entryBlock();
  if (!entry) return false;

  const auto originalEntryId = entry->id();
  auto* loopEntry = function.createBlock("tail.entry");
  const auto loopEntryId = loopEntry->id();
  auto& blocks = function.mutableBlocks();
  if (blocks.size() > 2 && blocks.back().get() == loopEntry) {
    auto movedLoopEntry = std::move(blocks.back());
    blocks.pop_back();
    blocks.insert(blocks.begin() + 1, std::move(movedLoopEntry));
  }

  auto& entryInsts = entry->mutableInstructions();
  std::size_t paramPrefix = 0;
  while (paramPrefix < entryInsts.size() && paramPrefix < function.params().size()) {
    const auto& inst = *entryInsts[paramPrefix];
    const auto& param = function.params()[paramPrefix];
    if (inst.opcode != Opcode::SlotStore || inst.slot != param.slotId ||
        inst.lhs != param.valueId) {
      break;
    }
    ++paramPrefix;
  }

  auto& loopInsts = loopEntry->mutableInstructions();
  for (std::size_t i = paramPrefix; i < entryInsts.size(); ++i) {
    loopInsts.push_back(std::move(entryInsts[i]));
  }
  entryInsts.erase(entryInsts.begin() + static_cast<std::ptrdiff_t>(paramPrefix), entryInsts.end());
  if (entry->hasTerminator()) {
    loopEntry->setTerminator(*entry->terminator());
    entry->clearTerminator();
  }
  Terminator jumpToLoop;
  jumpToLoop.opcode = Opcode::Br;
  jumpToLoop.branchTarget = loopEntryId;
  entry->setTerminator(jumpToLoop);

  for (auto& block : function.blocks()) {
    if (block.get() == entry) continue;
    if (auto* term = block->mutableTerminator()) {
      rewriteTerminatorTarget(*term, originalEntryId, loopEntryId);
    }
  }

  std::vector<ValueId> erasedValues;
  for (auto [block, index] : tailCalls) {
    if (block == entry) {
      block = loopEntry;
      if (index < paramPrefix) continue;
      index -= paramPrefix;
    }
    auto& insts = block->mutableInstructions();
    if (index >= insts.size()) continue;
    auto& call = *insts[index];
    if (call.opcode != Opcode::Call || call.callee != function.id() || !call.result.has_value()) continue;
    const auto args = call.arguments;
    erasedValues.push_back(*call.result);
    insts.erase(insts.begin() + static_cast<std::ptrdiff_t>(index));
    for (std::size_t i = 0; i < function.params().size(); ++i) {
      insts.push_back(makeSlotStore(function.params()[i].slotId, args[i]));
    }
    block->clearTerminator();
    Terminator jump;
    jump.opcode = Opcode::Br;
    jump.branchTarget = loopEntryId;
    block->setTerminator(jump);
  }

  function.eraseValues(erasedValues);
  rebuildCFG(function);
  return true;
}

void rewriteTerminatorTarget(Terminator& term, BlockId oldTarget, BlockId newTarget) {
  if (term.opcode == Opcode::Br && term.branchTarget == oldTarget) {
    term.branchTarget = newTarget;
  } else if (term.opcode == Opcode::CondBr) {
    if (term.condTrueTarget == oldTarget) term.condTrueTarget = newTarget;
    if (term.condFalseTarget == oldTarget) term.condFalseTarget = newTarget;
  }
}

bool removeUnreachableBlocks(Function& function) {
  if (!function.entryBlock()) return false;
  rebuildCFG(function);
  std::unordered_set<BlockId> reachable;
  std::function<void(BlockId)> visit = [&](BlockId id) {
    if (!reachable.insert(id).second) return;
    const auto* block = findBlock(function, id);
    if (!block) return;
    auto succs = block->successors();
    std::sort(succs.begin(), succs.end());
    for (auto succ : succs) visit(succ);
  };
  visit(function.entryBlock()->id());

  std::vector<BlockId> toErase;
  for (const auto& block : function.blocks()) {
    if (!reachable.contains(block->id())) toErase.push_back(block->id());
  }
  if (toErase.empty()) return false;

  for (auto& block : function.blocks()) {
    if (!reachable.contains(block->id()) || !block->hasTerminator()) continue;
    const auto& succs = block->successors();
    for (auto succ : succs) {
      (void)succ;
    }
  }

  std::vector<ValueId> deadValues;
  for (auto blockId : toErase) {
    const auto* block = findBlock(function, blockId);
    if (!block) continue;
    for (const auto& inst : block->instructions()) {
      if (inst->result.has_value()) deadValues.push_back(*inst->result);
    }
  }
  for (auto blockId : toErase) {
    (void)function.eraseBlock(blockId);
  }
  std::unordered_set<BlockId> erasedBlocks(toErase.begin(), toErase.end());
  for (auto& block : function.blocks()) {
    for (auto& inst : block->mutableInstructions()) {
      if (inst->opcode != Opcode::Phi) continue;
      inst->phiIncoming.erase(std::remove_if(inst->phiIncoming.begin(), inst->phiIncoming.end(),
                                             [&](const PhiIncoming& incoming) {
                                               return erasedBlocks.contains(incoming.predecessor);
                                             }),
                              inst->phiIncoming.end());
    }
  }
  function.eraseValues(deadValues);
  rebuildCFG(function);
  return true;
}

} // namespace toyc
