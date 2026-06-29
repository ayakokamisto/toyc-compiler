#include "ir/ir_passes.h"

#include "ir/ir_cfg.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace toyc::ir {
namespace {

namespace contract = toyc::codegen::contract;

// ─── helpers ────────────────────────────────────────────────────────────────

// Resolve a vreg through a replacement map, chasing the full chain.
std::string resolve(const std::unordered_map<std::string, std::string>& repl,
                    std::string vreg) {
    // At most a few hops; guard against pathological chains.
    for (int depth = 0; depth < 64; ++depth) {
        auto it = repl.find(vreg);
        if (it == repl.end()) {
            return vreg;
        }
        vreg = it->second;
    }
    return vreg;
}

// Rewrite a single vreg in-place through the replacement map.
void rewriteVReg(std::unordered_map<std::string, std::string>& repl,
                 std::string& vreg) {
    vreg = resolve(repl, vreg);
}

// ─── Copy Propagation ──────────────────────────────────────────────────────

// Rewrite all source operands of an instruction through the replacement map.
void rewriteInstructionSources(
    std::unordered_map<std::string, std::string>& repl,
    contract::Instruction& inst) {
    std::visit(
        [&](auto& i) {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, contract::CopyInst>) {
                rewriteVReg(repl, i.src);
            } else if constexpr (std::is_same_v<T, contract::StoreGlobalInst>) {
                rewriteVReg(repl, i.src);
            } else if constexpr (std::is_same_v<T, contract::CallInst>) {
                for (auto& arg : i.args) {
                    rewriteVReg(repl, arg);
                }
            } else if constexpr (std::is_same_v<T, contract::CallVoidInst>) {
                for (auto& arg : i.args) {
                    rewriteVReg(repl, arg);
                }
            } else if constexpr (std::is_same_v<T, contract::AddInst> ||
                                 std::is_same_v<T, contract::SubInst> ||
                                 std::is_same_v<T, contract::MulInst> ||
                                 std::is_same_v<T, contract::DivInst> ||
                                 std::is_same_v<T, contract::ModInst> ||
                                 std::is_same_v<T, contract::EqInst> ||
                                 std::is_same_v<T, contract::NeInst> ||
                                 std::is_same_v<T, contract::LtInst> ||
                                 std::is_same_v<T, contract::LeInst> ||
                                 std::is_same_v<T, contract::GtInst> ||
                                 std::is_same_v<T, contract::GeInst>) {
                rewriteVReg(repl, i.src1);
                rewriteVReg(repl, i.src2);
            } else if constexpr (std::is_same_v<T, contract::NegInst> ||
                                 std::is_same_v<T, contract::LNotInst>) {
                rewriteVReg(repl, i.src);
            }
            // ConstInst, LoadGlobalInst — no vreg sources to rewrite.
        },
        inst);
}

// Rewrite terminator source operands through the replacement map.
void rewriteTerminatorSources(
    std::unordered_map<std::string, std::string>& repl,
    contract::Terminator& term) {
    std::visit(
        [&](auto& t) {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, contract::BranchInst>) {
                rewriteVReg(repl, t.cond);
            } else if constexpr (std::is_same_v<T, contract::ReturnInst>) {
                if (t.src.has_value()) {
                    rewriteVReg(repl, *t.src);
                }
            }
            // JumpInst — no vreg sources.
        },
        term);
}

// Get the dst vreg of an instruction, if it has one.
// Returns empty string for instructions without a dst.
std::string_view instructionDst(const contract::Instruction& inst) {
    return std::visit(
        [](const auto& i) -> std::string_view {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, contract::ConstInst> ||
                          std::is_same_v<T, contract::CopyInst> ||
                          std::is_same_v<T, contract::LoadGlobalInst> ||
                          std::is_same_v<T, contract::CallInst> ||
                          std::is_same_v<T, contract::AddInst> ||
                          std::is_same_v<T, contract::SubInst> ||
                          std::is_same_v<T, contract::MulInst> ||
                          std::is_same_v<T, contract::DivInst> ||
                          std::is_same_v<T, contract::ModInst> ||
                          std::is_same_v<T, contract::NegInst> ||
                          std::is_same_v<T, contract::EqInst> ||
                          std::is_same_v<T, contract::NeInst> ||
                          std::is_same_v<T, contract::LtInst> ||
                          std::is_same_v<T, contract::LeInst> ||
                          std::is_same_v<T, contract::GtInst> ||
                          std::is_same_v<T, contract::GeInst> ||
                          std::is_same_v<T, contract::LNotInst>) {
                return i.dst;
            }
            return "";
        },
        inst);
}

// ─── Use-count helpers for DCE ─────────────────────────────────────────────

