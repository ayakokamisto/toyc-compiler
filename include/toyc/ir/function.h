#pragma once
/// Function — a collection of basic blocks forming a callable unit.

#include "toyc/ir/basic_block.h"
#include "toyc/ir/ir_type.h"
#include "toyc/sema/symbol.h"
#include "toyc/support/ids.h"

#include <memory>
#include <string>
#include <vector>

namespace toyc {

/// A function in the IR module.
class Function {
public:
  Function(FunctionId id, std::string name, IRType returnType)
      : id_(id), name_(std::move(name)), returnType_(returnType) {}

  [[nodiscard]] FunctionId id() const { return id_; }
  [[nodiscard]] const std::string& name() const { return name_; }
  [[nodiscard]] IRType returnType() const { return returnType_; }

  /// Basic blocks.
  [[nodiscard]] const std::vector<std::unique_ptr<BasicBlock>>& blocks() const { return blocks_; }
  BasicBlock* createBlock();

  /// Parameters (SymbolIds from sema).
  [[nodiscard]] const std::vector<SymbolId>& params() const { return params_; }
  void addParam(SymbolId s) { params_.push_back(s); }

  /// Entry block (first block).
  [[nodiscard]] BasicBlock* entryBlock() const;

private:
  FunctionId id_;
  std::string name_;
  IRType returnType_;
  std::vector<std::unique_ptr<BasicBlock>> blocks_;
  std::vector<SymbolId> params_;
};

} // namespace toyc
