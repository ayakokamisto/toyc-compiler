#pragma once

#include "codegen/abi/CallingConvention.h"
#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"
#include "codegen/frame/VRegAssignment.h"
#include "codegen/lower/BlockVRegCache.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace toyc::codegen {

class RiscvEmitter;

class InstructionSelector {
public:
    InstructionSelector(RiscvEmitter& emitter,
                        const StackFrame& frame,
                        const VRegAssignment& assignment,
                        bool enableOpt = false);

    void beginBasicBlock();
    void emit(const contract::Instruction& instruction);
    void emitTerminator(const contract::Terminator& terminator,
                        std::string_view functionName,
                        std::string_view epilogueLabel);

    [[nodiscard]] bool tryEmitFusedCompareBranch(const contract::Instruction& lastInstruction,
                                                 const contract::BranchInst& branch,
                                                 std::string_view functionName);

private:
    void loadVReg(std::string_view reg, std::string_view vreg);
    void storeVReg(std::string_view vreg, std::string_view reg);
    [[nodiscard]] std::optional<std::string_view> physicalReg(std::string_view vreg) const;
    [[nodiscard]] bool tryEmitPhysicalBinaryOp(std::string_view dst,
                                               std::string_view src1,
                                               std::string_view src2,
                                               std::string_view mnemonic);
    [[nodiscard]] bool tryEmitPhysicalCompareOp(std::string_view dst,
                                                std::string_view src1,
                                                std::string_view src2,
                                                std::string_view compareKind);
    [[nodiscard]] bool tryEmitPhysicalUnaryOp(std::string_view dst,
                                              std::string_view src,
                                              std::string_view mnemonic);
    [[nodiscard]] bool tryEmitPhysicalConst(std::string_view dst, int value);
    [[nodiscard]] bool tryEmitPhysicalLoadGlobal(std::string_view dst, std::string_view name);
    [[nodiscard]] std::string_view loadBranchOperand(std::string_view vreg,
                                                     std::string_view scratchReg);
    [[nodiscard]] bool tryEmitSltImmediate(std::string_view dst,
                                           std::string_view src,
                                           int value,
                                           bool invert);
    void emitBinaryInputs(std::string_view src1, std::string_view src2);
    void emitBinaryOp(std::string_view dst,
                      std::string_view src1,
                      std::string_view src2,
                      std::string_view mnemonic);
    void emitBranchTo(std::string_view functionName,
                      std::string_view trueLabel,
                      std::string_view falseLabel,
                      std::string_view takenMnemonic,
                      std::string_view condReg);

    RiscvEmitter& emitter_;
    const StackFrame& frame_;
    const VRegAssignment& assignment_;
    CallingConvention abi_;
    BlockVRegCache vregCache_;
    bool enableOpt_ = false;

    // Tracks vregs known to hold constant immediate values within a basic block.
    // Used to fold constants into immediate-encoded instructions (addi, andi, etc.).
    // Cleared at block boundaries, calls, and any instruction that writes to a
    // tracked vreg with a non-const value.
    std::unordered_map<std::string, std::int32_t> knownImm_;
};

} // namespace toyc::codegen