void countInstructionUses(const contract::Instruction& inst,
                          std::unordered_map<std::string, int>& counts) {
    std::visit(
        [&](const auto& i) {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, contract::CopyInst>) {
                ++counts[i.src];
            } else if constexpr (std::is_same_v<T, contract::StoreGlobalInst>) {
                ++counts[i.src];
            } else if constexpr (std::is_same_v<T, contract::CallInst>) {
                for (const auto& arg : i.args) {
                    ++counts[arg];
                }
            } else if constexpr (std::is_same_v<T, contract::CallVoidInst>) {
                for (const auto& arg : i.args) {
                    ++counts[arg];
                }
            } else if constexpr (std::is_same_v<T, contract::AddInst> ||
                                 std::is_same_v<T, contract::SubInst> ||
                                 std::is_same_v<T, contract::MulInst> ||
                                 std::is_same_v<T, contract::DivInst> ||
                                 std::is_same_v<T, contract::ModInst> ||
                                 std::is_same_v<T, contract::EqInst> ||
                                 std::is_same_v<T, contract::NeInst> ||
                                 std::is_same_v<T, contract::LtInst> ||
                                 std::is_same_v<T, contract::LeInst> ||
                                 std::is_same_v<T, contract::GtInst> ||
                                 std::is_same_v<T, contract::GeInst>) {
                ++counts[i.src1];
                ++counts[i.src2];
            } else if constexpr (std::is_same_v<T, contract::NegInst> ||
                                 std::is_same_v<T, contract::LNotInst>) {
                ++counts[i.src];
            }
            // ConstInst, LoadGlobalInst — no vreg sources.
        },
        inst);
}

void countTerminatorUses(const contract::Terminator& term,
                         std::unordered_map<std::string, int>& counts) {
    std::visit(
        [&](const auto& t) {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, contract::BranchInst>) {
                ++counts[t.cond];
            } else if constexpr (std::is_same_v<T, contract::ReturnInst>) {
                if (t.src.has_value()) {
                    ++counts[*t.src];
                }
            }
        },
        term);
}

// Check if an instruction is a pure computation (safe to delete if unused).
bool isPureComputation(const contract::Instruction& inst) {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;
            return std::is_same_v<T, contract::ConstInst> ||
                   std::is_same_v<T, contract::CopyInst> ||
                   std::is_same_v<T, contract::AddInst> ||
                   std::is_same_v<T, contract::SubInst> ||
                   std::is_same_v<T, contract::MulInst> ||
                   std::is_same_v<T, contract::DivInst> ||
                   std::is_same_v<T, contract::ModInst> ||
                   std::is_same_v<T, contract::NegInst> ||
                   std::is_same_v<T, contract::EqInst> ||
                   std::is_same_v<T, contract::NeInst> ||
                   std::is_same_v<T, contract::LtInst> ||
                   std::is_same_v<T, contract::LeInst> ||
                   std::is_same_v<T, contract::GtInst> ||
                   std::is_same_v<T, contract::GeInst> ||
                   std::is_same_v<T, contract::LNotInst>;
            // NOT pure: LoadGlobalInst, StoreGlobalInst, CallInst, CallVoidInst
        },
        inst);
}

// ─── CSE expression key ────────────────────────────────────────────────────

// Build an expression key for CSE.
// Returns empty string if the instruction is not eligible for CSE.
std::string expressionKey(const contract::Instruction& inst) {
    return std::visit(
        [](const auto& i) -> std::string {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, contract::ConstInst>) {
                return "const:" + std::to_string(i.value);
            } else if constexpr (std::is_same_v<T, contract::NegInst>) {
                return "neg:" + std::string(i.src);
            } else if constexpr (std::is_same_v<T, contract::LNotInst>) {
                return "lnot:" + std::string(i.src);
            } else if constexpr (std::is_same_v<T, contract::AddInst>) {
                if (i.src1 <= i.src2) {
                    return "add:" + i.src1 + ":" + i.src2;
                }
                return "add:" + i.src2 + ":" + i.src1;
            } else if constexpr (std::is_same_v<T, contract::MulInst>) {
                if (i.src1 <= i.src2) {
                    return "mul:" + i.src1 + ":" + i.src2;
                }
                return "mul:" + i.src2 + ":" + i.src1;
            } else if constexpr (std::is_same_v<T, contract::EqInst>) {
                if (i.src1 <= i.src2) {
                    return "eq:" + i.src1 + ":" + i.src2;
                }
                return "eq:" + i.src2 + ":" + i.src1;
            } else if constexpr (std::is_same_v<T, contract::NeInst>) {
                if (i.src1 <= i.src2) {
                    return "ne:" + i.src1 + ":" + i.src2;
                }
                return "ne:" + i.src2 + ":" + i.src1;
            } else if constexpr (std::is_same_v<T, contract::SubInst>) {
                return "sub:" + i.src1 + ":" + i.src2;
            } else if constexpr (std::is_same_v<T, contract::DivInst>) {
                return "div:" + i.src1 + ":" + i.src2;
            } else if constexpr (std::is_same_v<T, contract::ModInst>) {
                return "mod:" + i.src1 + ":" + i.src2;
            } else if constexpr (std::is_same_v<T, contract::LtInst>) {
                return "lt:" + i.src1 + ":" + i.src2;
            } else if constexpr (std::is_same_v<T, contract::LeInst>) {
                return "le:" + i.src1 + ":" + i.src2;
            } else if constexpr (std::is_same_v<T, contract::GtInst>) {
                return "gt:" + i.src1 + ":" + i.src2;
            } else if constexpr (std::is_same_v<T, contract::GeInst>) {
                return "ge:" + i.src1 + ":" + i.src2;
            }
            // Not eligible for CSE.
            return "";
        },
        inst);
}

