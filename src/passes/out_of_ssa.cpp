#include "toyc/passes/out_of_ssa.h"

#include "toyc/analysis/cfg.h"
#include "toyc/passes/ir_utils.h"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

namespace toyc {

namespace {

struct EdgeStore {
  SlotId slot;
  ValueId value;
};

struct PhiLowering {
  BlockId block;
  ValueId phiValue;
  SlotId slot;
  std::vector<PhiIncoming> incoming;
};

static std::unique_ptr<Inst> makeStore(SlotId slot, ValueId value) {
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::SlotStore;
  inst->resultType = VoidIRType;
  inst->slot = slot;
  inst->lhs = value;
  return inst;
}

static std::unique_ptr<Inst> makeLoad(Function& function, SlotId slot, ValueId result) {
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::SlotLoad;
  inst->resultType = I32Type;
  inst->result = result;
  inst->slot = slot;
  return inst;
}

static bool edgeNeedsSplit(const Function& function, BlockId pred, BlockId succ) {
  const auto* predBlock = findBlock(function, pred);
  const auto* succBlock = findBlock(function, succ);
  return predBlock && succBlock && predBlock->successors().size() > 1 && succBlock->predecessors().size() > 1;
}

static BlockId splitEdge(Function& function, BlockId pred, BlockId succ, std::size_t index) {
  auto* edgeBlock = function.createBlock("phi.edge." + std::to_string(pred.value) + "." +
                                         std::to_string(succ.value) + "." + std::to_string(index));
  auto* predBlock = findBlock(function, pred);
  if (predBlock && predBlock->mutableTerminator()) {
    rewriteTerminatorTarget(*predBlock->mutableTerminator(), succ, edgeBlock->id());
  }
  Terminator br;
  br.opcode = Opcode::Br;
  br.branchTarget = succ;
  edgeBlock->setTerminator(br);
  rebuildCFG(function);
  return edgeBlock->id();
}

} // namespace

OutOfSSAResult OutOfSSAPass::run(Function& function) {
  OutOfSSAResult result;
  if (function.form() != IRForm::SSA) return result;
  rebuildCFG(function);

  std::vector<PhiLowering> phis;
  std::unordered_map<ValueId, ValueId> replacements;
  std::vector<ValueId> erasedPhiValues;

  for (auto& block : function.blocks()) {
    for (const auto& inst : block->instructions()) {
      if (inst->opcode != Opcode::Phi) continue;
      if (!inst->result.has_value()) continue;
      SlotId slot = function.createSlot(SlotKind::Temporary, std::nullopt,
                                        "phi." + std::to_string(inst->result->value));
      phis.push_back(PhiLowering{block->id(), *inst->result, slot, inst->phiIncoming});
      erasedPhiValues.push_back(*inst->result);
      ++result.loweredPhiCount;
    }
  }

  if (phis.empty()) {
    function.setForm(IRForm::CanonicalSlot);
    return result;
  }

  std::map<std::pair<uint32_t, uint32_t>, std::vector<EdgeStore>> edgeStores;
  for (const auto& phi : phis) {
    auto* block = findBlock(function, phi.block);
    if (!block) continue;
    ValueId loadValue = function.createInstValue();
    replacements[phi.phiValue] = loadValue;

    auto& insts = block->mutableInstructions();
    auto insertIt = insts.begin();
    while (insertIt != insts.end() && (*insertIt)->opcode == Opcode::Phi) ++insertIt;
    insts.insert(insertIt, makeLoad(function, phi.slot, loadValue));

    for (const auto& incoming : phi.incoming) {
      edgeStores[{incoming.predecessor.value, phi.block.value}].push_back(EdgeStore{phi.slot, incoming.value});
    }
  }

  replaceAllUses(function, replacements);
  for (auto& entry : edgeStores) {
    for (auto& store : entry.second) {
      auto it = replacements.find(store.value);
      if (it != replacements.end()) store.value = it->second;
    }
  }

  std::size_t splitIndex = 0;
  for (const auto& entry : edgeStores) {
    BlockId pred(entry.first.first);
    BlockId succ(entry.first.second);
    BlockId storeBlockId = pred;
    if (edgeNeedsSplit(function, pred, succ)) {
      storeBlockId = splitEdge(function, pred, succ, splitIndex++);
      ++result.splitEdgeCount;
    }
    auto* storeBlock = findBlock(function, storeBlockId);
    if (!storeBlock) continue;
    auto& insts = storeBlock->mutableInstructions();
    for (const auto& store : entry.second) {
      insts.push_back(makeStore(store.slot, store.value));
    }
  }

  for (auto& block : function.blocks()) {
    auto& insts = block->mutableInstructions();
    insts.erase(std::remove_if(insts.begin(), insts.end(), [](const auto& inst) {
                  return inst->opcode == Opcode::Phi;
                }),
                insts.end());
  }

  function.eraseValues(erasedPhiValues);
  function.setForm(IRForm::CanonicalSlot);
  rebuildCFG(function);
  result.changed = true;
  return result;
}

} // namespace toyc
