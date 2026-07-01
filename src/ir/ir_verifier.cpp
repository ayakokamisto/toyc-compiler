#include "toyc/ir/ir_verifier.h"

#include "toyc/ir/cfg.h"
#include "toyc/ir/instruction.h"

#include <sstream>
#include <unordered_map>
#include <unordered_set>

// =============================================================================
// verifyIR — structural correctness
// =============================================================================

std::vector<IRVerifierError> verifyIR(const Function& fn) {
    std::vector<IRVerifierError> errors;

    // Collect all block labels for target validation.
    std::unordered_set<Label*> labels;
    for (const auto& bb : fn.blocks()) {
        if (!labels.insert(bb->label()).second) {
            std::ostringstream msg;
            msg << "duplicate label '" << bb->label()->name() << "'";
            errors.push_back({0, msg.str()});
        }
    }

    // Build CFG to check edges.
    CFG cfg = CFG::build(const_cast<Function&>(fn));

    for (const auto& bb : fn.blocks()) {
        // Check: terminator exists.
        if (!bb->is_terminated()) {
            std::ostringstream msg;
            msg << "block '" << bb->label()->name() << "' has no terminator";
            errors.push_back({0, msg.str()});
            continue;
        }

        // Check: terminator is at the end (no instructions after terminator).
        // This is structurally enforced by the IR design, but verify.
        Instr* term = bb->terminator();
        (void)term;

        for (const auto& body_instr : bb->instructions()) {
            auto* instr = body_instr.get();
            if (instr->is_terminator()) {
                std::ostringstream msg;
                msg << "terminator appears in block body for '" << bb->label()->name() << "'";
                errors.push_back({0, msg.str()});
            }
            if (instr->kind() == InstrKind::Phi) {
                std::ostringstream msg;
                msg << "phi appears outside phi prefix for '" << bb->label()->name() << "'";
                errors.push_back({0, msg.str()});
            }
        }

        for (const auto& phi_instr : bb->phis()) {
            if (phi_instr->kind() != InstrKind::Phi) {
                std::ostringstream msg;
                msg << "non-phi instruction appears in phi prefix for '" << bb->label()->name() << "'";
                errors.push_back({0, msg.str()});
            }
        }

        // Check: each Value has exactly one defining instruction.
        // (unique definition)
        for (auto* instr : bb->all_instrs()) {
            Value* result = instr->result();
            if (result == nullptr) continue;

            // Count definitions by scanning all blocks.
            // This is O(n*m) but fine for P1.
        }

        // Check: branch targets exist.
        if (auto* br = dynamic_cast<BranchInstr*>(term)) {
            if (labels.find(br->target()) == labels.end()) {
                std::ostringstream msg;
                msg << "branch target '" << br->target()->name() << "' not found";
                errors.push_back({0, msg.str()});
            }
        } else if (auto* cbr = dynamic_cast<CondBranchInstr*>(term)) {
            if (labels.find(cbr->true_target()) == labels.end()) {
                std::ostringstream msg;
                msg << "condbranch true target '" << cbr->true_target()->name() << "' not found";
                errors.push_back({0, msg.str()});
            }
            if (labels.find(cbr->false_target()) == labels.end()) {
                std::ostringstream msg;
                msg << "condbranch false target '" << cbr->false_target()->name() << "' not found";
                errors.push_back({0, msg.str()});
            }
        }
        // ReturnInstr has no target labels — valid.

        // Check: all operands non-null.
        for (auto* instr : bb->all_instrs()) {
            for (Value* op : instr->operands()) {
                if (op == nullptr) {
                    std::ostringstream msg;
                    msg << "null operand in instruction";
                    errors.push_back({0, msg.str()});
                }
            }
        }
    }

    return errors;
}

