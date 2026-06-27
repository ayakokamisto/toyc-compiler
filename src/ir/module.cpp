/// Module implementation for ToyC Canonical Slot IR.

#include "toyc/ir/module.h"

namespace toyc {

Module::Module(Module&& other) noexcept
    : funcs_(std::move(other.funcs_)),
      globals_(std::move(other.globals_)),
      nextFuncId_(other.nextFuncId_),
      nextGlobalId_(other.nextGlobalId_),
      nextValueId_(other.nextValueId_),
      nextSlotId_(other.nextSlotId_),
      nextBlockId_(other.nextBlockId_) {
  // Rebind all Function pointers to this (the new Module address).
  for (auto& f : funcs_) {
    f->rebindParentModule(this);
  }
}

Module& Module::operator=(Module&& other) noexcept {
  if (this != &other) {
    funcs_ = std::move(other.funcs_);
    globals_ = std::move(other.globals_);
    nextFuncId_ = other.nextFuncId_;
    nextGlobalId_ = other.nextGlobalId_;
    nextValueId_ = other.nextValueId_;
    nextSlotId_ = other.nextSlotId_;
    nextBlockId_ = other.nextBlockId_;
    // Rebind all Function pointers to this.
    for (auto& f : funcs_) {
      f->rebindParentModule(this);
    }
  }
  return *this;
}

Function* Module::createFunction(std::string name, IRType returnType) {
  FunctionId fid(nextFuncId_++);
  auto func = std::make_unique<Function>(fid, std::move(name), returnType, this);
  auto* ptr = func.get();
  funcs_.push_back(std::move(func));
  return ptr;
}

GlobalId Module::createGlobal(IRGlobal global) {
  GlobalId gid(nextGlobalId_++);
  global.id = gid;
  globals_.push_back(std::move(global));
  return gid;
}

const IRGlobal* Module::findGlobal(GlobalId id) const {
  for (const auto& g : globals_) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

const Function* Module::findFunction(FunctionId id) const {
  for (const auto& f : funcs_) {
    if (f->id() == id) return f.get();
  }
  return nullptr;
}

Function* Module::findFunctionByName(const std::string& name) {
  for (auto& f : funcs_) {
    if (f->name() == name) return f.get();
  }
  return nullptr;
}

} // namespace toyc
