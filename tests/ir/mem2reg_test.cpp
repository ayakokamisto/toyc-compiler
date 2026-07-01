#include <gtest/gtest.h>

#include "toyc/ir/alloca_promotability.h"
#include "toyc/ir/control_flow_graph.h"
#include "toyc/ir/def_use.h"
#include "toyc/ir/dominator_tree.h"
#include "toyc/ir/ir_verifier.h"
#include "toyc/ir/mem2reg.h"
#include "toyc/ir/module.h"

#include <algorithm>
#include <memory>

namespace {

Function* only_function(std::unique_ptr<Module>& module) {
    return module->functions()[0].get();
}

Mem2RegResult run_mem2reg(Function& function) {
    ControlFlowGraph cfg(function);
    DominatorTree dom(cfg);
    DefUseIndex def_use(function);
    AllocaPromotabilityAnalysis promotability(function, def_use);
    return promoteMemToReg(function, cfg, dom, def_use, promotability);
}

std::size_t count_kind(const Function& function, InstrKind kind) {
    std::size_t count = 0;
    for (const auto& block : function.blocks()) {
        for (const Instruction* instruction : block->all_instrs()) {
            if (instruction->kind() == kind) ++count;
        }
    }
    return count;
}

std::unique_ptr<Module> build_straight_line_alloca() {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    LocalVar* x = fn->new_local("x", false);
    Constant* c7 = fn->new_constant(7);
    Temp* value = fn->new_temp(Type::Int);
    Temp* loaded = fn->new_temp(Type::Int);
    fn->entry_block()->add_instruction(std::make_unique<AllocaInstr>(x));
    fn->entry_block()->add_instruction(std::make_unique<LoadImmInstr>(value, c7));
    fn->entry_block()->add_instruction(std::make_unique<StoreInstr>(value, x));
    fn->entry_block()->add_instruction(std::make_unique<LoadInstr>(loaded, x));
    fn->entry_block()->set_terminator(std::make_unique<ReturnInstr>(loaded));
    module->add_function(std::move(fn));
    return module;
}

std::unique_ptr<Module> build_diamond_alloca() {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    LocalVar* x = fn->new_local("x", false);
    Constant* c0 = fn->new_constant(0);
    Constant* c1 = fn->new_constant(1);
    Constant* c2 = fn->new_constant(2);
    auto* then_label = fn->new_label("then");
    auto* else_label = fn->new_label("else");
    auto* join_label = fn->new_label("join");
    Temp* init = fn->new_temp(Type::Int);
    Temp* cond = fn->new_temp(Type::Int);
    fn->entry_block()->add_instruction(std::make_unique<AllocaInstr>(x));
    fn->entry_block()->add_instruction(std::make_unique<LoadImmInstr>(init, c0));
    fn->entry_block()->add_instruction(std::make_unique<StoreInstr>(init, x));
    fn->entry_block()->add_instruction(std::make_unique<LoadImmInstr>(cond, c1));
    fn->entry_block()->set_terminator(std::make_unique<CondBranchInstr>(cond, then_label, else_label));

    auto then_block = std::make_unique<BasicBlock>(then_label);
    Temp* then_value = fn->new_temp(Type::Int);
    then_block->add_instruction(std::make_unique<LoadImmInstr>(then_value, c1));
    then_block->add_instruction(std::make_unique<StoreInstr>(then_value, x));
    then_block->set_terminator(std::make_unique<BranchInstr>(join_label));

    auto else_block = std::make_unique<BasicBlock>(else_label);
    Temp* else_value = fn->new_temp(Type::Int);
    else_block->add_instruction(std::make_unique<LoadImmInstr>(else_value, c2));
    else_block->add_instruction(std::make_unique<StoreInstr>(else_value, x));
    else_block->set_terminator(std::make_unique<BranchInstr>(join_label));

    auto join_block = std::make_unique<BasicBlock>(join_label);
    Temp* loaded = fn->new_temp(Type::Int);
    join_block->add_instruction(std::make_unique<LoadInstr>(loaded, x));
    join_block->set_terminator(std::make_unique<ReturnInstr>(loaded));

    fn->add_block(std::move(then_block));
    fn->add_block(std::move(else_block));
    fn->add_block(std::move(join_block));
    module->add_function(std::move(fn));
    return module;
}

std::unique_ptr<Module> build_load_before_store() {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    LocalVar* x = fn->new_local("x", false);
    Temp* loaded = fn->new_temp(Type::Int);
    fn->entry_block()->add_instruction(std::make_unique<AllocaInstr>(x));
    fn->entry_block()->add_instruction(std::make_unique<LoadInstr>(loaded, x));
    fn->entry_block()->set_terminator(std::make_unique<ReturnInstr>(loaded));
    module->add_function(std::move(fn));
    return module;
}

} // namespace

