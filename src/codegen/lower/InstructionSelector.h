#pragma once

#include "codegen/abi/CallingConvention.h"
#include "codegen/ContractIR.h"
#include "codegen/frame/StackFrame.h"
#include "codegen/frame/VRegAssignment.h"
#include "codegen/lower/BlockVRegCache.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace toyc::codegen {

class RiscvEmitter;

class InstructionSelector {
public:
    InstructionSelector(RiscvEmitter& emitter,
                        const StackFrame& frame,
                        const VRegAssignment& assignment,
                        bool enableOpt = false,
                        std::unordered_map<std::string, std::int32_t> foldableConsts = {});

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
    // Returns the constant value if `vreg` is a foldable immediate constant.
    [[nodiscard]] std::optional<std::int32_t> foldableConst(std::string_view vreg) const;
    // True if either operand is a foldable constant.
    [[nodiscard]] bool hasFoldableOperand(std::string_view src1, std::string_view src2) const;
    // If one operand is the foldable constant 0, returns the other operand;
    // otherwise returns an empty view.
    [[nodiscard]] std::string_view zeroCompareOperand(std::string_view src1,
                                                      std::string_view src2) const;
    // Emit a binary op whose right operand is an immediate (addi/slti/...).
    // Returns false if the op cannot use the immediate form.
    [[nodiscard]] bool tryEmitImmediateBinaryOp(std::string_view dst,
                                                 std::string_view src,
                                                 std::int32_t imm,
                                                 std::string_view immMnemonic);
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
    std::unordered_map<std::string, std::int32_t> foldableConsts_;
};

} // namespace toyc::codegen
