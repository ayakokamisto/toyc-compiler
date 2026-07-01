#pragma once

#include "instruction.h"

#include <cstdint>
#include <memory>
#include <vector>

// =============================================================================
// BasicBlock — a single basic block in the CFG.
//
// Contains phi instructions, a body of non-phi non-terminator instructions,
// and exactly one terminator.
//
// Owns all instructions via std::unique_ptr<Instr>.
// Does NOT own its Label (owned by Function's value arena).
// Does NOT store a parent Function pointer (matching Java semantics).
// =============================================================================

using InstrPtr = std::unique_ptr<Instr>;

class BasicBlock {
public:
    explicit BasicBlock(Label* label);

    // Non-copyable.
    BasicBlock(const BasicBlock&) = delete;
    BasicBlock& operator=(const BasicBlock&) = delete;
    BasicBlock(BasicBlock&&) = default;
    BasicBlock& operator=(BasicBlock&&) = default;

    Label* label() const { return label_; }

    // --- Phi access ---
    const std::vector<InstrPtr>& phis() const { return phis_; }
    void add_phi(InstrPtr phi);
    void clear_phis();

    // --- Body instructions (non-phi, non-terminator) ---
    const std::vector<InstrPtr>& instructions() const { return instrs_; }
    void add_instruction(InstrPtr instr);
    void insert_instruction(size_t index, InstrPtr instr);
    void replace_body(std::vector<InstrPtr> new_instrs);

    // --- Terminator ---
    Instr* terminator() const { return terminator_.get(); }
    void set_terminator(InstrPtr term);
    bool is_terminated() const { return terminator_ != nullptr; }

    // --- Combined iteration ---
    // Flat vector of all instructions in order: phis → body → terminator.
    // Each element is a raw pointer; ownership stays in the block.
    std::vector<Instr*> all_instrs() const;

private:
    Label* label_;
    std::vector<InstrPtr> phis_;
    std::vector<InstrPtr> instrs_;
    InstrPtr terminator_;
};

// Block ID type for CFG mappings.
using BlockId = BasicBlock*;