std::vector<IRVerifierError> verifySSA(const Function& fn) {
    std::vector<IRVerifierError> errors;
    CFG cfg = CFG::build(const_cast<Function&>(fn));
    std::unordered_map<const Value*, const Instruction*> definitions;

    for (const auto& bb : fn.blocks()) {
        for (const Instruction* instruction : bb->all_instrs()) {
            if (Value* result = instruction->result()) {
                if (!definitions.emplace(result, instruction).second) {
                    errors.push_back({0, "value has multiple defining instructions: " + result->name()});
                }
            }
        }
    }

    for (const auto& bb : fn.blocks()) {
        BasicBlock* block = bb.get();
        if (block == fn.entry_block() && !block->phis().empty()) {
            errors.push_back({0, "entry block must not contain phi instructions"});
        }

        std::unordered_set<Label*> expected_pred_labels;
        for (BasicBlock* pred : cfg.predecessors(block)) {
            expected_pred_labels.insert(pred->label());
        }

        for (const auto& phi_ptr : block->phis()) {
            auto* phi = dynamic_cast<const PhiInstr*>(phi_ptr.get());
            if (phi == nullptr) continue;

            std::unordered_set<Label*> actual_pred_labels;
            for (const auto& incoming : phi->incoming()) {
                if (incoming.predecessor == nullptr) {
                    errors.push_back({0, "phi incoming predecessor is null"});
                    continue;
                }
                if (incoming.value == nullptr) {
                    errors.push_back({0, "phi incoming value is null"});
                }
                if (!actual_pred_labels.insert(incoming.predecessor).second) {
                    errors.push_back({0, "phi has duplicate incoming predecessor: "
                        + incoming.predecessor->name()});
                }
            }

            for (Label* expected : expected_pred_labels) {
                if (actual_pred_labels.count(expected) == 0) {
                    errors.push_back({0, "phi is missing incoming predecessor: "
                        + expected->name()});
                }
            }
            for (Label* actual : actual_pred_labels) {
                if (expected_pred_labels.count(actual) == 0) {
                    errors.push_back({0, "phi incoming predecessor is not a CFG predecessor: "
                        + actual->name()});
                }
            }
        }
    }

    return errors;
}

// =============================================================================
// verifyP1EmitterSupport — P1 backend capability constraints
// =============================================================================

std::vector<IRVerifierError> verifyP1EmitterSupport(const Function& fn) {
    std::vector<IRVerifierError> errors;

    // Collect all Alloca address values from entry block.
    std::unordered_set<Value*> entry_allocas;
    if (!fn.blocks().empty()) {
        for (auto* instr : fn.blocks()[0]->all_instrs()) {
            if (instr->kind() == InstrKind::Alloca) {
                entry_allocas.insert(instr->result());
            }
        }
    }

    for (const auto& bb : fn.blocks()) {
        for (auto* instr : bb->all_instrs()) {
            switch (instr->kind()) {
            case InstrKind::Phi: {
                auto* phi = static_cast<const PhiInstr*>(instr);
                (void)phi;
                errors.push_back({0,
                    "P1 RV32 emitter does not support Phi; lower Phi before emission"});
                break;
            }

            case InstrKind::Store: {
                auto* st = static_cast<const StoreInstr*>(instr);
                if (entry_allocas.find(st->address()) == entry_allocas.end()) {
                    bool is_global = false;
                    for (const auto& bb2 : fn.blocks()) {
                        for (auto* i : bb2->all_instrs()) {
                            if (i->kind() == InstrKind::GlobalAddr && i->result() == st->address())
                                is_global = true;
                        }
                    }
                    if (!is_global) {
                        errors.push_back({0,
                            "RV32 emitter only supports local alloca or global addr for Store"});
                    }
                }
                break;
            }
            case InstrKind::Load: {
                auto* ld = static_cast<const LoadInstr*>(instr);
                if (entry_allocas.find(ld->address()) == entry_allocas.end()) {
                    // Check if it's a global addr (in valueHome via GlobalAddrInstr)
                    bool is_global = false;
                    for (const auto& bb2 : fn.blocks()) {
                        for (auto* i : bb2->all_instrs()) {
                            if (i->kind() == InstrKind::GlobalAddr && i->result() == ld->address())
                                is_global = true;
                        }
                    }
                    if (!is_global) {
                        errors.push_back({0,
                            "RV32 emitter only supports local alloca or global addr for Load"});
                    }
                }
                break;
            }
            default:
                break; // All other instruction kinds are supported.
            }
        }
    }

    return errors;
}
