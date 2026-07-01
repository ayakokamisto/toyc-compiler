#include "toyc/ir/def_use.h"

#include <unordered_map>

DefUseIndex::DefUseIndex(const Function& function) {
    for (const auto& block : function.blocks()) {
        for (const Instruction* instruction : block->all_instrs()) {
            addDefinition(*instruction);
            addUses(*instruction);
        }
    }
}

const Instruction* DefUseIndex::definitionOf(const Value& value) const {
    auto it = definitions_.find(&value);
    return it == definitions_.end() ? nullptr : it->second;
}

const std::vector<UseSite>& DefUseIndex::usesOf(const Value& value) const {
    static const std::vector<UseSite> empty;
    auto it = uses_.find(&value);
    return it == uses_.end() ? empty : it->second;
}

bool DefUseIndex::hasSingleDefinition(const Value& value) const {
    return definitions_.count(&value) == 1;
}

void DefUseIndex::addDefinition(const Instruction& instruction) {
    if (Value* result = instruction.result()) {
        definitions_.emplace(result, &instruction);
    }
}

void DefUseIndex::addUses(const Instruction& instruction) {
    auto operands = instruction.operands();
    for (std::size_t i = 0; i < operands.size(); ++i) {
        if (operands[i] != nullptr) {
            uses_[operands[i]].push_back({&instruction, i});
        }
    }
}

std::vector<std::string> verifyDefUseConsistency(
    const Function& function,
    const DefUseIndex& defUseIndex) {
    std::vector<std::string> errors;
    std::unordered_map<const Value*, const Instruction*> definitions;

    for (const auto& block : function.blocks()) {
        for (const Instruction* instruction : block->all_instrs()) {
            if (Value* result = instruction->result()) {
                if (!definitions.emplace(result, instruction).second) {
                    errors.push_back("value has multiple defining instructions: " + result->name());
                }
                if (defUseIndex.definitionOf(*result) != instruction) {
                    errors.push_back("definition index mismatch for " + result->name());
                }
            }

            auto operands = instruction->operands();
            for (std::size_t i = 0; i < operands.size(); ++i) {
                if (operands[i] == nullptr) {
                    errors.push_back("null operand in instruction");
                    continue;
                }
                bool found = false;
                for (const UseSite& use : defUseIndex.usesOf(*operands[i])) {
                    if (use.user == instruction && use.operandIndex == i) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    errors.push_back("missing use site for " + operands[i]->name());
                }
            }
        }
    }
    return errors;
}

void replaceAllUsesWith(Function& function, Value& oldValue, Value& replacement) {
    for (const auto& block : function.blocks()) {
        for (Instruction* instruction : block->all_instrs()) {
            instruction->replace_operand(&oldValue, &replacement);
        }
    }
}