// Check if an instruction is eligible for CSE (pure computation only).
bool isCSEEligible(const contract::Instruction& inst) {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;
            return std::is_same_v<T, contract::ConstInst> ||
                   std::is_same_v<T, contract::AddInst> ||
                   std::is_same_v<T, contract::SubInst> ||
                   std::is_same_v<T, contract::MulInst> ||
                   std::is_same_v<T, contract::DivInst> ||
                   std::is_same_v<T, contract::ModInst> ||
                   std::is_same_v<T, contract::NegInst> ||
                   std::is_same_v<T, contract::LNotInst> ||
                   std::is_same_v<T, contract::EqInst> ||
                   std::is_same_v<T, contract::NeInst> ||
                   std::is_same_v<T, contract::LtInst> ||
                   std::is_same_v<T, contract::LeInst> ||
                   std::is_same_v<T, contract::GtInst> ||
                   std::is_same_v<T, contract::GeInst>;
        },
        inst);
}

// Collect all vreg uses from a block (instructions + terminator).
std::unordered_set<std::string> collectBlockUses(const contract::BasicBlock& block) {
    std::unordered_set<std::string> uses;
    for (const auto& inst : block.instructions) {
        std::visit(
            [&](const auto& i) {
                using T = std::decay_t<decltype(i)>;
                if constexpr (std::is_same_v<T, contract::CopyInst>) {
                    uses.insert(i.src);
                } else if constexpr (std::is_same_v<T, contract::StoreGlobalInst>) {
                    uses.insert(i.src);
                } else if constexpr (std::is_same_v<T, contract::CallInst>) {
                    for (const auto& a : i.args) uses.insert(a);
                } else if constexpr (std::is_same_v<T, contract::CallVoidInst>) {
                    for (const auto& a : i.args) uses.insert(a);
                } else if constexpr (std::is_same_v<T, contract::AddInst> ||
                                     std::is_same_v<T, contract::SubInst> ||
                                     std::is_same_v<T, contract::MulInst> ||
                                     std::is_same_v<T, contract::DivInst> ||
                                     std::is_same_v<T, contract::ModInst> ||
                                     std::is_same_v<T, contract::EqInst> ||
                                     std::is_same_v<T, contract::NeInst> ||
                                     std::is_same_v<T, contract::LtInst> ||
                                     std::is_same_v<T, contract::LeInst> ||
                                     std::is_same_v<T, contract::GtInst> ||
                                     std::is_same_v<T, contract::GeInst>) {
                    uses.insert(i.src1);
                    uses.insert(i.src2);
                } else if constexpr (std::is_same_v<T, contract::NegInst> ||
                                     std::is_same_v<T, contract::LNotInst>) {
                    uses.insert(i.src);
                }
            },
            inst);
    }
    std::visit(
        [&](const auto& t) {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, contract::BranchInst>) {
                uses.insert(t.cond);
            } else if constexpr (std::is_same_v<T, contract::ReturnInst>) {
                if (t.src.has_value()) uses.insert(*t.src);
            }
        },
        block.terminator);
    return uses;
}

// For each block label, compute the set of vregs used in OTHER blocks.
// This lets copy propagation decide whether a CopyInst's dst can be safely erased.
std::unordered_map<std::string, std::unordered_set<std::string>>
computeCrossBlockUses(const contract::IRFunction& function) {
    std::vector<std::unordered_set<std::string>> perBlockUses;
    perBlockUses.reserve(function.basicBlocks.size());
    for (const auto& block : function.basicBlocks) {
        perBlockUses.push_back(collectBlockUses(block));
    }

    std::unordered_map<std::string, std::unordered_set<std::string>> result;
    for (std::size_t i = 0; i < function.basicBlocks.size(); ++i) {
        std::unordered_set<std::string> outside;
        for (std::size_t j = 0; j < function.basicBlocks.size(); ++j) {
            if (i == j) continue;
            for (const auto& vreg : perBlockUses[j]) {
                outside.insert(vreg);
            }
        }
        result[function.basicBlocks[i].label] = std::move(outside);
    }
    return result;
}

} // namespace

// ─── Pass: Copy Propagation ────────────────────────────────────────────────

bool runCopyPropagation(contract::IRFunction& function) {
    bool changed = false;

    // Precompute which vregs each block uses outside itself.
    const auto crossBlockUses = computeCrossBlockUses(function);

    for (contract::BasicBlock& block : function.basicBlocks) {
        // Per-block replacement map: dst → canonical source vreg.
        std::unordered_map<std::string, std::string> replacement;

        // Rewrite terminator sources (will be rewritten again after last inst).
        rewriteTerminatorSources(replacement, block.terminator);

        auto it = block.instructions.begin();
        while (it != block.instructions.end()) {
            // Rewrite source operands through current replacement map.
            rewriteInstructionSources(replacement, *it);

            // Check if this is a copy instruction.
            if (auto* copy = std::get_if<contract::CopyInst>(&*it)) {
                // Only erase the CopyInst if dst is not used outside this block.
                // Cross-block uses cannot be rewritten by per-block propagation.
                const auto outsideIt = crossBlockUses.find(block.label);
                const bool usedOutside =
                    outsideIt != crossBlockUses.end() &&
                    outsideIt->second.count(copy->dst) > 0;

                if (!usedOutside) {
                    // dst = copy src  →  replacement[dst] = resolve(src)
                    replacement[copy->dst] = resolve(replacement, copy->src);
                    it = block.instructions.erase(it);
                    changed = true;
                } else {
                    // Keep the copy — do NOT add to replacement map.
                    // Adding it would cause uses before the copy to be rewritten
                    // to the copy's source, creating forward references and
                    // circular dependencies (e.g., mul %x, 3 → mul %t7, 3
                    // where %t7 = add %t5, %t6 is defined AFTER the mul).
                    ++it;
                }
                continue;
            }

            // For any other instruction with a dst, invalidate stale mappings.
            std::string_view dst = instructionDst(*it);
            if (!dst.empty()) {
                replacement.erase(std::string(dst));
            }

            ++it;
        }

        // Rewrite terminator sources one final time after all instructions.
        rewriteTerminatorSources(replacement, block.terminator);
    }

    return changed;
}

