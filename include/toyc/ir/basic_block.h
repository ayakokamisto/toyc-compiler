#pragma once
/// BasicBlock — a straight-line sequence of instructions ending with a terminator.

#include "toyc/ir/instruction.h"
#include "toyc/support/ids.h"

#include <memory>
#include <vector>

namespace toyc {

/// A basic block: a sequence of instructions with a single entry and exit.
class BasicBlock {
public:
  explicit BasicBlock(BlockId id) : id_(id) {}

  [[nodiscard]] BlockId id() const { return id_; }

  /// Instructions in this block (in order).
  [[nodiscard]] const std::vector<Instruction*>& instructions() const { return insts_; }
  void appendInst(Instruction* inst);

  /// CFG edges (set during CFG construction).
  [[nodiscard]] const std::vector<BlockId>& successors() const { return succs_; }
  [[nodiscard]] const std::vector<BlockId>& predecessors() const { return preds_; }
  void addSuccessor(BlockId b);
  void addPredecessor(BlockId b);

  /// The terminator instruction (br, condbr, ret) — last instruction.
  [[nodiscard]] Instruction* terminator() const;

  /// Parent function.
  [[nodiscard]] FunctionId parentFunction() const { return parentFunc_; }
  void setParentFunction(FunctionId f) { parentFunc_ = f; }

private:
  BlockId id_;
  FunctionId parentFunc_;
  std::vector<Instruction*> insts_;
  std::vector<BlockId> succs_;
  std::vector<BlockId> preds_;
};

} // namespace toyc
