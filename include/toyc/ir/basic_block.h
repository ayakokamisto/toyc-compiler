#pragma once
/// BasicBlock — a straight-line sequence of instructions ending with a terminator.
///
/// Each block has a unique BlockId and label, a list of ordinary instructions,
/// exactly one terminator, and predecessor/successor edges maintained by rebuildCFG().

#include "toyc/ir/instruction.h"
#include "toyc/support/ids.h"

#include <memory>
#include <string>
#include <vector>

namespace toyc {

class Function;

/// A basic block: a sequence of instructions with a single entry and exit.
class BasicBlock {
public:
  explicit BasicBlock(BlockId id, std::string label = "");

  [[nodiscard]] BlockId id() const { return id_; }
  [[nodiscard]] const std::string& label() const { return label_; }

  /// Ordinary instructions (not including the terminator).
  [[nodiscard]] const std::vector<std::unique_ptr<Inst>>& instructions() const {
    return insts_;
  }
  [[nodiscard]] std::vector<std::unique_ptr<Inst>>& mutableInstructions() { return insts_; }

  /// Append an ordinary instruction. Returns a raw pointer to the appended inst.
  /// Must not be called after a terminator has been set.
  Inst* appendInst(std::unique_ptr<Inst> inst);
  Inst* prependPhi(std::unique_ptr<Inst> inst);

  /// The terminator (Br, CondBr, or Ret). Null if not yet set.
  [[nodiscard]] const Terminator* terminator() const { return term_ ? &*term_ : nullptr; }
  [[nodiscard]] Terminator* mutableTerminator() { return term_ ? &*term_ : nullptr; }
  [[nodiscard]] bool hasTerminator() const { return term_.has_value(); }

  /// Set the terminator. Must not be called twice.
  void setTerminator(Terminator term);

  /// CFG edges — set by rebuildCFG(), not by lowering.
  [[nodiscard]] const std::vector<BlockId>& successors() const { return succs_; }
  [[nodiscard]] const std::vector<BlockId>& predecessors() const { return preds_; }

  /// Parent function.
  [[nodiscard]] FunctionId parentFunction() const { return parentFunc_; }
  void setParentFunction(FunctionId f) { parentFunc_ = f; }

private:
  BlockId id_;
  std::string label_;
  FunctionId parentFunc_;
  std::vector<std::unique_ptr<Inst>> insts_;
  std::optional<Terminator> term_;
  std::vector<BlockId> succs_;
  std::vector<BlockId> preds_;

  // Only rebuildCFG() should modify edges.
  friend void rebuildCFG(Function& func);
  void clearEdges();
  void addSuccessor(BlockId b);
  void addPredecessor(BlockId b);
};

} // namespace toyc
