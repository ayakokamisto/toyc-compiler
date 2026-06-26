#pragma once
/// Function — a collection of basic blocks forming a callable unit.
///
/// Each function has:
///   - A unique FunctionId and name.
///   - A return type (I32 or Void).
///   - Parameters with ValueIds for argument values and SlotIds for storage.
///   - A list of Slots (parameter, local variable, temporary).
///   - A list of BasicBlocks, the first of which is the entry block.

#include "toyc/ir/basic_block.h"
#include "toyc/ir/ir_type.h"
#include "toyc/ir/value.h"
#include "toyc/support/ids.h"

#include <memory>
#include <string>
#include <vector>

namespace toyc {

/// A parameter with its source symbol and IR value.
struct ParamInfo {
  SymbolId sourceSymbol;
  ValueId valueId;
  SlotId slotId;
};

/// A function in the IR module.
class Function {
public:
  Function(FunctionId id, std::string name, IRType returnType);

  [[nodiscard]] FunctionId id() const { return id_; }
  [[nodiscard]] const std::string& name() const { return name_; }
  [[nodiscard]] IRType returnType() const { return returnType_; }
  [[nodiscard]] bool isInternal() const { return isInternal_; }
  void setInternal(bool v) { isInternal_ = v; }

  /// Basic blocks.
  [[nodiscard]] const std::vector<std::unique_ptr<BasicBlock>>& blocks() const { return blocks_; }
  BasicBlock* createBlock(std::string label = "");

  /// Entry block (first block created).
  [[nodiscard]] BasicBlock* entryBlock() const;

  /// Parameters.
  [[nodiscard]] const std::vector<ParamInfo>& params() const { return params_; }

  /// Add a parameter. Creates the ArgumentValue and parameter Slot.
  /// Returns the ParamInfo with assigned ValueId and SlotId.
  ParamInfo addParam(SymbolId sym);

  /// Slots.
  [[nodiscard]] const std::vector<Slot>& slots() const { return slots_; }
  SlotId createSlot(SlotKind kind, std::optional<SymbolId> sym = std::nullopt);

  /// Values.
  [[nodiscard]] const std::vector<Value>& values() const { return values_; }
  ValueId createArgumentValue();
  ValueId createInstValue();

private:
  FunctionId id_;
  std::string name_;
  IRType returnType_;
  bool isInternal_ = false;
  std::vector<std::unique_ptr<BasicBlock>> blocks_;
  std::vector<ParamInfo> params_;
  std::vector<Slot> slots_;
  std::vector<Value> values_;
  uint32_t nextBlockId_ = 0;
  uint32_t nextSlotId_ = 0;
  uint32_t nextValueId_ = 0;
};

} // namespace toyc
