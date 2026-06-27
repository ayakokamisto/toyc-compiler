/// BasicBlock implementation for ToyC Canonical Slot IR.

#include "toyc/ir/basic_block.h"

namespace toyc {

BasicBlock::BasicBlock(BlockId id, std::string label)
    : id_(id), label_(std::move(label)) {}

Inst* BasicBlock::appendInst(std::unique_ptr<Inst> inst) {
  auto* raw = inst.get();
  insts_.push_back(std::move(inst));
  return raw;
}

Inst* BasicBlock::prependPhi(std::unique_ptr<Inst> inst) {
  auto* raw = inst.get();
  auto insertIt = insts_.begin();
  while (insertIt != insts_.end() && (*insertIt)->opcode == Opcode::Phi) {
    ++insertIt;
  }
  insts_.insert(insertIt, std::move(inst));
  return raw;
}

void BasicBlock::setTerminator(Terminator term) {
  term_ = std::move(term);
}

void BasicBlock::clearEdges() {
  succs_.clear();
  preds_.clear();
}

void BasicBlock::addSuccessor(BlockId b) {
  for (auto s : succs_) {
    if (s == b) return;  // Deduplicate.
  }
  succs_.push_back(b);
}

void BasicBlock::addPredecessor(BlockId b) {
  for (auto p : preds_) {
    if (p == b) return;  // Deduplicate.
  }
  preds_.push_back(b);
}

} // namespace toyc
