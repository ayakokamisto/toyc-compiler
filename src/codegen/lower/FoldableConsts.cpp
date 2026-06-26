#include "codegen/lower/FoldableConsts.h"

#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace toyc::codegen {

namespace {

namespace c = contract;

// RISC-V I-type immediates are 12-bit signed. Keep a symmetric range so the
// `sub dst, src, -c` rewrite stays in range too.
constexpr std::int32_t kImmMin = -2047;
constexpr std::int32_t kImmMax = 2047;

bool inImmRange(std::int32_t value) {
    return value >= kImmMin && value <= kImmMax;
}

} // namespace

std::unordered_map<std::string, std::int32_t>
computeFoldableImmediateConsts(const c::IRFunction& function) {
    // candidate value + single-definition tracking
    std::unordered_map<std::string, std::int32_t> constValue;
    std::unordered_map<std::string, int> defCount;
    std::unordered_set<std::string> disqualified;

    auto disqualify = [&](const std::string& v) {
        if (!v.empty()) {
            disqualified.insert(v);
        }
    };

    // Parameters are never compile-time constants here.
    for (const c::Param& param : function.params) {
        disqualify(param.vreg);
    }

    // Pass 1: record const definitions and disqualify multiply-defined vregs.
    for (const c::BasicBlock& block : function.basicBlocks) {
        for (const c::Instruction& inst : block.instructions) {
            std::visit(
                [&](const auto& i) {
                    using T = std::decay_t<decltype(i)>;
                    if constexpr (std::is_same_v<T, c::ConstInst>) {
                        ++defCount[i.dst];
                        if (inImmRange(i.value)) {
                            constValue[i.dst] = i.value;
                        } else {
                            disqualify(i.dst);
                        }
                    } else if constexpr (std::is_same_v<T, c::CopyInst> ||
                                         std::is_same_v<T, c::LoadGlobalInst> ||
                                         std::is_same_v<T, c::CallInst> ||
                                         std::is_same_v<T, c::AddInst> ||
                                         std::is_same_v<T, c::SubInst> ||
                                         std::is_same_v<T, c::MulInst> ||
                                         std::is_same_v<T, c::DivInst> ||
                                         std::is_same_v<T, c::ModInst> ||
                                         std::is_same_v<T, c::NegInst> ||
                                         std::is_same_v<T, c::EqInst> ||
                                         std::is_same_v<T, c::NeInst> ||
                                         std::is_same_v<T, c::LtInst> ||
                                         std::is_same_v<T, c::LeInst> ||
                                         std::is_same_v<T, c::GtInst> ||
                                         std::is_same_v<T, c::GeInst> ||
                                         std::is_same_v<T, c::LNotInst>) {
                        // A non-const definition of this vreg disqualifies it.
                        ++defCount[i.dst];
                        disqualify(i.dst);
                    }
                },
                inst);
        }
    }

    std::unordered_map<std::string, int> useCount;
    auto noteUse = [&](const std::string& v) {
        if (!v.empty()) {
            ++useCount[v];
        }
    };

    // A use is "immediate-eligible" only in specific operand positions. Any
    // other use disqualifies the constant, because once it is excluded from
    // register allocation there is no fallback materialization.
    for (const c::BasicBlock& block : function.basicBlocks) {
        for (const c::Instruction& inst : block.instructions) {
            std::visit(
                [&](const auto& i) {
                    using T = std::decay_t<decltype(i)>;
                    if constexpr (std::is_same_v<T, c::AddInst>) {
                        // commutative: const on either side -> addi
                        noteUse(i.src1);
                        noteUse(i.src2);
                    } else if constexpr (std::is_same_v<T, c::SubInst>) {
                        // only src2 const -> addi dst, src1, -c; src1 const cannot fold
                        noteUse(i.src2);
                        disqualify(i.src1);
                    } else if constexpr (std::is_same_v<T, c::LtInst>) {
                        // only src2 const -> slti; src1 const cannot fold
                        noteUse(i.src2);
                        disqualify(i.src1);
                    } else if constexpr (std::is_same_v<T, c::EqInst> ||
                                         std::is_same_v<T, c::NeInst>) {
                        // foldable only when the constant operand is exactly 0
                        auto eligibleZero = [&](const std::string& v) {
                            const auto it = constValue.find(v);
                            return it != constValue.end() && it->second == 0;
                        };
                        if (eligibleZero(i.src1)) {
                            noteUse(i.src1);
                        } else {
                            disqualify(i.src1);
                        }
                        if (eligibleZero(i.src2)) {
                            noteUse(i.src2);
                        } else {
                            disqualify(i.src2);
                        }
                    } else if constexpr (std::is_same_v<T, c::MulInst>) {
                        // A constant multiplier is eligible only if it can be
                        // strength-reduced (power of two or small-constant
                        // expandable to shift+add).
                        auto mulEligible = [&](const std::string& op) {
                            const auto it = constValue.find(op);
                            if (it == constValue.end()) return false;
                            const std::int32_t v = it->second;
                            if (v <= 0) return false;
                            // power of two
                            if ((v & (v - 1)) == 0) return true;
                            // small constants with shift+add expansions
                            return v == 3 || v == 5 || v == 6 || v == 9 || v == 10;
                        };
                        if (mulEligible(i.src1)) noteUse(i.src1); else disqualify(i.src1);
                        if (mulEligible(i.src2)) noteUse(i.src2); else disqualify(i.src2);
                    } else if constexpr (std::is_same_v<T, c::DivInst> ||
                                         std::is_same_v<T, c::ModInst> ||
                                         std::is_same_v<T, c::LeInst> ||
                                         std::is_same_v<T, c::GtInst> ||
                                         std::is_same_v<T, c::GeInst>) {
                        disqualify(i.src1);
                        disqualify(i.src2);
                    } else if constexpr (std::is_same_v<T, c::CopyInst> ||
                                         std::is_same_v<T, c::NegInst> ||
                                         std::is_same_v<T, c::LNotInst> ||
                                         std::is_same_v<T, c::StoreGlobalInst>) {
                        disqualify(i.src);
                    } else if constexpr (std::is_same_v<T, c::CallInst> ||
                                         std::is_same_v<T, c::CallVoidInst>) {
                        for (const std::string& arg : i.args) {
                            disqualify(arg);
                        }
                    }
                },
                inst);
        }

        // Terminator operands (branch cond, return value) need a real register.
        std::visit(
            [&](const auto& t) {
                using T = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<T, c::BranchInst>) {
                    disqualify(t.cond);
                } else if constexpr (std::is_same_v<T, c::ReturnInst>) {
                    if (t.src.has_value()) {
                        disqualify(*t.src);
                    }
                }
            },
            block.terminator);
    }

    std::unordered_map<std::string, std::int32_t> result;
    for (const auto& [vreg, value] : constValue) {
        if (disqualified.count(vreg) != 0) {
            continue;
        }
        if (defCount[vreg] != 1) {
            continue;
        }
        if (useCount[vreg] == 0) {
            continue; // unused; DCE handles it, no point folding
        }
        result.emplace(vreg, value);
    }
    return result;
}

} // namespace toyc::codegen