// ─── Pass: Local CSE ──────────────────────────────────────────────────────

bool runLocalCSE(contract::IRFunction& function) {
    bool changed = false;

    // Precompute which vregs each block uses outside itself.
    // This prevents CSE from erasing an instruction whose dst is used
    // in other blocks (which would leave undefined vreg references).
    const auto crossBlockUses = computeCrossBlockUses(function);

    for (contract::BasicBlock& block : function.basicBlocks) {
        // Per-block expression map: expression key → producing vreg.
        std::unordered_map<std::string, std::string> exprMap;
        // Replacement map for CSE (dst of eliminated → existing vreg).
        std::unordered_map<std::string, std::string> replacement;

        const auto outsideIt = crossBlockUses.find(block.label);
        const auto& outsideUses =
            outsideIt != crossBlockUses.end() ? outsideIt->second
                                              : *static_cast<const std::unordered_set<std::string>*>(nullptr);

        for (auto it = block.instructions.begin(); it != block.instructions.end();) {
            // First resolve all source operands through existing CSE replacements.
            rewriteInstructionSources(replacement, *it);

            if (!isCSEEligible(*it)) {
                std::string_view dst = instructionDst(*it);
                if (!dst.empty()) {
                    for (auto em = exprMap.begin(); em != exprMap.end();) {
                        if (em->second == dst) {
                            em = exprMap.erase(em);
                        } else {
                            ++em;
                        }
                    }
                }
                ++it;
                continue;
            }

            // Build expression key from (now-resolved) operands.
            std::string key = expressionKey(*it);
            if (key.empty()) {
                ++it;
                continue;
            }

            auto existing = exprMap.find(key);
            if (existing != exprMap.end()) {
                std::string_view dst = instructionDst(*it);
                if (!dst.empty()) {
                    // Only erase if dst is NOT used in other blocks.
                    const bool usedOutside = outsideIt != crossBlockUses.end() &&
                                             outsideUses.count(std::string(dst)) > 0;
                    if (!usedOutside) {
                        replacement[std::string(dst)] = existing->second;
                        it = block.instructions.erase(it);
                        changed = true;
                        continue;
                    }
                    // If used outside, skip CSE for this instruction.
                }
                ++it;
            } else {
                // CSE miss: record this expression.
                std::string_view dst = instructionDst(*it);
                if (!dst.empty()) {
                    exprMap[key] = std::string(dst);
                }
                ++it;
            }
        }

        // Rewrite terminator sources through CSE replacements.
        rewriteTerminatorSources(replacement, block.terminator);
    }

    return changed;
}

// ─── Pass: DCE ─────────────────────────────────────────────────────────────

bool runDCE(contract::IRFunction& function) {
    bool changed = false;
    bool iterationChanged = true;

    while (iterationChanged) {
        iterationChanged = false;

        // Compute global use counts across all blocks.
        std::unordered_map<std::string, int> useCounts;
        for (const contract::Param& param : function.params) {
            ++useCounts[param.vreg];
        }
        for (const contract::BasicBlock& block : function.basicBlocks) {
            for (const contract::Instruction& inst : block.instructions) {
                countInstructionUses(inst, useCounts);
            }
            countTerminatorUses(block.terminator, useCounts);
        }

        // Forward sweep per block. Delete dead pure-computation instructions.
        // We use a forward sweep so that erasing an instruction does not affect
        // the use counts of instructions we have not yet visited. Source use
        // counts are NOT decremented here; they are recomputed from scratch at
        // the start of the next iteration.
        for (contract::BasicBlock& block : function.basicBlocks) {
            auto it = block.instructions.begin();
            while (it != block.instructions.end()) {
                if (!isPureComputation(*it)) {
                    ++it;
                    continue;
                }

                std::string_view dst = instructionDst(*it);
                if (dst.empty()) {
                    ++it;
                    continue;
                }

                auto countIt = useCounts.find(std::string(dst));
                if (countIt != useCounts.end() && countIt->second > 0) {
                    ++it;
                    continue;
                }

                it = block.instructions.erase(it);
                changed = true;
                iterationChanged = true;
            }
        }
    }

    return changed;
}

// ─── Pass: LICM ────────────────────────────────────────────────────────────

