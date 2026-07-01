#include "toyc/ir/basic_block.h"

BasicBlock::BasicBlock(Label* label)
    : label_(label) {}

void BasicBlock::add_phi(InstrPtr phi) {
    phis_.push_back(std::move(phi));
}

void BasicBlock::clear_phis() {
    phis_.clear();
}

void BasicBlock::insert_instruction(size_t index, InstrPtr instr) {
    if (is_terminated()) return;
    if (index > instrs_.size()) index = instrs_.size();
    instrs_.insert(instrs_.begin() + index, std::move(instr));
}

void BasicBlock::add_instruction(InstrPtr instr) {
    if (is_terminated()) {
        // Mimics Java: "cannot append instruction after terminator"
        // For now, silently ignore; caller should check.
        return;
    }
    instrs_.push_back(std::move(instr));
}

void BasicBlock::replace_body(std::vector<InstrPtr> new_instrs) {
    instrs_ = std::move(new_instrs);
}

void BasicBlock::set_terminator(InstrPtr term) {
    // Java validates isTerminator() here; we trust the caller for now.
    terminator_ = std::move(term);
}

std::vector<Instr*> BasicBlock::all_instrs() const {
    std::vector<Instr*> result;
    result.reserve(phis_.size() + instrs_.size() + (terminator_ ? 1 : 0));
    for (const auto& p : phis_) {
        result.push_back(p.get());
    }
    for (const auto& i : instrs_) {
        result.push_back(i.get());
    }
    if (terminator_) {
        result.push_back(terminator_.get());
    }
    return result;
}
