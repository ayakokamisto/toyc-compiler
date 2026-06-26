#include "codegen/opt/IrOptimizer.h"

#include "codegen/frame/VRegAnalysis.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace toyc::codegen {

namespace {

namespace c = contract;

using ConstMap = std::unordered_map<std::string, std::int32_t>;
using CopyMap = std::unordered_map<std::string, std::string>;

std::int32_t wrapAdd(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t wrapSub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t wrapMul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

// Returns the destination vreg an instruction defines, or empty if none.
std::string defOf(const c::Instruction& inst) {
    return std::visit(
        [](const auto& i) -> std::string {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, c::ConstInst> || std::is_same_v<T, c::CopyInst> ||
                          std::is_same_v<T, c::LoadGlobalInst> || std::is_same_v<T, c::CallInst> ||
                          std::is_same_v<T, c::AddInst> || std::is_same_v<T, c::SubInst> ||
                          std::is_same_v<T, c::MulInst> || std::is_same_v<T, c::DivInst> ||
                          std::is_same_v<T, c::ModInst> || std::is_same_v<T, c::NegInst> ||
                          std::is_same_v<T, c::EqInst> || std::is_same_v<T, c::NeInst> ||
                          std::is_same_v<T, c::LtInst> || std::is_same_v<T, c::LeInst> ||
                          std::is_same_v<T, c::GtInst> || std::is_same_v<T, c::GeInst> ||
                          std::is_same_v<T, c::LNotInst>) {
                return i.dst;
            } else {
                return std::string{};
            }
        },
        inst);
}

bool isPure(const c::Instruction& inst) {
    return std::visit(
        [](const auto& i) {
            using T = std::decay_t<decltype(i)>;
            return !(std::is_same_v<T, c::CallInst> || std::is_same_v<T, c::CallVoidInst> ||
                     std::is_same_v<T, c::StoreGlobalInst>);
        },
        inst);
}

template <typename Fn>
void forEachUse(c::Instruction& inst, Fn fn) {
    std::visit(
        [&](auto& i) {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, c::CopyInst> || std::is_same_v<T, c::StoreGlobalInst> ||
                          std::is_same_v<T, c::NegInst> || std::is_same_v<T, c::LNotInst>) {
                fn(i.src);
            } else if constexpr (std::is_same_v<T, c::AddInst> || std::is_same_v<T, c::SubInst> ||
                                 std::is_same_v<T, c::MulInst> || std::is_same_v<T, c::DivInst> ||
                                 std::is_same_v<T, c::ModInst> || std::is_same_v<T, c::EqInst> ||
                                 std::is_same_v<T, c::NeInst> || std::is_same_v<T, c::LtInst> ||
                                 std::is_same_v<T, c::LeInst> || std::is_same_v<T, c::GtInst> ||
                                 std::is_same_v<T, c::GeInst>) {
                fn(i.src1);
                fn(i.src2);
            } else if constexpr (std::is_same_v<T, c::CallInst> ||
                                 std::is_same_v<T, c::CallVoidInst>) {
                for (std::string& arg : i.args) {
                    fn(arg);
                }
            }
        },
        inst);
}

template <typename Fn>
void forEachUse(c::Terminator& term, Fn fn) {
    std::visit(
        [&](auto& t) {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, c::BranchInst>) {
                fn(t.cond);
            } else if constexpr (std::is_same_v<T, c::ReturnInst>) {
                if (t.src.has_value()) {
                    fn(*t.src);
                }
            }
        },
        term);
}

std::string resolve(const CopyMap& copies, const std::string& name) {
    std::string cur = name;
    // Bounded chase; copy chains are short and acyclic by construction.
    for (int i = 0; i < 64; ++i) {
        const auto it = copies.find(cur);
        if (it == copies.end()) {
            break;
        }
        cur = it->second;
    }
    return cur;
}