namespace {

// Get source vregs used by an instruction.
std::vector<std::string> instructionSources(const contract::Instruction& inst) {
    std::vector<std::string> sources;
    std::visit(
        [&](const auto& i) {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, contract::CopyInst>) {
                sources.push_back(i.src);
            } else if constexpr (std::is_same_v<T, contract::StoreGlobalInst>) {
                sources.push_back(i.src);
            } else if constexpr (std::is_same_v<T, contract::CallInst>) {
                for (const auto& a : i.args) sources.push_back(a);
            } else if constexpr (std::is_same_v<T, contract::CallVoidInst>) {
                for (const auto& a : i.args) sources.push_back(a);
            } else if constexpr (std::is_same_v<T, contract::AddInst> ||
                                 std::is_same_v<T, contract::SubInst> ||
                                 std::is_same_v<T, contract::MulInst> ||
                                 std::is_same_v<T, contract::DivInst> ||
                                 std::is_same_v<T, contract::ModInst> ||
                                 std::is_same_v<T, contract::EqInst> ||
                                 std::is_same_v<T, contract::NeInst> ||
                                 std::is_same_v<T, contract::LtInst> ||
                                 std::is_same_v<T, contract::LeInst> ||
                                 std::is_same_v<T, contract::GtInst> ||
                                 std::is_same_v<T, contract::GeInst>) {
                sources.push_back(i.src1);
                sources.push_back(i.src2);
            } else if constexpr (std::is_same_v<T, contract::NegInst> ||
                                 std::is_same_v<T, contract::LNotInst>) {
                sources.push_back(i.src);
            }
        },
        inst);
    return sources;
}

} // namespace

// Tail recursion elimination.
namespace {

bool isSelfTailCallBlock(const contract::IRFunction& function,
                         const contract::BasicBlock& block) {
    const auto* ret = std::get_if<contract::ReturnInst>(&block.terminator);
    if (ret == nullptr || !ret->src.has_value() || block.instructions.empty()) {
        return false;
    }

    const auto* call = std::get_if<contract::CallInst>(&block.instructions.back());
    return call != nullptr && call->functionName == function.name &&
           call->dst == *ret->src && call->args.size() == function.params.size();
}

std::unordered_set<std::string> collectFunctionVRegs(
    const contract::IRFunction& function) {
    std::unordered_set<std::string> names;
    for (const auto& param : function.params) {
        names.insert(param.vreg);
    }
    for (const auto& block : function.basicBlocks) {
        for (const auto& inst : block.instructions) {
            const std::string_view dst = instructionDst(inst);
            if (!dst.empty()) {
                names.insert(std::string(dst));
            }
            for (const auto& src : instructionSources(inst)) {
                names.insert(src);
            }
        }
        std::visit(
            [&](const auto& term) {
                using T = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<T, contract::BranchInst>) {
                    names.insert(term.cond);
                } else if constexpr (std::is_same_v<T, contract::ReturnInst>) {
                    if (term.src.has_value()) {
                        names.insert(*term.src);
                    }
                }
            },
            block.terminator);
    }
    return names;
}

std::unordered_set<std::string> collectBlockLabels(
    const contract::IRFunction& function) {
    std::unordered_set<std::string> labels;
    for (const auto& block : function.basicBlocks) {
        labels.insert(block.label);
    }
    return labels;
}

std::string makeUniqueVReg(std::unordered_set<std::string>& used,
                           std::string_view stem) {
    for (std::size_t index = 0;; ++index) {
        std::string candidate = "%" + std::string(stem) + std::to_string(index);
        if (used.insert(candidate).second) {
            return candidate;
        }
    }
}

std::string makeUniqueLabel(std::unordered_set<std::string>& used,
                            std::string_view stem) {
    for (std::size_t index = 0;; ++index) {
        std::string candidate = std::string(stem) + "_" + std::to_string(index);
        if (used.insert(candidate).second) {
            return candidate;
        }
    }
}

void rewriteEntryTargets(contract::IRFunction& function,
                         std::string_view newEntryLabel) {
    for (auto& block : function.basicBlocks) {
        std::visit(
            [&](auto& term) {
                using T = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<T, contract::JumpInst>) {
                    if (term.targetLabel == "entry") {
                        term.targetLabel = std::string(newEntryLabel);
                    }
                } else if constexpr (std::is_same_v<T, contract::BranchInst>) {
                    if (term.trueLabel == "entry") {
                        term.trueLabel = std::string(newEntryLabel);
                    }
                    if (term.falseLabel == "entry") {
                        term.falseLabel = std::string(newEntryLabel);
                    }
                }
            },
            block.terminator);
    }
}

std::string ensureTailLoopEntry(contract::IRFunction& function) {
    auto labels = collectBlockLabels(function);
    const std::string loopEntryLabel = makeUniqueLabel(labels, "tail_entry");

    contract::BasicBlock loopEntry = std::move(function.basicBlocks.front());
    loopEntry.label = loopEntryLabel;

    function.basicBlocks.front() =
        contract::BasicBlock{"entry", {}, contract::JumpInst{loopEntryLabel}};
    function.basicBlocks.insert(function.basicBlocks.begin() + 1,
                                std::move(loopEntry));

    rewriteEntryTargets(function, loopEntryLabel);
    function.basicBlocks.front().terminator = contract::JumpInst{loopEntryLabel};
    return loopEntryLabel;
}

} // namespace

