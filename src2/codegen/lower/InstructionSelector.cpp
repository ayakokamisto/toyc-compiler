#include "codegen/lower/InstructionSelector.h"

#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"

#include <limits>
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
        knownImm_.clear();
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
        // A vreg being written to by a non-const instruction — clear its known
        // immediate value (if any). ConstInst re-sets it immediately after.
        knownImm_.erase(std::string(vreg));
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

std::string_view InstructionSelector::loadBranchOperand(std::string_view vreg,
                                                        std::string_view scratchReg) {
    if (const std::optional<std::string_view> phys = physicalReg(vreg)) {
        return *phys;
    }

    const auto immIt = knownImm_.find(std::string(vreg));
    if (immIt != knownImm_.end()) {
        if (immIt->second == 0) {
            return "zero";
        }
        if (immIt->second >= -2048 && immIt->second <= 2047) {
            emitter_.instruction("addi", {scratchReg, "zero", imm(immIt->second)});
        } else {
            emitter_.instruction("li", {scratchReg, imm(immIt->second)});
        }
        vregCache_.clobberRegister(scratchReg);
        return scratchReg;
    }

    loadVReg(scratchReg, vreg);
    return scratchReg;
}

bool InstructionSelector::tryEmitSltImmediate(std::string_view dst,
                                              std::string_view src,
                                              const int value,
                                              const bool invert) {
    if (value < -2048 || value > 2047) {
        return false;
    }

    std::string_view outReg = "t0";
    if (const std::optional<std::string_view> dstPhys = physicalReg(dst)) {
        outReg = *dstPhys;
    }

    if (const std::optional<std::string_view> srcPhys = physicalReg(src)) {
        emitter_.instruction("slti", {outReg, *srcPhys, imm(value)});
    } else {
        loadVReg(outReg, src);
        emitter_.instruction("slti", {outReg, outReg, imm(value)});
    }
    if (invert) {
        emitter_.instruction("xori", {outReg, outReg, "1"});
    }
    vregCache_.forgetVReg(dst);
    if (!physicalReg(dst).has_value()) {
        storeVReg(dst, outReg);
    }
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
    if (enableOpt_) {
        const auto knownValue = [this](std::string_view vreg) -> std::optional<std::int32_t> {
            const auto it = knownImm_.find(std::string(vreg));
            if (it == knownImm_.end()) {
                return std::nullopt;
            }
            return it->second;
        };
        const auto emitCopy = [this, dst](std::string_view src) {
            emit(contract::CopyInst{std::string(dst), std::string(src)});
        };
        const auto emitZero = [this, dst] {
            emit(contract::ConstInst{std::string(dst), 0});
        };

        const std::optional<std::int32_t> left = knownValue(src1);
        const std::optional<std::int32_t> right = knownValue(src2);
        if (mnemonic == "add") {
            if (left == 0) {
                emitCopy(src2);
                return;
            }
            if (right == 0) {
                emitCopy(src1);
                return;
            }
        } else if (mnemonic == "sub" && right == 0) {
            emitCopy(src1);
            return;
        } else if (mnemonic == "mul") {
            if (left == 0 || right == 0) {
                emitZero();
                return;
            }
            if (left == 1) {
                emitCopy(src2);
                return;
            }
            if (right == 1) {
                emitCopy(src1);
                return;
            }
        } else if (mnemonic == "div" && right == 1) {
            emitCopy(src1);
            return;
        } else if (mnemonic == "rem" && right == 1) {
            emitZero();
            return;
        } else if (mnemonic == "and") {
            if (left == 0 || right == 0) {
                emitZero();
                return;
            }
            if (left == -1) {
                emitCopy(src2);
                return;
            }
            if (right == -1) {
                emitCopy(src1);
                return;
            }
        } else if (mnemonic == "or" || mnemonic == "xor") {
            if (left == 0) {
                emitCopy(src2);
                return;
            }
            if (right == 0) {
                emitCopy(src1);
                return;
            }
        }
    }

    // Try immediate folding FIRST: if one source is a known constant that fits
    // in a 12-bit signed immediate, use the I-type encoding (addi, andi, ori, xori).
    // This must run before tryEmitPhysicalBinaryOp because the physical op would
    // succeed (all operands have registers) but emit a register-register form.
    if (enableOpt_) {
        auto tryImmediate = [&](std::string_view regSrc, std::string_view immSrc) -> bool {
            auto it = knownImm_.find(std::string(immSrc));
            if (it == knownImm_.end()) return false;
            const std::int32_t val = it->second;
            if (val < -2048 || val > 2047) return false;

            std::string immMnemonic;
            if (mnemonic == "add") immMnemonic = "addi";
            else if (mnemonic == "and") immMnemonic = "andi";
            else if (mnemonic == "or") immMnemonic = "ori";
            else if (mnemonic == "xor") immMnemonic = "xori";
            else return false;

            // For addi with negative immediate, flip to addi with negated value.
            // For sub with immediate: sub x, a, imm → addi x, a, -imm
            if (const std::optional<std::string_view> dstPhys = physicalReg(dst)) {
                if (const std::optional<std::string_view> srcPhys = physicalReg(regSrc)) {
                    emitter_.instruction(immMnemonic, {*dstPhys, *srcPhys, imm(val)});
                    vregCache_.forgetVReg(dst);
                    return true;
                }
            }

            loadVReg("t0", regSrc);
            vregCache_.clobberRegister("t0");
            emitter_.instruction(immMnemonic, {"t0", "t0", imm(val)});
            storeVReg(dst, "t0");
            return true;
        };

        // Try src2 as immediate first (more common pattern), then src1.
        if (tryImmediate(src1, src2)) {
            return;
        }
        if (tryImmediate(src2, src1)) {
            return;
        }

        // Special case: sub x, a, imm → addi x, a, -imm
        if (mnemonic == "sub") {
            auto it = knownImm_.find(std::string(src2));
            if (it != knownImm_.end()) {
                const std::int32_t val = it->second;
                const std::int32_t neg = -val;
                if (neg >= -2048 && neg <= 2047) {
                    if (const std::optional<std::string_view> dstPhys = physicalReg(dst)) {
                        if (const std::optional<std::string_view> srcPhys = physicalReg(src1)) {
                            emitter_.instruction("addi", {*dstPhys, *srcPhys, imm(neg)});
                            vregCache_.forgetVReg(dst);
                            return;
                        }
                    }

                    loadVReg("t0", src1);
                    vregCache_.clobberRegister("t0");
                    emitter_.instruction("addi", {"t0", "t0", imm(neg)});
                    storeVReg(dst, "t0");
                    return;
                }
            }
        }
    }

    // Try physical register form (all operands in callee-saved registers).
    if (tryEmitPhysicalBinaryOp(dst, src1, src2, mnemonic)) {
        return;
    }

    // Fallback: load operands and emit operation.
    // When sources have physical registers, use them directly to avoid mv.
    // Always write result through storeVReg to maintain cache consistency.
    const std::optional<std::string_view> s1Phys = physicalReg(src1);
    const std::optional<std::string_view> s2Phys = physicalReg(src2);
    const std::string_view r1 = s1Phys.has_value() ? *s1Phys : [&] {
        loadVReg("t0", src1);
        return std::string_view{"t0"};
    }();
    const std::string_view r2 = s2Phys.has_value() ? *s2Phys : [&] {
        loadVReg("t1", src2);
        return std::string_view{"t1"};
    }();
    vregCache_.clobberRegister("t0");
    emitter_.instruction(mnemonic, {"t0", r1, r2});
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

void emitDirectCompareBranch(RiscvEmitter& emitter,
                             std::string_view functionName,
                             std::string_view trueLabel,
                             std::string_view falseLabel,
                             std::string_view mnemonic,
                             std::string_view lhs,
                             std::string_view rhs) {
    emitter.instruction(std::string(mnemonic), {lhs, rhs, blockLabel(functionName, trueLabel)});
    emitter.instruction("j", {blockLabel(functionName, falseLabel)});
}

void InstructionSelector::emit(const contract::Instruction& instruction) {
    std::visit(
        [&](const auto& inst) {
            using Inst = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<Inst, contract::ConstInst>) {
                // Record known immediate for later folding into addi/andi/etc.
                if (enableOpt_) {
                    knownImm_[std::string(inst.dst)] = inst.value;
                }
                if (enableOpt_ && assignment_.rematerializedConstant(inst.dst).has_value()) {
                    vregCache_.forgetVReg(inst.dst);
                    return;
                }
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
                knownImm_.clear(); // Calls clobber all known state.
                abi_.emitCallArgs(inst.args);
                emitter_.instruction("call", {inst.functionName});
                abi_.emitCallCleanup(inst.args.size());
                vregCache_.invalidateAll();
                storeVReg(inst.dst, "a0");
            } else if constexpr (std::is_same_v<Inst, contract::CallVoidInst>) {
                vregCache_.invalidateAll();
                knownImm_.clear(); // Calls clobber all known state.
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
                if (const auto it = knownImm_.find(inst.src2); it != knownImm_.end()) {
                    if (tryEmitSltImmediate(inst.dst, inst.src1, it->second, false)) {
                        return;
                    }
                }
                if (!tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "lt")) {
                    emitBinaryOp(inst.dst, inst.src1, inst.src2, "slt");
                }
            } else if constexpr (std::is_same_v<Inst, contract::LeInst>) {
                if (const auto it = knownImm_.find(inst.src2);
                    it != knownImm_.end() && it->second != std::numeric_limits<int>::max()) {
                    if (tryEmitSltImmediate(inst.dst, inst.src1, it->second + 1, false)) {
                        return;
                    }
                }
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
                if (const auto it = knownImm_.find(inst.src2); it != knownImm_.end()) {
                    if (tryEmitSltImmediate(inst.dst, inst.src1, it->second, true)) {
                        return;
                    }
                }
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
                const std::string_view lhs = loadBranchOperand(inst.src1, "t0");
                const std::string_view rhs = loadBranchOperand(inst.src2, "t1");
                emitDirectCompareBranch(
                    emitter_, functionName, branch.trueLabel, branch.falseLabel, "beq", lhs, rhs);
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::NeInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                const std::string_view lhs = loadBranchOperand(inst.src1, "t0");
                const std::string_view rhs = loadBranchOperand(inst.src2, "t1");
                emitDirectCompareBranch(
                    emitter_, functionName, branch.trueLabel, branch.falseLabel, "bne", lhs, rhs);
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::LtInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                const std::string_view lhs = loadBranchOperand(inst.src1, "t0");
                const std::string_view rhs = loadBranchOperand(inst.src2, "t1");
                emitDirectCompareBranch(
                    emitter_, functionName, branch.trueLabel, branch.falseLabel, "blt", lhs, rhs);
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::LeInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                const std::string_view lhs = loadBranchOperand(inst.src1, "t0");
                const std::string_view rhs = loadBranchOperand(inst.src2, "t1");
                emitDirectCompareBranch(
                    emitter_, functionName, branch.trueLabel, branch.falseLabel, "bge", rhs, lhs);
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::GtInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                const std::string_view lhs = loadBranchOperand(inst.src1, "t0");
                const std::string_view rhs = loadBranchOperand(inst.src2, "t1");
                emitDirectCompareBranch(
                    emitter_, functionName, branch.trueLabel, branch.falseLabel, "blt", rhs, lhs);
                vregCache_.invalidateAll();
                return true;
            }
            if constexpr (std::is_same_v<Inst, contract::GeInst>) {
                if (inst.dst != branch.cond) {
                    return false;
                }
                const std::string_view lhs = loadBranchOperand(inst.src1, "t0");
                const std::string_view rhs = loadBranchOperand(inst.src2, "t1");
                emitDirectCompareBranch(
                    emitter_, functionName, branch.trueLabel, branch.falseLabel, "bge", lhs, rhs);
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