// Drop all facts about a redefined vreg, including copies that pointed at it.
void invalidateDef(const std::string& dst, ConstMap& consts, CopyMap& copies) {
    consts.erase(dst);
    copies.erase(dst);
    for (auto it = copies.begin(); it != copies.end();) {
        if (it->second == dst) {
            it = copies.erase(it);
        } else {
            ++it;
        }
    }
}

std::optional<std::int32_t> getConst(const ConstMap& consts, const std::string& name) {
    const auto it = consts.find(name);
    if (it == consts.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::int32_t> foldBinary(std::string_view op, std::int32_t a, std::int32_t b) {
    constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
    if (op == "add") return wrapAdd(a, b);
    if (op == "sub") return wrapSub(a, b);
    if (op == "mul") return wrapMul(a, b);
    if (op == "div") {
        if (b == 0) return std::nullopt;               // div by zero is UB; leave it
        if (a == kMin && b == -1) return kMin;          // match RISC-V semantics
        return a / b;
    }
    if (op == "rem") {
        if (b == 0) return std::nullopt;
        if (a == kMin && b == -1) return 0;
        return a % b;
    }
    if (op == "eq") return a == b ? 1 : 0;
    if (op == "ne") return a != b ? 1 : 0;
    if (op == "lt") return a < b ? 1 : 0;
    if (op == "le") return a <= b ? 1 : 0;
    if (op == "gt") return a > b ? 1 : 0;
    if (op == "ge") return a >= b ? 1 : 0;
    return std::nullopt;
}

// Maps a binary instruction type to its fold mnemonic.
template <typename T>
constexpr std::string_view binaryOpName() {
    if constexpr (std::is_same_v<T, c::AddInst>) return "add";
    else if constexpr (std::is_same_v<T, c::SubInst>) return "sub";
    else if constexpr (std::is_same_v<T, c::MulInst>) return "mul";
    else if constexpr (std::is_same_v<T, c::DivInst>) return "div";
    else if constexpr (std::is_same_v<T, c::ModInst>) return "rem";
    else if constexpr (std::is_same_v<T, c::EqInst>) return "eq";
    else if constexpr (std::is_same_v<T, c::NeInst>) return "ne";
    else if constexpr (std::is_same_v<T, c::LtInst>) return "lt";
    else if constexpr (std::is_same_v<T, c::LeInst>) return "le";
    else if constexpr (std::is_same_v<T, c::GtInst>) return "gt";
    else if constexpr (std::is_same_v<T, c::GeInst>) return "ge";
    else return "";
}

// Constant-fold or algebraically simplify a single instruction in place.
// Returns true if the instruction was replaced.
bool tryFold(c::Instruction& inst, const ConstMap& consts) {
    return std::visit(
        [&](auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;

            // copy of a known constant becomes a const materialization
            if constexpr (std::is_same_v<T, c::CopyInst>) {
                if (const auto v = getConst(consts, i.src)) {
                    inst = c::ConstInst{i.dst, *v};
                    return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, c::NegInst>) {
                if (const auto v = getConst(consts, i.src)) {
                    inst = c::ConstInst{i.dst, wrapSub(0, *v)};
                    return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, c::LNotInst>) {
                if (const auto v = getConst(consts, i.src)) {
                    inst = c::ConstInst{i.dst, *v == 0 ? 1 : 0};
                    return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, c::AddInst> || std::is_same_v<T, c::SubInst> ||
                                 std::is_same_v<T, c::MulInst> || std::is_same_v<T, c::DivInst> ||
                                 std::is_same_v<T, c::ModInst> || std::is_same_v<T, c::EqInst> ||
                                 std::is_same_v<T, c::NeInst> || std::is_same_v<T, c::LtInst> ||
                                 std::is_same_v<T, c::LeInst> || std::is_same_v<T, c::GtInst> ||
                                 std::is_same_v<T, c::GeInst>) {
                constexpr std::string_view op = binaryOpName<T>();
                const std::string dst = i.dst;
                const std::string src1 = i.src1;
                const std::string src2 = i.src2;
                const auto a = getConst(consts, src1);
                const auto b = getConst(consts, src2);

                // both constant: full fold
                if (a && b) {
                    if (const auto r = foldBinary(op, *a, *b)) {
                        inst = c::ConstInst{dst, *r};
                        return true;
                    }
                    return false;
                }

                // algebraic identities (only for arithmetic, never compares)
                if constexpr (std::is_same_v<T, c::AddInst>) {
                    if (a && *a == 0) { inst = c::CopyInst{dst, src2}; return true; }
                    if (b && *b == 0) { inst = c::CopyInst{dst, src1}; return true; }
                } else if constexpr (std::is_same_v<T, c::SubInst>) {
                    if (b && *b == 0) { inst = c::CopyInst{dst, src1}; return true; }
                    if (src1 == src2) { inst = c::ConstInst{dst, 0}; return true; }
                } else if constexpr (std::is_same_v<T, c::MulInst>) {
                    if ((a && *a == 0) || (b && *b == 0)) { inst = c::ConstInst{dst, 0}; return true; }
                    if (a && *a == 1) { inst = c::CopyInst{dst, src2}; return true; }
                    if (b && *b == 1) { inst = c::CopyInst{dst, src1}; return true; }
                    if (a && *a == 2) { inst = c::AddInst{dst, src2, src2}; return true; }
                    if (b && *b == 2) { inst = c::AddInst{dst, src1, src1}; return true; }
                } else if constexpr (std::is_same_v<T, c::DivInst>) {
                    if (b && *b == 1) { inst = c::CopyInst{dst, src1}; return true; }
                } else if constexpr (std::is_same_v<T, c::ModInst>) {
                    if (b && *b == 1) { inst = c::ConstInst{dst, 0}; return true; }
                }
                return false;
            } else {
                return false;
            }
        },
        inst);
}

// Forward pass over a block: copy propagation + constant folding/algebraic.
bool simplifyBlock(c::BasicBlock& block) {
    ConstMap consts;
    CopyMap copies;
    bool changed = false;

    for (c::Instruction& inst : block.instructions) {
        forEachUse(inst, [&](std::string& operand) {
            const std::string resolved = resolve(copies, operand);
            if (resolved != operand) {
                operand = resolved;
                changed = true;
            }
        });

        if (tryFold(inst, consts)) {
            changed = true;
        }

        const std::string dst = defOf(inst);
        if (!dst.empty()) {
            invalidateDef(dst, consts, copies);
        }

        std::visit(
            [&](auto& i) {
                using T = std::decay_t<decltype(i)>;
                if constexpr (std::is_same_v<T, c::ConstInst>) {
                    consts[i.dst] = i.value;
                } else if constexpr (std::is_same_v<T, c::CopyInst>) {
                    copies[i.dst] = i.src;
                }
            },
            inst);
    }

    forEachUse(block.terminator, [&](std::string& operand) {
        const std::string resolved = resolve(copies, operand);
        if (resolved != operand) {
            operand = resolved;
            changed = true;
        }
    });

    return changed;
}

// Backward DCE within each block: drop pure defs not used later or live-out.
bool deadCodeElim(c::IRFunction& function) {
    const VRegAnalysis analysis = analyzeVRegs(function);
    bool changed = false;

    for (c::BasicBlock& block : function.basicBlocks) {
        std::set<std::string, std::less<>> live;
        const auto outIt = analysis.liveOuts.find(block.label);
        if (outIt != analysis.liveOuts.end()) {
            live = outIt->second;
        }
        // The terminator's operands are used after the last instruction; seed
        // them so we never delete the def of a branch condition or return value.
        forEachUse(block.terminator, [&](std::string& operand) { live.insert(operand); });

        std::vector<c::Instruction> kept;
        kept.reserve(block.instructions.size());
        for (auto it = block.instructions.rbegin(); it != block.instructions.rend(); ++it) {
            c::Instruction& inst = *it;
            const std::string dst = defOf(inst);

            if (!dst.empty() && isPure(inst) && live.find(dst) == live.end()) {
                changed = true;
                continue; // dead: drop it
            }

            if (!dst.empty()) {
                live.erase(dst);
            }
            forEachUse(inst, [&](std::string& operand) { live.insert(operand); });
            kept.push_back(std::move(inst));
        }

        std::reverse(kept.begin(), kept.end());
        block.instructions = std::move(kept);
    }

    return changed;
}

// Copy coalescing: when CopyInst{dst, src} follows an instruction whose only
// output is `src`, and `src` has no other uses (not live-out, not read later
// in this block, not consumed by the terminator), retarget the preceding
// instruction to write directly to `dst` and drop the copy. This eliminates
// the pervasive `op temp, a, b; mv var, temp` pattern from every assignment.
bool coalesceCopies(c::IRFunction& function) {
    const VRegAnalysis analysis = analyzeVRegs(function);
    bool changed = false;

    for (c::BasicBlock& block : function.basicBlocks) {
        if (block.instructions.size() < 2) {
            continue;
        }

        // Precompute, for each instruction, which vregs are read by later
        // instructions and by the terminator. Walk backwards.
        std::set<std::string, std::less<>> laterUses;
        forEachUse(block.terminator, [&](std::string& operand) { laterUses.insert(operand); });

        std::vector<bool> copyIsDead(block.instructions.size(), false);

        for (std::size_t ri = block.instructions.size(); ri > 0; --ri) {
            const std::size_t i = ri - 1;
            c::Instruction& inst = block.instructions[i];
            const std::string dst = defOf(inst);

            // Is this a CopyInst whose source is defined by the previous
            // instruction and whose source has no other readers?
            if (const auto* copy = std::get_if<c::CopyInst>(&inst)) {
                if (i > 0 && !dst.empty()) {
                    const std::string& src = copy->src;
                    const c::Instruction& prev = block.instructions[i - 1];
                    if (defOf(prev) == src) {
                        // src must NOT be read by anything after this copy
                        if (laterUses.find(src) == laterUses.end()) {
                            // Also check live-out
                            const auto outIt = analysis.liveOuts.find(block.label);
                            const auto& liveOut = (outIt == analysis.liveOuts.end())
                                ? std::set<std::string, std::less<>>{}
                                : outIt->second;
                            if (liveOut.find(src) == liveOut.end()) {
                                // Safe to coalesce. Mark copy for deletion;
                                // the prev instruction's dst will be renamed
                                // when we rebuild the instruction list.
                                copyIsDead[i] = true;
                                changed = true;
                                // The copy's dst now replaces prev's dst as
                                // the "current" definition for later-use
                                // tracking. We already recorded laterUses
                                // before this point, so this doesn't affect
                                // correctness — it's just for the next
                                // iteration of the backward scan.
                            }
                        }
                    }
                }
            }

            // Update `laterUses` for the next (earlier) instruction: add this
            // instruction's operands, remove its def.
            if (!dst.empty()) {
                laterUses.erase(dst);
            }
            forEachUse(inst, [&](std::string& operand) { laterUses.insert(operand); });
        }

        // Second forward pass: rebuild the instruction list with coalescing
        // applied (retarget the prev instruction, drop dead copies).
        if (changed) {
            std::vector<c::Instruction> kept;
            kept.reserve(block.instructions.size());
            for (std::size_t i = 0; i < block.instructions.size(); ++i) {
                if (copyIsDead[i]) {
                    // The preceding instruction (kept.back()) defined the
                    // copy's source. Retarget it to the copy's destination.
                    const auto& copy = std::get<c::CopyInst>(block.instructions[i]);
                    const std::string& newDst = copy.dst;
                    std::visit(
                        [&](auto& prevInst) {
                            using T = std::decay_t<decltype(prevInst)>;
                            if constexpr (std::is_same_v<T, c::ConstInst>) {
                                prevInst.dst = newDst;
                            } else if constexpr (std::is_same_v<T, c::CopyInst>) {
                                prevInst.dst = newDst;
                            } else if constexpr (std::is_same_v<T, c::LoadGlobalInst>) {
                                prevInst.dst = newDst;
                            } else if constexpr (std::is_same_v<T, c::CallInst>) {
                                prevInst.dst = newDst;
                            } else if constexpr (std::is_same_v<T, c::AddInst> ||
                                                 std::is_same_v<T, c::SubInst> ||
                                                 std::is_same_v<T, c::MulInst> ||
                                                 std::is_same_v<T, c::DivInst> ||
                                                 std::is_same_v<T, c::ModInst>) {
                                prevInst.dst = newDst;
                            } else if constexpr (std::is_same_v<T, c::NegInst> ||
                                                 std::is_same_v<T, c::LNotInst>) {
                                prevInst.dst = newDst;
                            } else if constexpr (std::is_same_v<T, c::EqInst> ||
                                                 std::is_same_v<T, c::NeInst> ||
                                                 std::is_same_v<T, c::LtInst> ||
                                                 std::is_same_v<T, c::LeInst> ||
                                                 std::is_same_v<T, c::GtInst> ||
                                                 std::is_same_v<T, c::GeInst>) {
                                prevInst.dst = newDst;
                            }
                        },
                        kept.back());
                    continue; // drop the copy
                }
                kept.push_back(std::move(block.instructions[i]));
            }
            block.instructions = std::move(kept);
        }
    }

    return changed;
}

// Tail-recursion elimination: turn a self-recursive call at the end of a
// block (immediately followed by ReturnInst) into parameter-copy assignments
// and a jump to the function's entry-body label. The entry-body label
// (functionName__body) sits after the prologue and parameter landing, so the
// transformed branch skips the stack-frame setup and re-enters the function
// body with the new argument values.
bool eliminateTailRecursion(c::IRFunction& function) {
    bool changed = false;
    const std::string& funcName = function.name;

    for (c::BasicBlock& block : function.basicBlocks) {
        auto& insts = block.instructions;
        if (insts.empty()) {
            continue;
        }

        c::Instruction& last = insts.back();
        bool isTail = false;
        std::vector<std::string> callArgs;

        if (const auto* call = std::get_if<c::CallInst>(&last)) {
            if (call->functionName == funcName) {
                const auto* ret = std::get_if<c::ReturnInst>(&block.terminator);
                if (ret != nullptr && ret->src.has_value() && *ret->src == call->dst) {
                    isTail = true;
                    callArgs = call->args;
                }
            }
        } else if (const auto* callVoid = std::get_if<c::CallVoidInst>(&last)) {
            if (callVoid->functionName == funcName) {
                const auto* ret = std::get_if<c::ReturnInst>(&block.terminator);
                if (ret != nullptr && !ret->src.has_value()) {
                    isTail = true;
                    callArgs = callVoid->args;
                }
            }
        }

        if (!isTail) {
            continue;
        }

        // Remove the tail call.
        insts.pop_back();

        // Directly retarget the argument-defining instructions (when the
        // arg is produced by a single instruction in this block) to write
        // to the corresponding parameter vreg, avoiding intermediate copies.
        //
        // Important ordering constraint: when two arg-defining instructions
        // both read the same parameter vreg and one of them writes it, the
        // instruction that reads must execute BEFORE the one that writes
        // (otherwise the read gets the wrong, post-update value).
        struct Retarget { std::size_t index; std::string paramVreg; };
        std::vector<Retarget> retargets;
        for (std::size_t i = 0; i < callArgs.size() && i < function.params.size(); ++i) {
            const std::string& arg = callArgs[i];
            const std::string& paramVreg = function.params[i].vreg;
            if (arg == paramVreg) {
                continue;
            }
            bool found = false;
            for (std::size_t j = 0; j < insts.size(); ++j) {
                if (defOf(insts[j]) == arg) {
                    retargets.push_back({j, paramVreg});
                    found = true;
                    break;
                }
            }
            if (!found) {
                insts.push_back(c::CopyInst{paramVreg, arg});
            }
        }

        // Reorder: a retargeted instruction that reads param V should execute
        // BEFORE any retargeted instruction that writes V (the read needs the
        // old value). Use simple bubble — retargets typically ≤ 3 elements.
        bool reordered = true;
        while (reordered) {
            reordered = false;
            for (std::size_t i = 0; i + 1 < retargets.size(); ++i) {
                // right reads left's write target? → swap so right executes first
                bool rightReadsLeftWrite = [&]() {
                    bool reads = false;
                    forEachUse(const_cast<c::Instruction&>(insts[retargets[i + 1].index]),
                               [&](std::string& op) {
                                   if (op == retargets[i].paramVreg) reads = true;
                               });
                    return reads;
                }();
                if (rightReadsLeftWrite) {
                    std::swap(retargets[i], retargets[i + 1]);
                    std::iter_swap(
                        insts.begin() + static_cast<std::ptrdiff_t>(retargets[i].index),
                        insts.begin() + static_cast<std::ptrdiff_t>(retargets[i + 1].index));
                    // Fix up indices after the instruction swap.
                    std::swap(retargets[i].index, retargets[i + 1].index);
                    reordered = true;
                }
            }
        }

        // Apply retargets.
        for (const auto& rt : retargets) {
            std::visit(
                [&](auto& prev) {
                    using T = std::decay_t<decltype(prev)>;
                    if constexpr (std::is_same_v<T, c::ConstInst> || std::is_same_v<T, c::CopyInst> ||
                                  std::is_same_v<T, c::LoadGlobalInst> || std::is_same_v<T, c::CallInst> ||
                                  std::is_same_v<T, c::AddInst> || std::is_same_v<T, c::SubInst> ||
                                  std::is_same_v<T, c::MulInst> || std::is_same_v<T, c::DivInst> ||
                                  std::is_same_v<T, c::ModInst> || std::is_same_v<T, c::NegInst> ||
                                  std::is_same_v<T, c::LNotInst> || std::is_same_v<T, c::EqInst> ||
                                  std::is_same_v<T, c::NeInst> || std::is_same_v<T, c::LtInst> ||
                                  std::is_same_v<T, c::LeInst> || std::is_same_v<T, c::GtInst> ||
                                  std::is_same_v<T, c::GeInst>) {
                        prev.dst = rt.paramVreg;
                    }
                },
                insts[rt.index]);
        }

        // Replace the return terminator with a jump to the entry block.
        // VRegAnalysis & DCE can track liveness across this successor edge.
        // The backend redirects "entry" jumps past the prologue (to the
        // tail_entry label) automatically.
        block.terminator = c::JumpInst{"entry"};
        changed = true;
    }

    return changed;
}

} // namespace

void IrOptimizer::optimize(c::IRFunction& function) {
    // Tail-recursion elimination first: the resulting CopyInsts and
    // unreachable blocks are cleaned up by subsequent passes.
    if (eliminateTailRecursion(function)) {
        // Run one round of simplification + DCE on the transformed function
        // so the copy-propagated param assignments and dead call args are
        // cleaned out before the main pass.
        for (c::BasicBlock& block : function.basicBlocks) {
            simplifyBlock(block);
        }
        deadCodeElim(function);
    }

    constexpr int kMaxIterations = 8;
    for (int iter = 0; iter < kMaxIterations; ++iter) {
        bool changed = false;
        for (c::BasicBlock& block : function.basicBlocks) {
            changed |= simplifyBlock(block);
        }
        changed |= deadCodeElim(function);
        if (!changed) {
            break;
        }
    }
    // After DCE, coalesce copies to eliminate redundant mv instructions in
    // assignment chains.
    coalesceCopies(function);
}

void IrOptimizer::optimize(c::IRModule& module) {
    for (c::IRFunction& function : module.functions) {
        optimize(function);
    }
}

} // namespace toyc::codegen
