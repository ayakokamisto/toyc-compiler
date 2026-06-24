#include "codegen/lower/InstructionSelector.h"

#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"

#include <type_traits>
#include <variant>

namespace toyc::codegen {

InstructionSelector::InstructionSelector(RiscvEmitter& emitter,
                                         const StackFrame& frame,
                                         const VRegAssignment& assignment,
                                         const bool enableOpt)
    : emitter_(emitter),
      frame_(frame),
      assignment_(assignment),
      abi_(emitter, frame, assignment),
      enableOpt_(enableOpt) {}

void InstructionSelector::beginBasicBlock() {
    if (enableOpt_) {
        vregCache_.invalidateAll();
    }
}

void InstructionSelector::loadVReg(std::string_view reg, std::string_view vreg) {
    if (enableOpt_) {
        vregCache_.load(abi_, emitter_, reg, vreg);
        return;
    }
    abi_.loadVReg(reg, vreg);
}

void InstructionSelector::storeVReg(std::string_view vreg, std::string_view reg) {
    if (enableOpt_) {
        vregCache_.store(abi_, vreg, reg);
        return;
    }
    abi_.storeVReg(vreg, reg);
}

std::optional<std::string_view> InstructionSelector::physicalReg(std::string_view vreg) const {
    if (!enableOpt_) {
        return std::nullopt;
    }
    return assignment_.physicalReg(vreg);
}

bool InstructionSelector::tryEmitPhysicalBinaryOp(std::string_view dst,
                                                  std::string_view src1,
                                                  std::string_view src2,
                                                  std::string_view mnemonic) {
    const std::optional<std::string_view> dstPhys = physicalReg(dst);
    const std::optional<std::string_view> src1Phys = physicalReg(src1);
    const std::optional<std::string_view> src2Phys = physicalReg(src2);
    if (!dstPhys.has_value() || !src1Phys.has_value() || !src2Phys.has_value()) {
        return false;
    }

    emitter_.instruction(std::string(mnemonic), {*dstPhys, *src1Phys, *src2Phys});
    vregCache_.forgetVReg(dst);
    return true;
}

bool InstructionSelector::tryEmitPhysicalCompareOp(std::string_view dst,
                                                   std::string_view src1,
                                                   std::string_view src2,
                                                   std::string_view compareKind) {
    const std::optional<std::string_view> dstPhys = physicalReg(dst);
    const std::optional<std::string_view> src1Phys = physicalReg(src1);
    const std::optional<std::string_view> src2Phys = physicalReg(src2);
    if (!dstPhys.has_value() || !src1Phys.has_value() || !src2Phys.has_value()) {
        return false;
    }

    if (compareKind == "eq" || compareKind == "ne") {
        emitter_.instruction("sub", {*dstPhys, *src1Phys, *src2Phys});
        emitter_.instruction(compareKind == "eq" ? "seqz" : "snez", {*dstPhys, *dstPhys});
    } else if (compareKind == "lt") {
        emitter_.instruction("slt", {*dstPhys, *src1Phys, *src2Phys});
    } else if (compareKind == "le") {
        emitter_.instruction("slt", {*dstPhys, *src2Phys, *src1Phys});
        emitter_.instruction("xori", {*dstPhys, *dstPhys, "1"});
    } else if (compareKind == "gt") {
        emitter_.instruction("slt", {*dstPhys, *src2Phys, *src1Phys});
    } else if (compareKind == "ge") {
        emitter_.instruction("slt", {*dstPhys, *src1Phys, *src2Phys});
        emitter_.instruction("xori", {*dstPhys, *dstPhys, "1"});
    } else {
        return false;
    }

    vregCache_.forgetVReg(dst);
    return true;
}

bool InstructionSelector::tryEmitPhysicalUnaryOp(std::string_view dst,
                                                 std::string_view src,
                                                 std::string_view mnemonic) {
    const std::optional<std::string_view> dstPhys = physicalReg(dst);
    if (!dstPhys.has_value()) {
        return false;
    }

    if (const std::optional<std::string_view> srcPhys = physicalReg(src)) {
        if (mnemonic == "neg") {
            emitter_.instruction("sub", {*dstPhys, "zero", *srcPhys});
        } else if (mnemonic == "lnot") {
            emitter_.instruction("seqz", {*dstPhys, *srcPhys});
        } else {
            return false;
        }
        vregCache_.forgetVReg(dst);
        return true;
    }

    loadVReg(*dstPhys, src);
    if (mnemonic == "neg") {
        emitter_.instruction("sub", {*dstPhys, "zero", *dstPhys});
    } else if (mnemonic == "lnot") {
        emitter_.instruction("seqz", {*dstPhys, *dstPhys});
    } else {
        return false;
    }
    vregCache_.forgetVReg(dst);
    return true;
}

bool InstructionSelector::tryEmitPhysicalConst(std::string_view dst, const int value) {
    const std::optional<std::string_view> dstPhys = physicalReg(dst);
    if (!dstPhys.has_value()) {
        return false;
    }

    if (value == 0 || value == 1) {
        emitter_.instruction("addi", {*dstPhys, "zero", imm(value)});
    } else {
        emitter_.instruction("li", {*dstPhys, imm(value)});
    }
    vregCache_.forgetVReg(dst);
    return true;
}

bool InstructionSelector::tryEmitPhysicalLoadGlobal(std::string_view dst, std::string_view name) {
    const std::optional<std::string_view> dstPhys = physicalReg(dst);
    if (!dstPhys.has_value()) {
        return false;
    }

    emitter_.instruction("la", {"t0", globalLabel(name)});
    emitter_.instruction("lw", {*dstPhys, offsetReg(0, "t0")});
    vregCache_.clobberRegister("t0");
    vregCache_.forgetVReg(dst);
    return true;
}

void InstructionSelector::emitBinaryInputs(std::string_view src1, std::string_view src2) {
    loadVReg("t0", src1);
    loadVReg("t1", src2);
}

void InstructionSelector::emitBinaryOp(std::string_view dst,
                                       std::string_view src1,
                                       std::string_view src2,
                                       std::string_view mnemonic) {
    if (tryEmitPhysicalBinaryOp(dst, src1, src2, mnemonic)) {
        return;
    }

    emitBinaryInputs(src1, src2);
    vregCache_.clobberRegister("t0");
    emitter_.instruction(mnemonic, {"t0", "t0", "t1"});
    storeVReg(dst, "t0");
}

void InstructionSelector::emitBranchTo(std::string_view functionName,
                                       std::string_view trueLabel,
                                       std::string_view falseLabel,
                                       std::string_view takenMnemonic,
                                       std::string_view condReg) {
    emitter_.instruction(std::string(takenMnemonic),
                         {condReg, blockLabel(functionName, trueLabel)});
    emitter_.instruction("j", {blockLabel(functionName, falseLabel)});
}

void InstructionSelector::emit(const contract::Instruction& instruction) {
    std::visit(
        [&](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::ConstInst>) {
                if (tryEmitPhysicalConst(inst.dst, inst.value)) {
                    return;
                }
                if (enableOpt_ && inst.value == 0) {
                    if (const std::optional<std::string_view> physReg =
                            assignment_.physicalReg(inst.dst)) {
                        emitter_.instruction("mv", {*physReg, "zero"});
                    } else {
                        emitter_.instruction(
                            "sw", {"zero", offsetReg(frame_.vregOffsetFromS0(inst.dst), "s0")});
                    }
                    vregCache_.forgetVReg(inst.dst);
                    return;
                }
                if (enableOpt_ && inst.value == 1) {
                    emitter_.instruction("addi", {"t0", "zero", "1"});
                    storeVReg(inst.dst, "t0");
                    return;
                }
                emitter_.instruction("li", {"t0", imm(inst.value)});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::CopyInst>) {
                if (enableOpt_) {
                    const std::optional<std::string_view> srcPhys =
                        assignment_.physicalReg(inst.src);
                    const std::optional<std::string_view> dstPhys =
                        assignment_.physicalReg(inst.dst);
                    if (srcPhys.has_value() && dstPhys.has_value() && *srcPhys == *dstPhys) {
                        vregCache_.forgetVReg(inst.dst);
                        return;
                    }
                    if (srcPhys.has_value()) {
                        storeVReg(inst.dst, *srcPhys);
                        return;
                    }
                    if (dstPhys.has_value()) {
                        loadVReg(*dstPhys, inst.src);
                        vregCache_.forgetVReg(inst.dst);
                        return;
                    }
                }
                loadVReg("t0", inst.src);
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::LoadGlobalInst>) {
                if (tryEmitPhysicalLoadGlobal(inst.dst, inst.name)) {
                    return;
                }
                emitter_.instruction("la", {"t0", globalLabel(inst.name)});
                emitter_.instruction("lw", {"t1", offsetReg(0, "t0")});
                vregCache_.clobberRegister("t0");
                vregCache_.clobberRegister("t1");
                storeVReg(inst.dst, "t1");
            } else if constexpr (std::is_same_v<Inst, contract::StoreGlobalInst>) {
                loadVReg("t0", inst.src);
                emitter_.instruction("la", {"t1", globalLabel(inst.name)});
                emitter_.instruction("sw", {"t0", offsetReg(0, "t1")});
                vregCache_.clobberRegister("t0");
                vregCache_.clobberRegister("t1");
            } else if constexpr (std::is_same_v<Inst, contract::CallInst>) {
                vregCache_.invalidateAll();
                abi_.emitCallArgs(inst.args);
                emitter_.instruction("call", {inst.functionName});
                abi_.emitCallCleanup(inst.args.size());
                vregCache_.invalidateAll();
                storeVReg(inst.dst, "a0");
            } else if constexpr (std::is_same_v<Inst, contract::CallVoidInst>) {
                vregCache_.invalidateAll();
                abi_.emitCallArgs(inst.args);
                emitter_.instruction("call", {inst.functionName});
                abi_.emitCallCleanup(inst.args.size());
                vregCache_.invalidateAll();
            } else if constexpr (std::is_same_v<Inst, contract::AddInst>) {
                emitBinaryOp(inst.dst, inst.src1, inst.src2, "add");
            } else if constexpr (std::is_same_v<Inst, contract::SubInst>) {
                emitBinaryOp(inst.dst, inst.src1, inst.src2, "sub");
            } else if constexpr (std::is_same_v<Inst, contract::MulInst>) {
                emitBinaryOp(inst.dst, inst.src1, inst.src2, "mul");
            } else if constexpr (std::is_same_v<Inst, contract::DivInst>) {
                emitBinaryOp(inst.dst, inst.src1, inst.src2, "div");
            } else if constexpr (std::is_same_v<Inst, contract::ModInst>) {
                emitBinaryOp(inst.dst, inst.src1, inst.src2, "rem");
            } else if constexpr (std::is_same_v<Inst, contract::NegInst>) {
                if (tryEmitPhysicalUnaryOp(inst.dst, inst.src, "neg")) {
                    return;
                }
                loadVReg("t0", inst.src);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("sub", {"t0", "zero", "t0"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::EqInst>) {
                if (tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "eq")) {
                    return;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("sub", {"t0", "t0", "t1"});
                emitter_.instruction("seqz", {"t0", "t0"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::NeInst>) {
                if (tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "ne")) {
                    return;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("sub", {"t0", "t0", "t1"});
                emitter_.instruction("snez", {"t0", "t0"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::LtInst>) {
                if (!tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "lt")) {
                    emitBinaryOp(inst.dst, inst.src1, inst.src2, "slt");
                }
            } else if constexpr (std::is_same_v<Inst, contract::LeInst>) {
                if (tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "le")) {
                    return;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("slt", {"t0", "t1", "t0"});
                emitter_.instruction("xori", {"t0", "t0", "1"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::GtInst>) {
                if (tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "gt")) {
                    return;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("slt", {"t0", "t1", "t0"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::GeInst>) {
                if (tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "ge")) {
                    return;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("slt", {"t0", "t0", "t1"});
                emitter_.instruction("xori", {"t0", "t0", "1"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::LNotInst>) {
                if (tryEmitPhysicalUnaryOp(inst.dst, inst.src, "lnot")) {
                    return;
                }
                loadVReg("t0", inst.src);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("seqz", {"t0", "t0"});
                storeVReg(inst.dst, "t0");
            }
        },
        instruction);
}

bool InstructionSelector::tryEmitFusedCompareBranch(const contract::Instruction& lastInstruction,
                                                    const contract::BranchInst& branch,
                                                    std::string_view functionName) {
    if (!enableOpt_) {
        return false;
    }

    return std::visit(
        [&](const auto& inst) -> bool {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::EqInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("sub", {"t0", "t0", "t1"});
                emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "beqz", "t0");
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::NeInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("sub", {"t0", "t0", "t1"});
                emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez", "t0");
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::LtInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("slt", {"t0", "t0", "t1"});
                emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez", "t0");
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::LeInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("slt", {"t0", "t1", "t0"});
                emitter_.instruction("xori", {"t0", "t0", "1"});
                emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez", "t0");
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::GtInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("slt", {"t0", "t1", "t0"});
                emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez", "t0");
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::GeInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("slt", {"t0", "t0", "t1"});
                emitter_.instruction("xori", {"t0", "t0", "1"});
                emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez", "t0");
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::LNotInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                loadVReg("t0", inst.src);
                emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "beqz", "t0");
                vregCache_.invalidateAll();
                return true;
            }
            return false;
        },
        lastInstruction);
}

void InstructionSelector::emitTerminator(const contract::Terminator& terminator,
                                         std::string_view functionName,
                                         std::string_view functionEpilogueLabel) {
    std::visit(
        [&](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::JumpInst>) {
                emitter_.instruction("j", {blockLabel(functionName, inst.targetLabel)});
            } else if constexpr (std::is_same_v<Inst, contract::BranchInst>) {
                loadVReg("t0", inst.cond);
                emitter_.instruction("bnez", {"t0", blockLabel(functionName, inst.trueLabel)});
                emitter_.instruction("j", {blockLabel(functionName, inst.falseLabel)});
            } else if constexpr (std::is_same_v<Inst, contract::ReturnInst>) {
                if (inst.src.has_value()) {
                    loadVReg("a0", *inst.src);
                }
                emitter_.instruction("j", {functionEpilogueLabel});
            }
        },
        terminator);
}

} // namespace toyc::codegen
