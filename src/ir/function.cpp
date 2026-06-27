/// Function implementation for ToyC Canonical Slot IR.

#include "toyc/ir/function.h"
#include "toyc/ir/module.h"

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
