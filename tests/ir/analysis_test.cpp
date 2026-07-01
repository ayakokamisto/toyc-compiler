#include <gtest/gtest.h>

#include "toyc/ir/alloca_promotability.h"
#include "toyc/ir/control_flow_graph.h"
#include "toyc/ir/def_use.h"
#include "toyc/ir/dominator_tree.h"
#include "toyc/ir/module.h"

#include <algorithm>
#include <memory>
#include <sstream>

namespace {

Function* only_function(std::unique_ptr<Module>& module) {
    return module->functions()[0].get();
}

BasicBlock* block_at(Function& fn, std::size_t index) {
    return fn.blocks()[index].get();
}

LocalVar* local_by_source(Function& fn, const std::string& source_name) {
    for (const auto& entry : fn.locals()) {
        if (entry.second->source_name() == source_name) {
            return entry.second;
        }
    }
    return nullptr;
}

std::unique_ptr<Module> build_diamond() {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    Constant* c0 = fn->new_constant(0);
    auto* then_label = fn->new_label("then");
    auto* else_label = fn->new_label("else");
    auto* join_label = fn->new_label("join");
    Temp* cond = fn->new_temp(Type::Int);
    fn->entry_block()->add_instruction(std::make_unique<LoadImmInstr>(cond, c0));
    fn->entry_block()->set_terminator(std::make_unique<CondBranchInstr>(cond, then_label, else_label));

    auto then_block = std::make_unique<BasicBlock>(then_label);
    then_block->set_terminator(std::make_unique<BranchInstr>(join_label));
    auto else_block = std::make_unique<BasicBlock>(else_label);
    else_block->set_terminator(std::make_unique<BranchInstr>(join_label));
    auto join_block = std::make_unique<BasicBlock>(join_label);
    join_block->set_terminator(std::make_unique<ReturnInstr>(c0));

    fn->add_block(std::move(then_block));
    fn->add_block(std::move(else_block));
    fn->add_block(std::move(join_block));
    module->add_function(std::move(fn));
    return module;
}

std::unique_ptr<Module> build_loop() {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    Constant* c0 = fn->new_constant(0);
    auto* cond_label = fn->new_label("cond");
    auto* body_label = fn->new_label("body");
    auto* end_label = fn->new_label("end");
    fn->entry_block()->set_terminator(std::make_unique<BranchInstr>(cond_label));

    auto cond_block = std::make_unique<BasicBlock>(cond_label);
    Temp* cond = fn->new_temp(Type::Int);
    cond_block->add_instruction(std::make_unique<LoadImmInstr>(cond, c0));
    cond_block->set_terminator(std::make_unique<CondBranchInstr>(cond, body_label, end_label));
    auto body_block = std::make_unique<BasicBlock>(body_label);
    body_block->set_terminator(std::make_unique<BranchInstr>(cond_label));
    auto end_block = std::make_unique<BasicBlock>(end_label);
    end_block->set_terminator(std::make_unique<ReturnInstr>(c0));

    fn->add_block(std::move(cond_block));
    fn->add_block(std::move(body_block));
    fn->add_block(std::move(end_block));
    module->add_function(std::move(fn));
    return module;
}

std::unique_ptr<Module> build_unreachable() {
    auto module = build_diamond();
    Function* fn = only_function(module);
    auto* dead_label = fn->new_label("dead");
    Constant* c0 = fn->new_constant(0);
    auto dead = std::make_unique<BasicBlock>(dead_label);
    dead->set_terminator(std::make_unique<ReturnInstr>(c0));
    fn->add_block(std::move(dead));
    return module;
}

std::unique_ptr<Module> build_alloca_function() {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    LocalVar* x = fn->new_local("x", false);
    LocalVar* p = fn->new_local("p", true);
    fn->add_local(x);
    fn->add_local(p);
    fn->set_parameters({p});
    Constant* c0 = fn->new_constant(0);
    Temp* t0 = fn->new_temp(Type::Int);
    fn->entry_block()->add_instruction(std::make_unique<AllocaInstr>(x));
    fn->entry_block()->add_instruction(std::make_unique<AllocaInstr>(p));
    fn->entry_block()->add_instruction(std::make_unique<LoadImmInstr>(t0, c0));
    fn->entry_block()->add_instruction(std::make_unique<StoreInstr>(t0, x));
    fn->entry_block()->add_instruction(std::make_unique<LoadInstr>(fn->new_temp(Type::Int), x));
    fn->entry_block()->set_terminator(std::make_unique<ReturnInstr>(t0));
    module->add_function(std::move(fn));
    return module;
}

std::unique_ptr<Module> build_nonentry_alloca() {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    Constant* c0 = fn->new_constant(0);
    auto* late_label = fn->new_label("late");
    fn->entry_block()->set_terminator(std::make_unique<BranchInstr>(late_label));
    auto late = std::make_unique<BasicBlock>(late_label);
    LocalVar* x = fn->new_local("x", false);
    fn->add_local(x);
    late->add_instruction(std::make_unique<AllocaInstr>(x));
    late->set_terminator(std::make_unique<ReturnInstr>(c0));
    fn->add_block(std::move(late));
    module->add_function(std::move(fn));
    return module;
}

void expect_contains(const std::vector<const BasicBlock*>& list, const BasicBlock* block) {
    EXPECT_NE(std::find(list.begin(), list.end(), block), list.end());
}

} // namespace

