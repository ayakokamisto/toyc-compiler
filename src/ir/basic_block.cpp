#include "toyc/ir/basic_block.h"

#include <stdexcept>

BasicBlock::BasicBlock(Label* label)
    : label_(label) {}

void BasicBlock::add_phi(InstrPtr phi) {
    phis_.push_back(std::move(phi));
}

void BasicBlock::clear_phis() {
    phis_.clear();
}

void BasicBlock::insert_instruction(size_t index, InstrPtr instr) {
    if (!instr) throw std::invalid_argument("cannot insert null instruction");
    if (instr->is_terminator())
        throw std::invalid_argument("cannot insert terminator into block body");
    if (index > terminator_index())
        throw std::out_of_range("cannot insert instruction after terminator");
    instrs_.insert(instrs_.begin() + index, std::move(instr));
}

void BasicBlock::add_instruction(InstrPtr instr) {
    if (!instr) throw std::invalid_argument("cannot append null instruction");
    if (instr->is_terminator())
        throw std::invalid_argument("cannot append terminator as body instruction");
    if (is_terminated()) {
        throw std::logic_error("cannot append instruction after terminator");
    }
    instrs_.push_back(std::move(instr));
}

void BasicBlock::replace_body(std::vector<InstrPtr> new_instrs) {
    for (const auto& instr : new_instrs) {
        if (!instr) throw std::invalid_argument("cannot replace body with null instruction");
        if (instr->is_terminator())
            throw std::invalid_argument("cannot replace body with terminator instruction");
    }
    instrs_ = std::move(new_instrs);
}

void BasicBlock::set_terminator(InstrPtr term) {
    if (!term) throw std::invalid_argument("cannot set null terminator");
    if (!term->is_terminator())
        throw std::invalid_argument("terminator instruction required");
    terminator_ = std::move(term);
}

std::vector<Instr*> BasicBlock::all_instrs() const {
    std::vector<Instr*> result;
    result.reserve(phis_.size() + instrs_.size() + (terminator_ ? 1 : 0));
    for (const auto& p : phis_) {
        if (p) result.push_back(p.get());
    }
    for (const auto& i : instrs_) {
        if (i) result.push_back(i.get());
    }
    if (terminator_) {
        result.push_back(terminator_.get());
    }
    return result;
}
