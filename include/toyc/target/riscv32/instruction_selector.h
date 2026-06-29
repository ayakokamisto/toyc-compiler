#pragma once
/// RV32 Instruction Selector — lowers Canonical Slot IR to RV32 MIR.

#include "toyc/mir/mir.h"
#include "toyc/ir/module.h"
#include "toyc/support/diagnostics.h"

#include <optional>
#include <unordered_map>

namespace toyc {

/// Lowers a Canonical Slot IR Module to an RV32 MIRModule.
class RV32InstructionSelector {
public:
  explicit RV32InstructionSelector(DiagnosticEngine& diagnostics);

  /// Lower the IR module to MIR. Returns nullopt on error.
  std::optional<MIRModule> lower(const Module& module);

  [[nodiscard]] bool hasError() const noexcept { return hasError_; }

private:
  DiagnosticEngine& diag_;
  bool hasError_ = false;

  // Lowering state (per-function)
  MIRModule* mirModule_ = nullptr;
  const Module* irModule_ = nullptr;
  MIRFunction* currentFunc_ = nullptr;

  // ValueId → VRegId mapping (per-function)
  std::unordered_map<uint32_t, VRegId> valueToVReg_;
  std::unordered_map<uint32_t, int32_t> valueToConst_;
  // SlotId → FrameObject index mapping (per-function)
  std::unordered_map<uint32_t, int> slotToFrameObj_;
  // BlockId → MIR block index mapping (per-function)
  std::unordered_map<uint32_t, int> blockToIndex_;

  // Soft arithmetic helper tracking
  bool usesMul_ = false;
  bool usesDiv_ = false;
  bool usesMod_ = false;

  void lowerFunction(const Function& func);
  void lowerBlock(const BasicBlock& bb);
  VRegId getOrCreateVReg(ValueId val);
  void lowerInst(const Inst& inst);
  void lowerTerminator(const Terminator& term, const BasicBlock& bb);

  // Emit helpers
  MIRBlock& currentBlock();
  void emit(MIROpcode op, std::vector<MIROperand> ops = {});
  void emitComment(const std::string& text);
  VRegId newVReg();

  // Global label helpers
  std::string globalSymbolName(GlobalId id) const;
  std::string functionLabel(const Function& func) const;
  std::string blockLabel(BlockId id) const;

  // Emit soft arithmetic helpers at end of module
  void emitSoftMulHelper();
  void emitSoftDivHelper();
  void emitSoftModHelper();
};

} // namespace toyc