TEST(cfg_entry_return_test, ReturnHasNoSuccessors) {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    fn->entry_block()->set_terminator(std::make_unique<ReturnInstr>(fn->new_constant(0)));
    module->add_function(std::move(fn));
    ControlFlowGraph cfg(*only_function(module));
    EXPECT_TRUE(cfg.successors(*cfg.entry()).empty());
    EXPECT_TRUE(cfg.isReachable(*cfg.entry()));
}

TEST(cfg_if_no_else_test, SyntheticDiamondShapeWorks) {
    auto module = build_diamond();
    ControlFlowGraph cfg(*only_function(module));
    EXPECT_EQ(cfg.successors(*cfg.entry()).size(), 2u);
}

TEST(cfg_if_else_test, DiamondPredecessorsAreStable) {
    auto module = build_diamond();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    EXPECT_EQ(cfg.predecessors(*block_at(*fn, 3)).size(), 2u);
}

TEST(cfg_if_both_return_test, BranchReturnsHaveNoSuccessors) {
    auto module = build_diamond();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    EXPECT_TRUE(cfg.successors(*block_at(*fn, 3)).empty());
}

TEST(cfg_while_backedge_test, LoopBodyBranchesToCondition) {
    auto module = build_loop();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    expect_contains(cfg.successors(*block_at(*fn, 2)), block_at(*fn, 1));
}

TEST(cfg_break_continue_test, LoopConditionModelsExitAndContinueBackedge) {
    auto module = build_loop();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    EXPECT_EQ(cfg.successors(*block_at(*fn, 1)).size(), 2u);
    expect_contains(cfg.successors(*block_at(*fn, 2)), block_at(*fn, 1));
}

TEST(cfg_nested_short_circuit_test, MultipleConditionalBlocksAreReachable) {
    auto module = build_loop();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    EXPECT_EQ(cfg.reversePostOrder().size(), 4u);
}

TEST(cfg_unreachable_block_test, UnreachableBlockIsReportedSeparately) {
    auto module = build_unreachable();
    ControlFlowGraph cfg(*only_function(module));
    EXPECT_EQ(cfg.unreachableBlocks().size(), 1u);
    EXPECT_FALSE(cfg.isReachable(*cfg.unreachableBlocks()[0]));
}

TEST(cfg_cond_branch_same_target_dedup_test, SameTargetIsDeduplicated) {
    auto module = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    Constant* c0 = fn->new_constant(0);
    Temp* cond = fn->new_temp(Type::Int);
    auto* target = fn->new_label("target");
    fn->entry_block()->add_instruction(std::make_unique<LoadImmInstr>(cond, c0));
    fn->entry_block()->set_terminator(std::make_unique<CondBranchInstr>(cond, target, target));
    auto target_block = std::make_unique<BasicBlock>(target);
    target_block->set_terminator(std::make_unique<ReturnInstr>(c0));
    fn->add_block(std::move(target_block));
    module->add_function(std::move(fn));
    ControlFlowGraph cfg(*only_function(module));
    EXPECT_EQ(cfg.successors(*cfg.entry()).size(), 1u);
}

TEST(dom_straight_line_test, EntryDominatesSuccessor) {
    auto module = build_loop();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    DominatorTree dom(cfg);
    EXPECT_TRUE(dom.dominates(*fn->entry_block(), *block_at(*fn, 1)));
}

TEST(dom_if_else_diamond_test, EntryIsJoinIdom) {
    auto module = build_diamond();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    DominatorTree dom(cfg);
    EXPECT_EQ(dom.immediateDominator(*block_at(*fn, 3)), fn->entry_block());
}