bool runTailRecursionElimination(contract::IRFunction& function) {
    if (function.basicBlocks.empty() || function.params.empty()) {
        return false;
    }

    bool hasTailCall = false;
    for (const auto& block : function.basicBlocks) {
        if (isSelfTailCallBlock(function, block)) {
            hasTailCall = true;
            break;
        }
    }
    if (!hasTailCall) {
        return false;
    }

    const std::string loopEntryLabel = ensureTailLoopEntry(function);
    std::unordered_set<std::string> usedVRegs = collectFunctionVRegs(function);

    bool changed = false;
    for (auto& block : function.basicBlocks) {
        if (!isSelfTailCallBlock(function, block)) {
            continue;
        }

        auto call = std::get<contract::CallInst>(block.instructions.back());
        block.instructions.pop_back();

        std::vector<std::string> argTemps;
        argTemps.reserve(call.args.size());
        for (const auto& arg : call.args) {
            const std::string temp = makeUniqueVReg(usedVRegs, "tail_arg_");
            block.instructions.push_back(contract::CopyInst{temp, arg});
            argTemps.push_back(temp);
        }

        for (std::size_t i = 0; i < argTemps.size(); ++i) {
            block.instructions.push_back(
                contract::CopyInst{function.params[i].vreg, argTemps[i]});
        }

        block.terminator = contract::JumpInst{loopEntryLabel};
        changed = true;
    }

    return changed;
}

