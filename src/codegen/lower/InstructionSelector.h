#pragma once

#include "codegen/abi/CallingConvention.h"
#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"
#include "codegen/frame/VRegAssignment.h"
#include "codegen/lower/BlockVRegCache.h"

#include <optional>
#include <string_view>

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
};

} // namespace toyc::codegen