TEST(phi_only_in_phi_prefix_test, BodyPhiIsRejected) {
    auto module = build_straight_line_alloca();
    Function* fn = only_function(module);
    fn->entry_block()->add_instruction(std::make_unique<PhiInstr>(fn->new_temp(Type::Int)));
    auto errors = verifyIR(*fn);
    EXPECT_FALSE(errors.empty());
}

TEST(phi_duplicate_predecessor_rejected_test, DuplicateIncomingIsRejected) {
    auto module = build_diamond_alloca();
    Function* fn = only_function(module);
    BasicBlock* join = fn->blocks()[3].get();
    auto phi = std::make_unique<PhiInstr>(fn->new_temp(Type::Int));
    phi->add_incoming(fn->blocks()[1]->label(), fn->new_constant(1));
    phi->add_incoming(fn->blocks()[1]->label(), fn->new_constant(2));
    join->add_phi(std::move(phi));
    auto errors = verifySSA(*fn);
    EXPECT_FALSE(errors.empty());
}

TEST(mem2reg_single_local_straight_line_test, RemovesAllocaLoadStore) {
    auto module = build_straight_line_alloca();
    Function* fn = only_function(module);
    auto result = run_mem2reg(*fn);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.promotedAllocas.size(), 1u);
    EXPECT_EQ(count_kind(*fn, InstrKind::Alloca), 0u);
    EXPECT_EQ(count_kind(*fn, InstrKind::Load), 0u);
    EXPECT_EQ(count_kind(*fn, InstrKind::Store), 0u);
    EXPECT_TRUE(verifyIR(*fn).empty());
    EXPECT_TRUE(verifySSA(*fn).empty());
}

TEST(mem2reg_if_diamond_phi_test, PlacesJoinPhi) {
    auto module = build_diamond_alloca();
    Function* fn = only_function(module);
    auto result = run_mem2reg(*fn);
    EXPECT_TRUE(result.changed);
    BasicBlock* join = fn->blocks()[3].get();
    ASSERT_EQ(join->phis().size(), 1u);
    auto* phi = dynamic_cast<PhiInstr*>(join->phis()[0].get());
    ASSERT_NE(phi, nullptr);
    EXPECT_EQ(phi->incoming().size(), 2u);
    EXPECT_TRUE(verifyIR(*fn).empty());
    EXPECT_TRUE(verifySSA(*fn).empty());
}

TEST(mem2reg_load_before_definition_skipped_test, KeepsMemoryForm) {
    auto module = build_load_before_store();
    Function* fn = only_function(module);
    auto result = run_mem2reg(*fn);
    EXPECT_FALSE(result.changed);
    EXPECT_EQ(result.skippedAllocas.size(), 1u);
    EXPECT_EQ(count_kind(*fn, InstrKind::Alloca), 1u);
    EXPECT_EQ(count_kind(*fn, InstrKind::Load), 1u);
}

TEST(phi_result_single_definition_test, VerifySsaAcceptsMem2RegPhi) {
    auto module = build_diamond_alloca();
    Function* fn = only_function(module);
    run_mem2reg(*fn);
    EXPECT_TRUE(verifySSA(*fn).empty());
}
