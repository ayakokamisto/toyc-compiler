/// Function implementation for ToyC Canonical Slot IR.

#include "toyc/ir/function.h"
#include "toyc/ir/module.h"

#include <algorithm>
#include <stdexcept>

namespace toyc {

Function::Function(FunctionId id, std::string name, IRType returnType, Module* parent)
    : id_(id), name_(std::move(name)), returnType_(returnType), parentModule_(parent) {}

BasicBlock* Function::createBlock(std::string label) {
  BlockId bid = parentModule_->allocBlockId();
  auto bb = std::make_unique<BasicBlock>(bid, label.empty() ? "block" + std::to_string(bid.value) : label);
  bb->setParentFunction(id_);
  auto* raw = bb.get();
  blocks_.push_back(std::move(bb));
  return raw;
}

bool Function::eraseBlock(BlockId block) {
  auto oldSize = blocks_.size();
  blocks_.erase(std::remove_if(blocks_.begin(), blocks_.end(), [&](const auto& bb) {
                  return bb->id() == block;
                }),
                blocks_.end());
  return blocks_.size() != oldSize;
}

BasicBlock* Function::entryBlock() const {
  return blocks_.empty() ? nullptr : blocks_.front().get();
}

ParamInfo Function::addParam(SymbolId sym, std::string debugName) {
  ValueId vid = createArgumentValue();
  SlotId sid = createSlot(SlotKind::Parameter, sym, std::move(debugName));
  ParamInfo info{sym, vid, sid};
  params_.push_back(info);
  return info;
}

SlotId Function::createSlot(SlotKind kind, std::optional<SymbolId> sym, std::string debugName) {
  SlotId sid = parentModule_->allocSlotId();
  Slot slot;
  slot.id = sid;
  slot.type = I32Type;
  slot.kind = kind;
  slot.sourceSymbol = sym;
  slot.debugName = std::move(debugName);
  slots_.push_back(slot);
  return sid;
}

void Function::eraseSlots(const std::vector<SlotId>& slots) {
  slots_.erase(std::remove_if(slots_.begin(), slots_.end(), [&](const Slot& slot) {
                 return std::find(slots.begin(), slots.end(), slot.id) != slots.end();
               }),
               slots_.end());
}

void Function::eraseValues(const std::vector<ValueId>& values) {
  values_.erase(std::remove_if(values_.begin(), values_.end(), [&](const Value& value) {
                  return std::find(values.begin(), values.end(), value.id) != values.end();
                }),
                values_.end());
}

ValueId Function::createArgumentValue() {
  ValueId vid = parentModule_->allocValueId();
  Value val;
  val.id = vid;
  val.type = I32Type;
  val.source = ValueSource::Argument;
  val.sourceIndex = static_cast<uint32_t>(params_.size());
  values_.push_back(val);
  return vid;
}

ValueId Function::createInstValue() {
  ValueId vid = parentModule_->allocValueId();
  Value val;
  val.id = vid;
  val.type = I32Type;
  val.source = ValueSource::InstructionResult;
  val.sourceIndex = 0;
  values_.push_back(val);
  return vid;
}

} // namespace toyc
