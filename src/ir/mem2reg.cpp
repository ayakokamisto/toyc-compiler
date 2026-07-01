#include "toyc/ir/mem2reg.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace {

using PhiMap = std::unordered_map<const Value*, std::unordered_map<const BasicBlock*, PhiInstr*>>;
using StackMap = std::unordered_map<const Value*, std::vector<Value*>>;

bool isLoadFrom(const Instruction& instruction, const Value* address) {
    auto* load = dynamic_cast<const LoadInstr*>(&instruction);
    return load != nullptr && load->address() == address;
}

bool isStoreTo(const Instruction& instruction, const Value* address) {
    auto* store = dynamic_cast<const StoreInstr*>(&instruction);
    return store != nullptr && store->address() == address;
}

bool hasLoadBeforeDefinition(const Function& function, const Value* address) {
    bool seen_store = false;
    for (const auto& block : function.blocks()) {
        for (const Instruction* instruction : block->all_instrs()) {
            if (isStoreTo(*instruction, address)) {
                seen_store = true;
            } else if (isLoadFrom(*instruction, address) && !seen_store) {
                return true;
            }
        }
    }
    return false;
}

bool hasUseInUnreachableBlock(
    const Function& function,
    const ControlFlowGraph& cfg,
    const Value* address) {
    for (const auto& block : function.blocks()) {
        if (cfg.isReachable(*block)) continue;
        for (const Instruction* instruction : block->all_instrs()) {
            if (isLoadFrom(*instruction, address) || isStoreTo(*instruction, address)) {
                return true;
            }
        }
    }
    return false;
}

std::vector<const BasicBlock*> definitionBlocks(const Function& function, const Value* address) {
    std::vector<const BasicBlock*> blocks;
    for (const auto& block : function.blocks()) {
        for (const Instruction* instruction : block->all_instrs()) {
            if (isStoreTo(*instruction, address)) {
                if (std::find(blocks.begin(), blocks.end(), block.get()) == blocks.end()) {
                    blocks.push_back(block.get());
                }
            }
        }
    }
    return blocks;
}

void placePhiNodes(
    Function& function,
    const DominatorTree& domTree,
    const std::vector<const Value*>& allocas,
    PhiMap& phis) {
    for (const Value* alloca : allocas) {
        std::vector<const BasicBlock*> work = definitionBlocks(function, alloca);
        std::unordered_set<const BasicBlock*> ever_on_worklist(work.begin(), work.end());
        std::unordered_set<const BasicBlock*> has_already;

        while (!work.empty()) {
            const BasicBlock* block = work.back();
            work.pop_back();
            for (const BasicBlock* frontier : domTree.dominanceFrontier(*block)) {
                if (!has_already.insert(frontier).second) continue;

                auto* mutable_block = const_cast<BasicBlock*>(frontier);
                auto* result = const_cast<Function&>(function).new_temp(Type::Int);
                auto phi = std::make_unique<PhiInstr>(result);
                PhiInstr* raw_phi = phi.get();
                mutable_block->add_phi(std::move(phi));
                phis[alloca][frontier] = raw_phi;

                if (ever_on_worklist.insert(frontier).second) {
                    work.push_back(frontier);
                }
            }
        }
    }
}

void renameBlock(
    Function& function,
    const ControlFlowGraph& cfg,
    const DominatorTree& domTree,
    const BasicBlock* block,
    const std::vector<const Value*>& allocas,
    const std::unordered_set<const Value*>& promoted,
    PhiMap& phis,
    StackMap& stacks,
    Mem2RegResult& result) {
    std::vector<const Value*> pushed_allocas;

    for (const Value* alloca : allocas) {
        auto phi_it = phis[alloca].find(block);
        if (phi_it != phis[alloca].end()) {
            stacks[alloca].push_back(phi_it->second->result());
            pushed_allocas.push_back(alloca);
        }
    }

    auto* mutable_block = const_cast<BasicBlock*>(block);
    std::vector<InstrPtr> kept;
    for (auto& instruction_ptr : mutable_block->mutable_instructions()) {
        Instruction* instruction = instruction_ptr.get();
        bool remove = false;

        if (instruction->kind() == InstrKind::Alloca && promoted.count(instruction->result()) != 0) {
            remove = true;
        } else if (auto* store = dynamic_cast<StoreInstr*>(instruction)) {
            const Value* address = store->address();
            if (promoted.count(address) != 0) {
                stacks[address].push_back(store->value());
                pushed_allocas.push_back(address);
                remove = true;
            }
        } else if (auto* load = dynamic_cast<LoadInstr*>(instruction)) {
            const Value* address = load->address();
            if (promoted.count(address) != 0) {
                auto& stack = stacks[address];
                if (stack.empty()) {
                    result.diagnostics.push_back(
                        "mem2reg skipped alloca '" + address->name() + "': load has no reaching definition");
                    result.skippedAllocas.push_back(address);
                } else {
                    replaceAllUsesWith(function, *load->result(), *stack.back());
                    remove = true;
                }
            }
        }

        if (!remove) {
            kept.push_back(std::move(instruction_ptr));
        } else {
            result.changed = true;
        }
    }
    mutable_block->mutable_instructions() = std::move(kept);

    for (const BasicBlock* successor : cfg.successors(*block)) {
        for (const Value* alloca : allocas) {
            auto phi_it = phis[alloca].find(successor);
            if (phi_it == phis[alloca].end()) continue;
            auto& stack = stacks[alloca];
            if (!stack.empty()) {
                phi_it->second->add_incoming(const_cast<BasicBlock*>(block)->label(), stack.back());
            }
        }
    }

    for (const BasicBlock* child : domTree.children(*block)) {
        renameBlock(function, cfg, domTree, child, allocas, promoted, phis, stacks, result);
    }

    for (auto it = pushed_allocas.rbegin(); it != pushed_allocas.rend(); ++it) {
        auto& stack = stacks[*it];
        if (!stack.empty()) {
            stack.pop_back();
        }
    }
}

} // namespace

Mem2RegResult promoteMemToReg(
    Function& function,
    const ControlFlowGraph& cfg,
    const DominatorTree& domTree,
    const DefUseIndex&,
    const AllocaPromotabilityAnalysis& promotability) {
    Mem2RegResult result;
    std::vector<const Value*> candidates = promotability.promotableAllocas();
    std::vector<const Value*> promoted_allocas;

    for (const Value* alloca : candidates) {
        if (hasUseInUnreachableBlock(function, cfg, alloca)) {
            result.skippedAllocas.push_back(alloca);
            result.diagnostics.push_back(
                "mem2reg skipped alloca '" + alloca->name() + "': alloca is used in unreachable block");
        } else if (hasLoadBeforeDefinition(function, alloca)) {
            result.skippedAllocas.push_back(alloca);
            result.diagnostics.push_back(
                "mem2reg skipped alloca '" + alloca->name() + "': load has no reaching definition");
        } else {
            promoted_allocas.push_back(alloca);
            result.promotedAllocas.push_back(alloca);
        }
    }

    if (promoted_allocas.empty()) {
        return result;
    }

    PhiMap phis;
    placePhiNodes(function, domTree, promoted_allocas, phis);

    StackMap stacks;
    std::unordered_set<const Value*> promoted_set(promoted_allocas.begin(), promoted_allocas.end());
    if (cfg.entry() != nullptr) {
        renameBlock(function, cfg, domTree, cfg.entry(), promoted_allocas, promoted_set, phis, stacks, result);
    }

    return result;
}