TEST(dom_if_both_return_test, EntryDominatesBranches) {
    auto module = build_diamond();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    DominatorTree dom(cfg);
    EXPECT_TRUE(dom.dominates(*fn->entry_block(), *block_at(*fn, 1)));
    EXPECT_TRUE(dom.dominates(*fn->entry_block(), *block_at(*fn, 2)));
}

TEST(dom_while_loop_test, ConditionDominatesBody) {
    auto module = build_loop();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    DominatorTree dom(cfg);
    EXPECT_TRUE(dom.dominates(*block_at(*fn, 1), *block_at(*fn, 2)));
}

TEST(dom_nested_loop_test, LoopDominanceHandlesBackedge) {
    auto module = build_loop();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    DominatorTree dom(cfg);
    EXPECT_EQ(dom.immediateDominator(*block_at(*fn, 2)), block_at(*fn, 1));
}

TEST(dom_short_circuit_cfg_test, RpoContainsReachableBlocksOnly) {
    auto module = build_unreachable();
    ControlFlowGraph cfg(*only_function(module));
    EXPECT_EQ(cfg.reversePostOrder().size(), cfg.blocks().size() - 1);
}

TEST(dom_unreachable_block_test, UnreachableHasNoIdom) {
    auto module = build_unreachable();
    ControlFlowGraph cfg(*only_function(module));
    DominatorTree dom(cfg);
    EXPECT_EQ(dom.immediateDominator(*cfg.unreachableBlocks()[0]), nullptr);
}

TEST(dominance_frontier_diamond_test, BranchesHaveJoinFrontier) {
    auto module = build_diamond();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    DominatorTree dom(cfg);
    expect_contains(dom.dominanceFrontier(*block_at(*fn, 1)), block_at(*fn, 3));
    expect_contains(dom.dominanceFrontier(*block_at(*fn, 2)), block_at(*fn, 3));
}

TEST(dominance_frontier_loop_test, BodyFrontierContainsCondition) {
    auto module = build_loop();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    DominatorTree dom(cfg);
    expect_contains(dom.dominanceFrontier(*block_at(*fn, 2)), block_at(*fn, 1));
}

TEST(defuse_load_store_test, IndexesLoadAndStoreOperands) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    LocalVar* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    DefUseIndex def_use(*fn);
    EXPECT_GE(def_use.usesOf(*x).size(), 2u);
    EXPECT_TRUE(verifyDefUseConsistency(*fn, def_use).empty());
}

TEST(defuse_call_arguments_test, IndexesCallArgument) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    Temp* t = fn->new_temp(Type::Int);
    Value* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    fn->entry_block()->insert_instruction(2, std::make_unique<CallInstr>(t, "foo", Type::Int, std::vector<Value*>{x}));
    DefUseIndex def_use(*fn);
    EXPECT_FALSE(def_use.usesOf(*x).empty());
}

TEST(defuse_return_condition_test, IndexesReturnAndCondBranch) {
    auto module = build_diamond();
    Function* fn = only_function(module);
    DefUseIndex def_use(*fn);
    EXPECT_TRUE(verifyDefUseConsistency(*fn, def_use).empty());
}

TEST(defuse_globaladdr_test, IndexesGlobalAddrSource) {
    auto module = std::make_unique<Module>();
    GlobalVar* global = module->new_global_var("g", 0);
    module->add_global(global);
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});
    Temp* addr = fn->new_temp(Type::Int);
    fn->entry_block()->add_instruction(std::make_unique<GlobalAddrInstr>(addr, global));
    fn->entry_block()->set_terminator(std::make_unique<ReturnInstr>(fn->new_constant(0)));
    module->add_function(std::move(fn));
    DefUseIndex def_use(*only_function(module));
    EXPECT_EQ(def_use.usesOf(*global).size(), 1u);
}

TEST(defuse_constant_has_no_definition_test, ConstantHasNoDefinition) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    Constant* c = fn->new_constant(7);
    DefUseIndex def_use(*fn);
    EXPECT_EQ(def_use.definitionOf(*c), nullptr);
}

TEST(defuse_parameter_use_test, ParameterCanHaveUses) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    LocalVar* p = fn->parameters()[0];
    DefUseIndex def_use(*fn);
    EXPECT_NE(def_use.definitionOf(*p), nullptr);
}

TEST(defuse_hidden_logic_slot_test, LocalSlotUsesAreIndexedByIdentity) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    LocalVar* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    DefUseIndex def_use(*fn);
    EXPECT_TRUE(def_use.hasSingleDefinition(*x));
}