bool runLICM(contract::IRFunction& function) {
    bool changed = false;

    // Single pass: build CFG once, find all loops, process each.
    CFG cfg = buildCFG(function);
    const DominatorSets dom = computeDominators(function, cfg);
    auto loops = findNaturalLoops(function, cfg, dom);

    // Track which headers we've already processed to avoid duplicates.
    std::unordered_set<std::size_t> processedHeaders;

    for (const auto& loop : loops) {
        if (processedHeaders.count(loop.header) > 0) continue;
        processedHeaders.insert(loop.header);

        // Find outside predecessors of the header.
        const std::size_t header = loop.header;
        std::vector<std::size_t> outsidePreds;
        for (const std::size_t pred : cfg.predecessors[header]) {
            if (loop.blocks.count(pred) == 0) {
                outsidePreds.push_back(pred);
            }
        }

        // Only handle the simple case: single outside predecessor.
        // This covers the standard while-loop pattern (entry → while_cond).
        if (outsidePreds.size() != 1) continue;

        const std::size_t preheaderIdx = outsidePreds[0];

        // Collect vregs defined inside the loop (excluding preheader).
        std::unordered_set<std::string> loopDefs;
        for (const std::size_t blockIdx : loop.blocks) {
            if (blockIdx == preheaderIdx) continue;
            for (const auto& inst : function.basicBlocks[blockIdx].instructions) {
                std::string_view dst = instructionDst(inst);
                if (!dst.empty()) {
                    loopDefs.insert(std::string(dst));
                }
            }
        }

        // Collect vregs that are "modified by the loop" — their value changes
        // each iteration. This includes:
        // 1. All vregs defined in loop blocks (already in loopDefs).
        // 2. Vregs that appear as both dst AND src of the same instruction
        //    (e.g., %t6 = add %t6, %t5 after copy propagation rewrites
        //    x = x + 1 → %t6 = add %x, %t5 → %t6 = add %t6, %t5).
        std::unordered_set<std::string> loopModified = loopDefs;
        for (const std::size_t blockIdx : loop.blocks) {
            if (blockIdx == preheaderIdx) continue;
            for (const auto& inst : function.basicBlocks[blockIdx].instructions) {
                std::visit([&](const auto& i) {
                    using T = std::decay_t<decltype(i)>;
                    if constexpr (std::is_same_v<T, contract::AddInst> ||
                                  std::is_same_v<T, contract::SubInst> ||
                                  std::is_same_v<T, contract::MulInst> ||
                                  std::is_same_v<T, contract::DivInst> ||
                                  std::is_same_v<T, contract::ModInst>) {
                        // If dst appears as a source, the vreg is modified each iteration.
                        if (i.dst == i.src1 || i.dst == i.src2) {
                            loopModified.insert(i.dst);
                        }
                    } else if constexpr (std::is_same_v<T, contract::NegInst> ||
                                         std::is_same_v<T, contract::LNotInst>) {
                        if (i.dst == i.src) {
                            loopModified.insert(i.dst);
                        }
                    } else if constexpr (std::is_same_v<T, contract::CopyInst>) {
                        if (i.dst == i.src) {
                            loopModified.insert(i.dst);
                        }
                    }
                }, inst);
            }
        }

        // Find loop-invariant instructions (iterative fixed point).
        // CopyInst is excluded because it represents variable re-assignment
        // (e.g., x = x + 1 generates add + copy). Hoisting the copy would
        // change the loop's semantics.
        // ConstInst is NOT independently marked as invariant. Instead, when
        // a non-const invariant instruction has ConstInst sources, those
        // constants are hoisted along with it. This preserves the backend's
        // ability to use immediate-encoded instructions (e.g., `addi s2, s6, 7`)
        // for constants that are only used by non-hoisted instructions.
        std::unordered_set<std::string> invariantVregs;
        bool foundNew = true;
        while (foundNew) {
            foundNew = false;
            for (const std::size_t blockIdx : loop.blocks) {
                if (blockIdx == preheaderIdx) continue;
                const auto& block = function.basicBlocks[blockIdx];
                for (const auto& inst : block.instructions) {
                    if (!isPureComputation(inst)) continue;
                    if (std::holds_alternative<contract::CopyInst>(inst)) continue;
                    if (std::holds_alternative<contract::ConstInst>(inst)) continue;

                    std::string_view dst = instructionDst(inst);
                    if (dst.empty()) continue;

                    const std::string dstStr(dst);
                    if (invariantVregs.count(dstStr) > 0) continue;

                    // All sources must be loop-invariant.
                    bool allInvariant = true;
                    for (const auto& src : instructionSources(inst)) {
                        // Source is modified by the loop (re-defined each iteration) → not invariant.
                        if (loopModified.count(src) > 0) {
                            allInvariant = false;
                            break;
                        }
                        // Source is defined outside the loop → invariant.
                        if (loopDefs.count(src) == 0) continue;
                        // Source is already marked invariant → invariant.
                        if (invariantVregs.count(src) > 0) continue;
                        // Otherwise → not invariant.
                        allInvariant = false;
                        break;
                    }
                    if (!allInvariant) continue;

                    // Defining block must dominate all loop-internal uses.
                    std::size_t defBlockIdx = 0;
                    bool foundDef = false;
                    for (std::size_t bi = 0; bi < function.basicBlocks.size(); ++bi) {
                        for (const auto& i : function.basicBlocks[bi].instructions) {
                            if (std::addressof(i) == std::addressof(inst)) {
                                defBlockIdx = bi;
                                foundDef = true;
                                break;
                            }
                        }
                        if (foundDef) break;
                    }
                    if (!foundDef) continue;

                    bool domOk = true;
                    for (const std::size_t bi2 : loop.blocks) {
                        if (bi2 == preheaderIdx) continue;
                        const auto& b2 = function.basicBlocks[bi2];
                        for (const auto& i2 : b2.instructions) {
                            for (const auto& s : instructionSources(i2)) {
                                if (s == dstStr && !dominates(dom, defBlockIdx, bi2)) {
                                    domOk = false;
                                    break;
                                }
                            }
                            if (!domOk) break;
                        }
                        if (!domOk) break;
                        std::visit([&](const auto& t) {
                            using T = std::decay_t<decltype(t)>;
                            if constexpr (std::is_same_v<T, contract::BranchInst>) {
                                if (t.cond == dstStr && !dominates(dom, defBlockIdx, bi2))
                                    domOk = false;
                            } else if constexpr (std::is_same_v<T, contract::ReturnInst>) {
                                if (t.src.has_value() && *t.src == dstStr && !dominates(dom, defBlockIdx, bi2))
                                    domOk = false;
                            }
                        }, b2.terminator);
                        if (!domOk) break;
                    }

                    if (domOk) {
                        invariantVregs.insert(dstStr);
                        foundNew = true;
                    }
                }
            }
        }

        if (invariantVregs.empty()) continue;

        // Hoist invariant instructions to the preheader.
        // Also hoist ConstInst sources of invariant instructions — they are
        // needed by the hoisted instructions and won't be used in the loop body
        // anymore (DCE will clean up any remaining dead constants).
        auto& preheader = function.basicBlocks[preheaderIdx];
        std::vector<contract::Instruction> toHoist;

        // First pass: collect ConstInst sources that need to be hoisted.
        // These are ConstInst in the loop body whose dst is used as a source
        // by an invariant instruction.
        std::unordered_set<std::string> constSrcsToHoist;
        for (const std::size_t blockIdx : loop.blocks) {
            if (blockIdx == preheaderIdx) continue;
            const auto& block = function.basicBlocks[blockIdx];
            for (const auto& inst : block.instructions) {
                std::string_view dst = instructionDst(inst);
                if (dst.empty() || invariantVregs.count(std::string(dst)) == 0) continue;
                // This instruction is invariant. Check its sources.
                for (const auto& src : instructionSources(inst)) {
                    if (loopDefs.count(src) > 0 && invariantVregs.count(src) == 0) {
                        // Source is defined in the loop but not marked invariant.
                        // Check if it's a ConstInst.
                        for (const auto& bi : loop.blocks) {
                            if (bi == preheaderIdx) continue;
                            for (const auto& si : function.basicBlocks[bi].instructions) {
                                if (std::holds_alternative<contract::ConstInst>(si)) {
                                    const auto& ci = std::get<contract::ConstInst>(si);
                                    if (ci.dst == src) {
                                        constSrcsToHoist.insert(src);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Second pass: collect instructions to hoist (invariant + their const sources).
        for (const std::size_t blockIdx : loop.blocks) {
            if (blockIdx == preheaderIdx) continue;
            auto& block = function.basicBlocks[blockIdx];
            auto it = block.instructions.begin();
            while (it != block.instructions.end()) {
                std::string_view dst = instructionDst(*it);
                if (!dst.empty() &&
                    (invariantVregs.count(std::string(dst)) > 0 ||
                     constSrcsToHoist.count(std::string(dst)) > 0)) {
                    toHoist.push_back(std::move(*it));
                    it = block.instructions.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
        }

        // Insert before the preheader's terminator.
        for (auto& inst : toHoist) {
            preheader.instructions.push_back(std::move(inst));
        }
    }

    return changed;
}

// ─── Pass: Constant Propagation + Folding ──────────────────────────────────

namespace {

using ConstMap = std::unordered_map<std::string, std::int32_t>;

std::optional<std::int32_t> lookupConst(const ConstMap& constants,
                                        const std::string& vreg) {
    const auto it = constants.find(vreg);
    if (it == constants.end()) return std::nullopt;
    return it->second;
}

std::int32_t wrapAdd(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) +
                                     static_cast<std::uint32_t>(b));
}
std::int32_t wrapSub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) -
                                     static_cast<std::uint32_t>(b));
}
std::int32_t wrapMul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) *
                                     static_cast<std::uint32_t>(b));
}
std::optional<std::int32_t> foldDiv(std::int32_t a, std::int32_t b) {
    if (b == 0) return std::nullopt;
    if (a == std::numeric_limits<std::int32_t>::min() && b == -1) return std::nullopt;
    return a / b;
}
std::optional<std::int32_t> foldMod(std::int32_t a, std::int32_t b) {
    if (b == 0) return std::nullopt;
    if (a == std::numeric_limits<std::int32_t>::min() && b == -1) return std::nullopt;
    return a % b;
}

// Try to constant-fold a binary instruction. Returns the folded value or nullopt.
std::optional<std::int32_t> foldBinary(const contract::Instruction& inst,
                                       const ConstMap& constants) {
    return std::visit(
        [&](const auto& i) -> std::optional<std::int32_t> {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, contract::AddInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return wrapAdd(*a, *b);
            } else if constexpr (std::is_same_v<T, contract::SubInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return wrapSub(*a, *b);
            } else if constexpr (std::is_same_v<T, contract::MulInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return wrapMul(*a, *b);
            } else if constexpr (std::is_same_v<T, contract::DivInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return foldDiv(*a, *b);
            } else if constexpr (std::is_same_v<T, contract::ModInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return foldMod(*a, *b);
            } else if constexpr (std::is_same_v<T, contract::EqInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return *a == *b ? 1 : 0;
            } else if constexpr (std::is_same_v<T, contract::NeInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return *a != *b ? 1 : 0;
            } else if constexpr (std::is_same_v<T, contract::LtInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return *a < *b ? 1 : 0;
            } else if constexpr (std::is_same_v<T, contract::LeInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return *a <= *b ? 1 : 0;
            } else if constexpr (std::is_same_v<T, contract::GtInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return *a > *b ? 1 : 0;
            } else if constexpr (std::is_same_v<T, contract::GeInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && b) return *a >= *b ? 1 : 0;
            }
            return std::nullopt;
        },
        inst);
}

// Try to constant-fold a unary instruction.
std::optional<std::int32_t> foldUnary(const contract::Instruction& inst,
                                      const ConstMap& constants) {
    return std::visit(
        [&](const auto& i) -> std::optional<std::int32_t> {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, contract::NegInst>) {
                auto a = lookupConst(constants, i.src);
                if (a) return wrapSub(0, *a);
            } else if constexpr (std::is_same_v<T, contract::LNotInst>) {
                auto a = lookupConst(constants, i.src);
                if (a) return *a == 0 ? 1 : 0;
            }
            return std::nullopt;
        },
        inst);
}

