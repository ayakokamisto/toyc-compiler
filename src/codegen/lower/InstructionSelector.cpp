#include "codegen/lower/InstructionSelector.h"

#include "codegen/emit/CodegenUtils.h"
#include "codegen/emit/RiscvEmitter.h"

#include <type_traits>
#include <variant>

namespace toyc::codegen {

InstructionSelector::InstructionSelector(RiscvEmitter& emitter,
                                         const StackFrame& frame,
                                         const VRegAssignment& assignment,
                                         const bool enableOpt,
                                         std::unordered_map<std::string, std::int32_t> foldableConsts)
    : emitter_(emitter),
      frame_(frame),
      assignment_(assignment),
      abi_(emitter, frame, assignment),
      enableOpt_(enableOpt),
      foldableConsts_(std::move(foldableConsts)) {}

std::optional<std::int32_t> InstructionSelector::foldableConst(std::string_view vreg) const {
    if (!enableOpt_) {
        return std::nullopt;
    }
    const auto it = foldableConsts_.find(std::string(vreg));
    if (it == foldableConsts_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool InstructionSelector::hasFoldableOperand(std::string_view src1, std::string_view src2) const {
    return foldableConst(src1).has_value() || foldableConst(src2).has_value();
}

std::string_view InstructionSelector::zeroCompareOperand(std::string_view src1,
                                                         std::string_view src2) const {
    if (foldableConst(src1) == std::optional<std::int32_t>(0)) {
        return src2;
    }
    if (foldableConst(src2) == std::optional<std::int32_t>(0)) {
        return src1;
    }
    return {};
}

void InstructionSelector::invalidateGlobalAddr(std::string_view reg) {
    for (auto it = globalAddrReg_.begin(); it != globalAddrReg_.end();) {
        if (it->second == reg) {
            it = globalAddrReg_.erase(it);
        } else {
            ++it;
        }
    }
}

void InstructionSelector::beginBasicBlock() {
    if (enableOpt_) {
        vregCache_.invalidateAll();
        globalAddrReg_.clear();
    }
}

void InstructionSelector::loadVReg(std::string_view reg, std::string_view vreg) {
    if (enableOpt_) {
        if (vregCache_.load(abi_, emitter_, reg, vreg)) {
            // reg was overwritten — any cached global address for this
            // register is now stale.
            invalidateGlobalAddr(reg);
        }
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

    // Reuse cached global address register when available — avoids redundant
    // `la` when a prior LoadGlobal or StoreGlobal already loaded the address.
    if (enableOpt_) {
        const auto addrIt = globalAddrReg_.find(std::string(name));
        if (addrIt != globalAddrReg_.end()) {
            emitter_.instruction("lw", {*dstPhys, offsetReg(0, addrIt->second)});
            vregCache_.forgetVReg(dst);
            return true;
        }
    }

    emitter_.instruction("la", {"t0", globalLabel(name)});
    emitter_.instruction("lw", {*dstPhys, offsetReg(0, "t0")});
    vregCache_.clobberRegister("t0");
    vregCache_.forgetVReg(dst);
    if (enableOpt_) {
        invalidateGlobalAddr("t0");
        globalAddrReg_[std::string(name)] = "t0";
    }
    return true;
}

bool InstructionSelector::tryEmitThreeOperandBinaryOp(std::string_view dst,
                                                      std::string_view src1,
                                                      std::string_view src2,
                                                      std::string_view mnemonic) {
    if (!enableOpt_) {
        return false;
    }
    const std::optional<std::string_view> dstPhys = physicalReg(dst);
    const std::optional<std::string_view> src1Phys = physicalReg(src1);
    // Only apply when dst and src1 share a physical register.
    if (!dstPhys.has_value() || !src1Phys.has_value() || *dstPhys != *src1Phys) {
        return false;
    }

    // Determine the immediate/register mnemonic to use for the three-operand form.
    auto immMnemonic = [&]() -> std::string_view {
        if (mnemonic == "add") return "addi";
        if (mnemonic == "sub") return "addi";  // addi dst, dst, -imm
        if (mnemonic == "slt") return "slti";
        return "";
    }();

    // Try foldable immediate
    if (const auto immVal = foldableConst(src2); immVal.has_value() && !immMnemonic.empty()) {
        const std::int32_t effImm = (mnemonic == "sub") ? -(*immVal) : *immVal;
        emitter_.instruction(std::string(immMnemonic), {*dstPhys, *dstPhys, imm(effImm)});
        vregCache_.forgetVReg(dst);
        return true;
    }

    // Try physical register for src2
    if (const std::optional<std::string_view> src2Phys = physicalReg(src2)) {
        emitter_.instruction(std::string(mnemonic), {*dstPhys, *dstPhys, *src2Phys});
        vregCache_.forgetVReg(dst);
        return true;
    }

    // src2 on stack: load to t0, then three-operand
    loadVReg("t0", src2);
    vregCache_.clobberRegister("t0");
    emitter_.instruction(std::string(mnemonic), {*dstPhys, *dstPhys, "t0"});
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
    if (tryEmitThreeOperandBinaryOp(dst, src1, src2, mnemonic)) {
        return;
    }

    emitBinaryInputs(src1, src2);
    vregCache_.clobberRegister("t0");
    emitter_.instruction(mnemonic, {"t0", "t0", "t1"});
    storeVReg(dst, "t0");
}

bool InstructionSelector::tryEmitImmediateBinaryOp(std::string_view dst,
                                                   std::string_view src,
                                                   const std::int32_t immValue,
                                                   std::string_view immMnemonic) {
    // src must be a real value (never itself a folded-away constant).
    if (foldableConst(src).has_value()) {
        return false;
    }
    if (const std::optional<std::string_view> dstPhys = physicalReg(dst)) {
        if (const std::optional<std::string_view> srcPhys = physicalReg(src)) {
            emitter_.instruction(std::string(immMnemonic), {*dstPhys, *srcPhys, imm(immValue)});
            vregCache_.forgetVReg(dst);
            return true;
        }
        loadVReg(*dstPhys, src);
        emitter_.instruction(std::string(immMnemonic), {*dstPhys, *dstPhys, imm(immValue)});
        vregCache_.forgetVReg(dst);
        return true;
    }

    // dst lives on the stack: compute into t0, then store.
    vregCache_.clobberRegister("t0");
    if (const std::optional<std::string_view> srcPhys = physicalReg(src)) {
        emitter_.instruction(std::string(immMnemonic), {"t0", *srcPhys, imm(immValue)});
    } else {
        loadVReg("t0", src);
        emitter_.instruction(std::string(immMnemonic), {"t0", "t0", imm(immValue)});
    }
    storeVReg(dst, "t0");
    return true;
}

namespace {

// Returns floor(log2(n)) if n is a positive power of two, else -1.
int ilog2(std::int32_t n) {
    if (n <= 0) return -1;
    int log = 0;
    while (n > 1) {
        if (n & 1) return -1;
        n >>= 1;
        ++log;
    }
    return log;
}

bool isPowerOfTwo(std::int32_t n) {
    return n > 0 && ilog2(n) >= 0;
}

} // namespace

bool InstructionSelector::tryEmitStrengthReducedMul(std::string_view dst,
                                                     std::string_view src,
                                                     const std::int32_t multiplier) {
    if (!enableOpt_ || foldableConst(src).has_value()) {
        return false;
    }
    if (const int shift = ilog2(multiplier); shift >= 0 && shift <= 11) {
        // *2^k → slli
        if (shift == 1) return false; // *2 already handled as add x,x by IrOptimizer
        if (const std::optional<std::string_view> dstPhys = physicalReg(dst)) {
            if (const std::optional<std::string_view> srcPhys = physicalReg(src)) {
                emitter_.instruction("slli", {*dstPhys, *srcPhys, imm(shift)});
                vregCache_.forgetVReg(dst);
                return true;
            }
            loadVReg(*dstPhys, src);
            emitter_.instruction("slli", {*dstPhys, *dstPhys, imm(shift)});
            vregCache_.forgetVReg(dst);
            return true;
        }
        vregCache_.clobberRegister("t0");
        if (const std::optional<std::string_view> srcPhys = physicalReg(src)) {
            emitter_.instruction("slli", {"t0", *srcPhys, imm(shift)});
        } else {
            loadVReg("t0", src);
            emitter_.instruction("slli", {"t0", "t0", imm(shift)});
        }
        storeVReg(dst, "t0");
        return true;
    }

    // Small-constant expansions: shift + add/sub. Uses up to three temps;
    // the source register is NOT clobbered (we copy it to a safe slot if it
    // overlaps with the scratch registers we use for intermediate values).
    auto emitSR = [&](std::int32_t s1, std::int32_t s2, bool add) {
        vregCache_.clobberRegister("t0");
        vregCache_.clobberRegister("t2");
        vregCache_.clobberRegister("t3");
        auto srcReg = physicalReg(src);
        const bool srcConflicts = srcReg && (*srcReg == "t0" || *srcReg == "t2" || *srcReg == "t3");
        if (srcConflicts) {
            // Copy source to t1 so we can use t0/t2/t3 as scratch freely.
            loadVReg("t1", src);
            srcReg = "t1";
            vregCache_.clobberRegister("t1");
        }
        if (srcReg) {
            emitter_.instruction("slli", {"t3", *srcReg, imm(s1)});
            emitter_.instruction("slli", {"t2", *srcReg, imm(s2)});
            emitter_.instruction(add ? "add" : "sub", {"t0", "t3", "t2"});
        } else {
            loadVReg("t0", src);
            loadVReg("t2", src);
            emitter_.instruction("slli", {"t0", "t0", imm(s1)});
            emitter_.instruction("slli", {"t2", "t2", imm(s2)});
            emitter_.instruction(add ? "add" : "sub", {"t0", "t0", "t2"});
        }
        storeVReg(dst, "t0");
    };

    // *3 = (x << 1) + x  →  slli t0, src, 1 ; add t0, t0, src
    // *5 = (x << 2) + x
    // *6 = (x << 2) + (x << 1)
    // *9 = (x << 3) + x
    // *10 = (x << 3) + (x << 1)
    // For *3, *5, *9: x*K = (x << shift) + x.  Needs both shifted and
    // unshifted source without clobbering it.
    auto emitShiftAdd = [&](int shift) -> bool {
        vregCache_.clobberRegister("t0");
        const auto srcPhys = physicalReg(src);
        if (srcPhys && *srcPhys != "t0") {
            // Source has a physical register that won't conflict with t0.
            emitter_.instruction("slli", {"t0", *srcPhys, imm(shift)});
            emitter_.instruction("add", {"t0", "t0", *srcPhys});
        } else if (srcPhys && *srcPhys == "t0") {
            // Source IS t0; copy to t1, then shift + add.
            loadVReg("t1", src);
            vregCache_.clobberRegister("t1");
            emitter_.instruction("slli", {"t0", "t1", imm(shift)});
            emitter_.instruction("add", {"t0", "t0", "t1"});
        } else {
            // Source on stack; need two loads.
            loadVReg("t0", src);
            loadVReg("t1", src);
            vregCache_.clobberRegister("t1");
            emitter_.instruction("slli", {"t0", "t0", imm(shift)});
            emitter_.instruction("add", {"t0", "t0", "t1"});
        }
        storeVReg(dst, "t0");
        return true;
    };

    switch (multiplier) {
    case 3: return emitShiftAdd(1);
    case 5: return emitShiftAdd(2);
    case 6: emitSR(2, 1, true); return true;
    case 9: return emitShiftAdd(3);
    case 10: emitSR(3, 1, true); return true;
    default: return false;
    }
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
                // Foldable immediate constants are never materialized; their
                // uses are emitted as immediate operands instead.
                if (foldableConst(inst.dst).has_value()) {
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
                    invalidateGlobalAddr("t0");
                    storeVReg(inst.dst, "t0");
                    return;
                }
                emitter_.instruction("li", {"t0", imm(inst.value)});
                invalidateGlobalAddr("t0");
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
                    // t0 holds address after la; track for later reuse.
                    if (enableOpt_) {
                        globalAddrReg_[inst.name] = "t0";
                    }
                    return;
                }
                emitter_.instruction("la", {"t0", globalLabel(inst.name)});
                emitter_.instruction("lw", {"t1", offsetReg(0, "t0")});
                vregCache_.clobberRegister("t0");
                vregCache_.clobberRegister("t1");
                storeVReg(inst.dst, "t1");
                if (enableOpt_) {
                    invalidateGlobalAddr("t0");
                    globalAddrReg_[inst.name] = "t0";
                }
            } else if constexpr (std::is_same_v<Inst, contract::StoreGlobalInst>) {
                // Under -opt: try to reuse a cached global address so we don't
                // emit a second `la`.  Non-opt keeps the original pattern.
                //
                // Use the source's physical register directly (if available)
                // to avoid an unnecessary `mv` into a temp before the store.
                const std::optional<std::string_view> srcPhys =
                    enableOpt_ ? physicalReg(inst.src) : std::nullopt;

                auto addrIt = enableOpt_ ? globalAddrReg_.find(inst.name)
                                         : globalAddrReg_.end();
                if (addrIt != globalAddrReg_.end()) {
                    if (srcPhys.has_value()) {
                        emitter_.instruction("sw", {*srcPhys, offsetReg(0, addrIt->second)});
                        vregCache_.forgetVReg(inst.src);
                    } else {
                        loadVReg("t1", inst.src);
                        vregCache_.clobberRegister("t1");
                        emitter_.instruction("sw", {"t1", offsetReg(0, addrIt->second)});
                    }
                } else if (enableOpt_) {
                    emitter_.instruction("la", {"t0", globalLabel(inst.name)});
                    if (srcPhys.has_value()) {
                        emitter_.instruction("sw", {*srcPhys, offsetReg(0, "t0")});
                        vregCache_.forgetVReg(inst.src);
                    } else {
                        loadVReg("t1", inst.src);
                        vregCache_.clobberRegister("t1");
                        emitter_.instruction("sw", {"t1", offsetReg(0, "t0")});
                    }
                    vregCache_.clobberRegister("t0");
                    invalidateGlobalAddr("t0");
                    globalAddrReg_[inst.name] = "t0";
                } else {
                    loadVReg("t0", inst.src);
                    emitter_.instruction("la", {"t1", globalLabel(inst.name)});
                    emitter_.instruction("sw", {"t0", offsetReg(0, "t1")});
                    vregCache_.clobberRegister("t0");
                    vregCache_.clobberRegister("t1");
                }
            } else if constexpr (std::is_same_v<Inst, contract::CallInst>) {
                vregCache_.invalidateAll();
                globalAddrReg_.clear();
                abi_.emitCallArgs(inst.args);
                emitter_.instruction("call", {inst.functionName});
                abi_.emitCallCleanup(inst.args.size());
                vregCache_.invalidateAll();
                storeVReg(inst.dst, "a0");
            } else if constexpr (std::is_same_v<Inst, contract::CallVoidInst>) {
                vregCache_.invalidateAll();
                globalAddrReg_.clear();
                abi_.emitCallArgs(inst.args);
                emitter_.instruction("call", {inst.functionName});
                abi_.emitCallCleanup(inst.args.size());
                vregCache_.invalidateAll();
            } else if constexpr (std::is_same_v<Inst, contract::AddInst>) {
                if (const auto v = foldableConst(inst.src2)) {
                    if (tryEmitImmediateBinaryOp(inst.dst, inst.src1, *v, "addi")) return;
                }
                if (const auto v = foldableConst(inst.src1)) {
                    if (tryEmitImmediateBinaryOp(inst.dst, inst.src2, *v, "addi")) return;
                }
                emitBinaryOp(inst.dst, inst.src1, inst.src2, "add");
            } else if constexpr (std::is_same_v<Inst, contract::SubInst>) {
                if (const auto v = foldableConst(inst.src2)) {
                    if (tryEmitImmediateBinaryOp(inst.dst, inst.src1, -*v, "addi")) return;
                }
                emitBinaryOp(inst.dst, inst.src1, inst.src2, "sub");
            } else if constexpr (std::is_same_v<Inst, contract::MulInst>) {
                // MulInst by constant already folds to AddInst (*2) or slli
                // in IrOptimizer; strength reduction here handles the
                // remaining constant-multiply cases at selection time.
                if (const auto v = foldableConst(inst.src2)) {
                    if (tryEmitStrengthReducedMul(inst.dst, inst.src1, *v)) return;
                }
                if (const auto v = foldableConst(inst.src1)) {
                    if (tryEmitStrengthReducedMul(inst.dst, inst.src2, *v)) return;
                }
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
                // x == 0  ->  seqz dst, x  (no constant materialization)
                std::string_view other;
                if (foldableConst(inst.src1) == std::optional<std::int32_t>(0)) {
                    other = inst.src2;
                } else if (foldableConst(inst.src2) == std::optional<std::int32_t>(0)) {
                    other = inst.src1;
                }
                if (!other.empty()) {
                    if (const std::optional<std::string_view> dstPhys = physicalReg(inst.dst)) {
                        if (const std::optional<std::string_view> srcPhys = physicalReg(other)) {
                            emitter_.instruction("seqz", {*dstPhys, *srcPhys});
                            vregCache_.forgetVReg(inst.dst);
                            return;
                        }
                    }
                    loadVReg("t0", other);
                    vregCache_.clobberRegister("t0");
                    emitter_.instruction("seqz", {"t0", "t0"});
                    storeVReg(inst.dst, "t0");
                    return;
                }
                if (tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "eq")) {
                    return;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("sub", {"t0", "t0", "t1"});
                emitter_.instruction("seqz", {"t0", "t0"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::NeInst>) {
                // x != 0  ->  snez dst, x
                std::string_view other;
                if (foldableConst(inst.src1) == std::optional<std::int32_t>(0)) {
                    other = inst.src2;
                } else if (foldableConst(inst.src2) == std::optional<std::int32_t>(0)) {
                    other = inst.src1;
                }
                if (!other.empty()) {
                    if (const std::optional<std::string_view> dstPhys = physicalReg(inst.dst)) {
                        if (const std::optional<std::string_view> srcPhys = physicalReg(other)) {
                            emitter_.instruction("snez", {*dstPhys, *srcPhys});
                            vregCache_.forgetVReg(inst.dst);
                            return;
                        }
                    }
                    loadVReg("t0", other);
                    vregCache_.clobberRegister("t0");
                    emitter_.instruction("snez", {"t0", "t0"});
                    storeVReg(inst.dst, "t0");
                    return;
                }
                if (tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "ne")) {
                    return;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("sub", {"t0", "t0", "t1"});
                emitter_.instruction("snez", {"t0", "t0"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::LtInst>) {
                if (const auto v = foldableConst(inst.src2)) {
                    if (tryEmitImmediateBinaryOp(inst.dst, inst.src1, *v, "slti")) return;
                }
                if (!tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "lt")) {
                    emitBinaryOp(inst.dst, inst.src1, inst.src2, "slt");
                }
            } else if constexpr (std::is_same_v<Inst, contract::LeInst>) {
                // x <= C ⟺ x < C+1 → slti (when C+1 fits in 12-bit imm)
                if (const auto v = foldableConst(inst.src2)) {
                    if (*v < 2047 && tryEmitImmediateBinaryOp(
                        inst.dst, inst.src1, *v + 1, "slti")) {
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
                // x > C ⟺ !(x < C+1) → slti t0, x, C+1; xori t0, t0, 1
                if (const auto v = foldableConst(inst.src2)) {
                    if (*v < 2047 && tryEmitImmediateBinaryOp(
                        inst.dst, inst.src1, *v + 1, "slti")) {
                        // Immediate form emitted; invert the result.
                        const auto dstPhys = physicalReg(inst.dst);
                        const std::string dstReg = dstPhys ? std::string(*dstPhys) : "t0";
                        emitter_.instruction("xori", {dstReg, dstReg, "1"});
                        return;
                    }
                }
                if (tryEmitPhysicalCompareOp(inst.dst, inst.src1, inst.src2, "gt")) {
                    return;
                }
                emitBinaryInputs(inst.src1, inst.src2);
                vregCache_.clobberRegister("t0");
                emitter_.instruction("slt", {"t0", "t1", "t0"});
                storeVReg(inst.dst, "t0");
            } else if constexpr (std::is_same_v<Inst, contract::GeInst>) {
                // x >= C ⟺ !(x < C) → slti t0, x, C; xori t0, t0, 1
                if (const auto v = foldableConst(inst.src2)) {
                    if (tryEmitImmediateBinaryOp(inst.dst, inst.src1, *v, "slti")) {
                        const auto dstPhys = physicalReg(inst.dst);
                        const std::string dstReg = dstPhys ? std::string(*dstPhys) : "t0";
                        emitter_.instruction("xori", {dstReg, dstReg, "1"});
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
                // x == 0  ->  beqz x
                if (const std::string_view z = zeroCompareOperand(inst.src1, inst.src2);
                    !z.empty()) {
                    if (const std::optional<std::string_view> zPhys = physicalReg(z)) {
                        emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "beqz",
                                     *zPhys);
                    } else {
                        loadVReg("t0", z);
                        emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "beqz",
                                     "t0");
                    }
                    vregCache_.invalidateAll();
                    return true;
                }
                if (hasFoldableOperand(inst.src1, inst.src2)) {
                    return false; // let the immediate-aware emit path handle it
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
                // x != 0  ->  bnez x
                if (const std::string_view z = zeroCompareOperand(inst.src1, inst.src2);
                    !z.empty()) {
                    if (const std::optional<std::string_view> zPhys = physicalReg(z)) {
                        emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez",
                                     *zPhys);
                    } else {
                        loadVReg("t0", z);
                        emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez",
                                     "t0");
                    }
                    vregCache_.invalidateAll();
                    return true;
                }
                if (hasFoldableOperand(inst.src1, inst.src2)) {
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
                // x < C  ->  slti t0, x, C ; bnez t0
                if (const auto v = foldableConst(inst.src2)) {
                    vregCache_.clobberRegister("t0");
                    if (const std::optional<std::string_view> srcPhys = physicalReg(inst.src1)) {
                        emitter_.instruction("slti", {"t0", *srcPhys, imm(*v)});
                    } else {
                        loadVReg("t0", inst.src1);
                        emitter_.instruction("slti", {"t0", "t0", imm(*v)});
                    }
                    emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez", "t0");
                    vregCache_.invalidateAll();
                    return true;
                }
                if (hasFoldableOperand(inst.src1, inst.src2)) {
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
                // x <= C ⟺ x < C+1 → slti ; bnez trueLabel
                if (const auto v = foldableConst(inst.src2); v && *v < 2047) {
                    loadVReg("t0", inst.src1);
                    vregCache_.clobberRegister("t0");
                    emitter_.instruction("slti", {"t0", "t0", imm(*v + 1)});
                    emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "bnez", "t0");
                    vregCache_.invalidateAll();
                    return true;
                }
                if (hasFoldableOperand(inst.src1, inst.src2)) {
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
                // x > C ⟺ !(x < C+1) → slti t0, x, C+1 ; beqz t0, true
                if (const auto v = foldableConst(inst.src2); v && *v < 2047) {
                    loadVReg("t0", inst.src1);
                    vregCache_.clobberRegister("t0");
                    emitter_.instruction("slti", {"t0", "t0", imm(*v + 1)});
                    emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "beqz", "t0");
                    vregCache_.invalidateAll();
                    return true;
                }
                if (hasFoldableOperand(inst.src1, inst.src2)) {
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
                // x >= C ⟺ !(x < C) → slti t0, x, C ; beqz t0, true
                if (const auto v = foldableConst(inst.src2)) {
                    loadVReg("t0", inst.src1);
                    vregCache_.clobberRegister("t0");
                    emitter_.instruction("slti", {"t0", "t0", imm(*v)});
                    emitBranchTo(functionName, branch.trueLabel, branch.falseLabel, "beqz", "t0");
                    vregCache_.invalidateAll();
                    return true;
                }
                if (hasFoldableOperand(inst.src1, inst.src2)) {
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
                if (foldableConst(inst.src).has_value()) {
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
                // Jumps to "entry" from a non-entry block are tail-recursion
                // back-edges — redirect past the prologue to the body label.
                const std::string target =
                    (inst.targetLabel == "entry")
                        ? blockLabel(functionName, "tail_entry")
                        : blockLabel(functionName, inst.targetLabel);
                emitter_.instruction("j", {target});
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