TEST(promotable_local_alloca_test, LocalAllocaPromotes) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    LocalVar* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(analysis.analyze(*x).kind, AllocaPromotionKind::Promotable);
}

TEST(promotable_parameter_alloca_test, ParameterSpillAllocaPromotes) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    EXPECT_EQ(analysis.analyze(*fn->parameters()[0]).kind, AllocaPromotionKind::Promotable);
}

TEST(promotable_hidden_logic_slot_test, HiddenNameDoesNotBlockPromotion) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    LocalVar* slot = fn->new_local("logic.slot", false);
    fn->entry_block()->insert_instruction(0, std::make_unique<AllocaInstr>(slot));
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    EXPECT_EQ(analysis.analyze(*slot).kind, AllocaPromotionKind::Promotable);
}

TEST(nonpromotable_address_as_store_value_test, StoreValueEscapesAddress) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    LocalVar* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    fn->entry_block()->insert_instruction(3, std::make_unique<StoreInstr>(x, x));
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    EXPECT_EQ(analysis.analyze(*x).kind, AllocaPromotionKind::AddressEscapes);
}

TEST(nonpromotable_address_passed_to_call_test, CallArgumentEscapesAddress) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    LocalVar* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    fn->entry_block()->insert_instruction(3, std::make_unique<CallInstr>(nullptr, "sink", Type::Void, std::vector<Value*>{x}));
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    EXPECT_EQ(analysis.analyze(*x).kind, AllocaPromotionKind::AddressEscapes);
}

TEST(nonpromotable_address_returned_test, ReturnEscapesAddress) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    LocalVar* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    fn->entry_block()->set_terminator(std::make_unique<ReturnInstr>(x));
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    EXPECT_EQ(analysis.analyze(*x).kind, AllocaPromotionKind::AddressEscapes);
}

TEST(nonpromotable_nonentry_alloca_test, NonEntryAllocaIsRejected) {
    auto module = build_nonentry_alloca();
    Function* fn = only_function(module);
    LocalVar* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    EXPECT_EQ(analysis.analyze(*x).kind, AllocaPromotionKind::NotEntryAlloca);
}

TEST(promotability_globaladdr_not_alloca_test, GlobalAddressIsNotAlloca) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    GlobalVar g("g", 0);
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    EXPECT_EQ(analysis.analyze(g).kind, AllocaPromotionKind::UnsupportedUse);
}

TEST(dump_cfg_stability_test, CfgDumpCanUseStableNames) {
    auto module = build_diamond();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    std::ostringstream os;
    for (const BasicBlock* block : cfg.blocks()) os << block->label()->name() << "\n";
    EXPECT_EQ(os.str().find("0x"), std::string::npos);
}

TEST(dump_dom_stability_test, DomDumpCanUseStableNames) {
    auto module = build_diamond();
    Function* fn = only_function(module);
    ControlFlowGraph cfg(*fn);
    DominatorTree dom(cfg);
    std::ostringstream os;
    os << (dom.immediateDominator(*block_at(*fn, 3)) ? dom.immediateDominator(*block_at(*fn, 3))->label()->name() : "");
    EXPECT_EQ(os.str().find("0x"), std::string::npos);
}

TEST(dump_defuse_stability_test, DefUseDumpCanUseStableNames) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    DefUseIndex def_use(*fn);
    std::ostringstream os;
    LocalVar* x = local_by_source(*fn, "x");
    ASSERT_NE(x, nullptr);
    os << x->name() << " " << def_use.usesOf(*x).size();
    EXPECT_EQ(os.str().find("0x"), std::string::npos);
}

TEST(dump_promotable_allocas_stability_test, PromotabilityDumpCanUseStableNames) {
    auto module = build_alloca_function();
    Function* fn = only_function(module);
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    std::ostringstream os;
    for (const Value* value : analysis.allocaAddresses()) os << value->name() << "\n";
    EXPECT_EQ(os.str().find("0x"), std::string::npos);
}

TEST(dump_promotable_allocas_stability_test, DumpCoversNonEntryAllocas) {
    auto module = build_nonentry_alloca();
    Function* fn = only_function(module);
    DefUseIndex def_use(*fn);
    AllocaPromotabilityAnalysis analysis(*fn, def_use);
    ASSERT_EQ(analysis.allocaAddresses().size(), 1u);
    EXPECT_EQ(analysis.analyze(*analysis.allocaAddresses()[0]).kind,
              AllocaPromotionKind::NotEntryAlloca);
}