// Try algebraic simplification. Returns replacement vreg or empty string.
// Handles: x+0→x, 0+x→x, x-0→x, x*1→x, 1*x→x, x*0→0, 0*x→0
[[maybe_unused]] std::string algebraicSimplify(const contract::Instruction& inst,
                                               const ConstMap& constants) {
    return std::visit(
        [&](const auto& i) -> std::string {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, contract::AddInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && *a == 0) return i.src2;  // 0 + x = x
                if (b && *b == 0) return i.src1;  // x + 0 = x
            } else if constexpr (std::is_same_v<T, contract::SubInst>) {
                auto b = lookupConst(constants, i.src2);
                if (b && *b == 0) return i.src1;  // x - 0 = x
            } else if constexpr (std::is_same_v<T, contract::MulInst>) {
                auto a = lookupConst(constants, i.src1);
                auto b = lookupConst(constants, i.src2);
                if (a && *a == 0) return i.src1;  // 0 * x = 0 (src1)
                if (b && *b == 0) return i.src2;  // x * 0 = 0 (src2)
                if (a && *a == 1) return i.src2;  // 1 * x = x
                if (b && *b == 1) return i.src1;  // x * 1 = x
            }
            return std::string{};
        },
        inst);
}

} // namespace

bool runConstProp(contract::IRFunction& function) {
    bool changed = false;

    try {
    for (auto& block : function.basicBlocks) {
        ConstMap constants;

        for (auto& inst : block.instructions) {
            // Try constant folding for binary/unary instructions.
            // This does NOT erase or replace instructions — it only converts
            // a computation instruction into a ConstInst when all operands
            // are known constants. This is safe across block boundaries
            // because the dst vreg still has a definition.
            if (isPureComputation(inst)) {
                std::optional<std::int32_t> folded = foldBinary(inst, constants);
                if (!folded) folded = foldUnary(inst, constants);
                std::string_view dst = instructionDst(inst);
                if (folded && !dst.empty()) {
                    inst = contract::ConstInst{std::string(dst), *folded};
                    constants[std::string(dst)] = *folded;
                    changed = true;
                    continue;
                }
            }

            // Track known constants.
            if (std::holds_alternative<contract::ConstInst>(inst)) {
                const auto& ci = std::get<contract::ConstInst>(inst);
                constants[ci.dst] = ci.value;
            } else {
                // Non-constant definition invalidates the dst.
                std::string_view dst = instructionDst(inst);
                if (!dst.empty()) {
                    constants.erase(std::string(dst));
                }
            }
        }
    }
    } catch (...) {
        return changed;
    }

    return changed;
}

} // namespace toyc::ir
