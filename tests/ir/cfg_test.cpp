#include <gtest/gtest.h>

#include "toyc/ir/basic_block.h"
#include "toyc/ir/cfg.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/ir_printer.h"
#include "toyc/ir/module.h"
#include "toyc/ir/value.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

// =============================================================================
// Helpers
// =============================================================================

// Build a simple if/else CFG:
//   entry → [then, else] → end
static std::unique_ptr<Module> build_if_else_fixture() {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("test", Type::Int, std::vector<LocalVar*>{});

    LocalVar* x = fn->new_local("x", false);
    Temp* t0 = fn->new_temp(Type::Int);
    Temp* cval = fn->new_temp(Type::Int);
    Constant* c1 = fn->new_constant(1);
    Constant* c0 = fn->new_constant(0);

    auto* then_lab = fn->new_label("then");
    auto* else_lab = fn->new_label("else");
    auto* end_lab = fn->new_label("end");

    // Entry: x = 1; if (x) goto then else else
    BasicBlock* entry = fn->entry_block();
    entry->add_instruction(std::make_unique<AllocaInstr>(x));
    entry->add_instruction(std::make_unique<LoadImmInstr>(cval, c1));
    entry->add_instruction(std::make_unique<StoreInstr>(cval, x));
    entry->add_instruction(std::make_unique<LoadInstr>(t0, x));
    entry->set_terminator(std::make_unique<CondBranchInstr>(t0, then_lab, else_lab));

    // Then: ret 1
    auto then_bb = std::make_unique<BasicBlock>(then_lab);
    then_bb->set_terminator(std::make_unique<ReturnInstr>(c1));

    // Else: goto end
    auto else_bb = std::make_unique<BasicBlock>(else_lab);
    else_bb->set_terminator(std::make_unique<BranchInstr>(end_lab));

    // End: ret 0
    auto end_bb = std::make_unique<BasicBlock>(end_lab);
    end_bb->set_terminator(std::make_unique<ReturnInstr>(c0));

    fn->add_block(std::move(then_bb));
    fn->add_block(std::move(else_bb));
    fn->add_block(std::move(end_bb));

    mod->add_function(std::move(fn));
    return mod;
}

// Build a while-loop CFG:
//   entry → cond → body → cond  (backedge)
//                → end
static std::unique_ptr<Module> build_while_loop_fixture() {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("test", Type::Int, std::vector<LocalVar*>{});

    LocalVar* x = fn->new_local("x", false);
    Temp* t0 = fn->new_temp(Type::Int);
    Temp* t1 = fn->new_temp(Type::Int);
    Temp* t2 = fn->new_temp(Type::Int);
    Constant* c0 = fn->new_constant(0);
    Constant* c10 = fn->new_constant(10);
    Constant* c1 = fn->new_constant(1);

    auto* cond_lab = fn->new_label("while.cond");
    auto* body_lab = fn->new_label("while.body");
    auto* end_lab = fn->new_label("while.end");

    // Entry: x = 0; goto cond
    BasicBlock* entry = fn->entry_block();
    entry->add_instruction(std::make_unique<AllocaInstr>(x));
    entry->add_instruction(std::make_unique<LoadImmInstr>(t0, c0));
    entry->add_instruction(std::make_unique<StoreInstr>(t0, x));
    entry->set_terminator(std::make_unique<BranchInstr>(cond_lab));

    // Cond: %t1 = load x; if %t1 < 10 goto body else end
    auto cond_bb = std::make_unique<BasicBlock>(cond_lab);
    Temp* t_cond = fn->new_temp(Type::Int);
    Temp* t_limit = fn->new_temp(Type::Int);
    cond_bb->add_instruction(std::make_unique<LoadInstr>(t_cond, x));
    cond_bb->add_instruction(std::make_unique<LoadImmInstr>(t_limit, c10));
    Temp* cmp_res = fn->new_temp(Type::Int);
    cond_bb->add_instruction(std::make_unique<CompareInstr>(cmp_res, CompareInstr::Predicate::Lt, t_cond, t_limit));
    cond_bb->set_terminator(std::make_unique<CondBranchInstr>(cmp_res, body_lab, end_lab));

    // Body: x = x + 1; goto cond
    auto body_bb = std::make_unique<BasicBlock>(body_lab);
    Temp* t_old = fn->new_temp(Type::Int);
    Temp* t_new = fn->new_temp(Type::Int);
    Temp* t_one = fn->new_temp(Type::Int);
    body_bb->add_instruction(std::make_unique<LoadInstr>(t_old, x));
    body_bb->add_instruction(std::make_unique<LoadImmInstr>(t_one, c1));
    body_bb->add_instruction(std::make_unique<BinaryOpInstr>(t_new, BinaryOpInstr::Op::Add, t_old, t_one));
    body_bb->add_instruction(std::make_unique<StoreInstr>(t_new, x));
    body_bb->set_terminator(std::make_unique<BranchInstr>(cond_lab));

    // End: ret 0
    auto end_bb = std::make_unique<BasicBlock>(end_lab);
    Temp* t_ret = fn->new_temp(Type::Int);
    end_bb->add_instruction(std::make_unique<LoadImmInstr>(t_ret, c0));
    end_bb->set_terminator(std::make_unique<ReturnInstr>(t_ret));

    fn->add_block(std::move(cond_bb));
    fn->add_block(std::move(body_bb));
    fn->add_block(std::move(end_bb));

    mod->add_function(std::move(fn));
    return mod;
}

// Build a CFG with an unreachable block:
//   entry → end
//   unreachable → unreachable
static std::unique_ptr<Module> build_unreachable_fixture() {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("test", Type::Int, std::vector<LocalVar*>{});

    Constant* c0 = fn->new_constant(0);
    auto* end_lab = fn->new_label("end");
    auto* dead_lab = fn->new_label("dead");

    // Entry: goto end
    BasicBlock* entry = fn->entry_block();
    entry->set_terminator(std::make_unique<BranchInstr>(end_lab));

    // End: ret 0
    auto end_bb = std::make_unique<BasicBlock>(end_lab);
    end_bb->set_terminator(std::make_unique<ReturnInstr>(c0));

    // Dead block (unreachable): ret 0
    auto dead_bb = std::make_unique<BasicBlock>(dead_lab);
    dead_bb->set_terminator(std::make_unique<ReturnInstr>(c0));

    fn->add_block(std::move(end_bb));
    fn->add_block(std::move(dead_bb));

    mod->add_function(std::move(fn));
    return mod;
}

// =============================================================================
// Test 1: CFG Branch — CondBranch has 2 successors
// =============================================================================

TEST(CFGBranchTest, CondBranchHasTwoSuccessors) {
    auto mod = build_if_else_fixture();
    Function* fn = mod->functions()[0].get();
    CFG cfg = CFG::build(*fn);

    // Entry block has a CondBranch → 2 successors.
    // Need to find it by scanning blocks (it may not be blocks[0]).
    BasicBlock* entry = fn->entry_block();
    auto succ = cfg.successors(entry);
    ASSERT_EQ(succ.size(), 2u);

    // Both successors should have entry as a predecessor.
    for (BasicBlock* s : succ) {
        auto pred = cfg.predecessors(s);
        bool found = std::find(pred.begin(), pred.end(), entry) != pred.end();
        EXPECT_TRUE(found) << "Successor " << s->label()->name()
                           << " should have entry as predecessor";
    }
}

TEST(CFGBranchTest, BranchHasOneSuccessor) {
    auto mod = build_while_loop_fixture();
    Function* fn = mod->functions()[0].get();
    CFG cfg = CFG::build(*fn);

    // Find the else block (has Branch, not CondBranch / Return).
    for (auto& bb : fn->blocks()) {
        Instr* term = bb->terminator();
        if (term && term->kind() == InstrKind::Branch) {
            auto succ = cfg.successors(bb.get());
            ASSERT_EQ(succ.size(), 1u);
        }
    }
}

TEST(CFGBranchTest, ReturnBlockHasNoSuccessors) {
    auto mod = build_if_else_fixture();
    Function* fn = mod->functions()[0].get();
    CFG cfg = CFG::build(*fn);

    // Find blocks ending with Return — they have 0 successors.
    for (auto& bb : fn->blocks()) {
        Instr* term = bb->terminator();
        if (term && term->kind() == InstrKind::Return) {
            EXPECT_TRUE(cfg.successors(bb.get()).empty());
        }
    }
}

// =============================================================================
// Test 2: CFG Loop — detect backedge
// =============================================================================

TEST(CFGLoopTest, WhileLoopBackedge) {
    auto mod = build_while_loop_fixture();
    Function* fn = mod->functions()[0].get();
    CFG cfg = CFG::build(*fn);

    // Build a set of blocks by label name for easy lookup.
    std::unordered_map<std::string, BasicBlock*> blocks_by_label;
    for (auto& bb : fn->blocks()) {
        blocks_by_label[bb->label()->name()] = bb.get();
    }

    // The cond block's successors should include body.
    // The body block's successor should include cond (the backedge).
    // Find the body block by looking for one whose successors include cond.
    BasicBlock* cond_block = nullptr;
    BasicBlock* body_block = nullptr;

    // Find cond block (it's the one after entry).
    auto& blocks = fn->blocks();
    ASSERT_GE(blocks.size(), 3u);

    // The cond block is blocks[1] (after entry).
    // We identify it by checking: it has a CondBranch.
    cond_block = blocks[1].get();
    ASSERT_NE(cond_block, fn->entry_block());

    auto cond_succ = cfg.successors(cond_block);
    ASSERT_EQ(cond_succ.size(), 2u);

    // One successor is the body, the other is the end.
    // The body block has a backedge to cond.
    for (BasicBlock* s : cond_succ) {
        auto s_succ = cfg.successors(s);
        // If one of this block's successors is cond, it's the body.
        if (std::find(s_succ.begin(), s_succ.end(), cond_block) != s_succ.end()) {
            body_block = s;
        }
    }

    ASSERT_NE(body_block, nullptr) << "No body block found (no backedge to cond)";

    // Verify backedge: body → cond
    auto body_succ = cfg.successors(body_block);
    EXPECT_TRUE(std::find(body_succ.begin(), body_succ.end(), cond_block) != body_succ.end())
        << "Body block should have a backedge to cond block";
}

// =============================================================================
// Test 3: Unreachable Block
// =============================================================================

TEST(UnreachableBlockTest, NoPredecessors) {
    auto mod = build_unreachable_fixture();
    Function* fn = mod->functions()[0].get();
    CFG cfg = CFG::build(*fn);

    // The "dead" block should have no predecessors.
    for (auto& bb : fn->blocks()) {
        if (bb->label()->name().find("dead") != std::string::npos) {
            EXPECT_TRUE(cfg.predecessors(bb.get()).empty());
            EXPECT_FALSE(cfg.is_reachable(bb.get()));
        }
    }
}

TEST(UnreachableBlockTest, ReachableBlocksCount) {
    auto mod = build_unreachable_fixture();
    Function* fn = mod->functions()[0].get();
    CFG cfg = CFG::build(*fn);

    // 2 of 3 blocks are reachable (entry + end).
    EXPECT_EQ(cfg.reachable_blocks().size(), 2u);
    EXPECT_EQ(fn->blocks().size(), 3u);
}

// =============================================================================
// Test 4: Smoke fixture CFG correctness
// =============================================================================

TEST(CFGSmokeTest, SmokeFixtureHasCorrectEdges) {
    // Build the IR program from the driver.
    // We replicate the build from main.cpp here.
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});

    LocalVar* x_local = fn->new_local("x", false);
    Temp* ti1 = fn->new_temp(Type::Int);
    Temp* ti2 = fn->new_temp(Type::Int);
    Temp* ti3 = fn->new_temp(Type::Int);
    Temp* ti4 = fn->new_temp(Type::Int);
    Temp* tj1 = fn->new_temp(Type::Int);
    Temp* tj2 = fn->new_temp(Type::Int);
    Temp* tj3 = fn->new_temp(Type::Int);
    Temp* ek1 = fn->new_temp(Type::Int);
    Constant* c3 = fn->new_constant(3);
    Constant* c5 = fn->new_constant(5);
    Constant* c1 = fn->new_constant(1);
    Constant* c0 = fn->new_constant(0);
    auto* then_lab = fn->new_label("if.then");
    auto* else_lab = fn->new_label("if.else");

    BasicBlock* entry = fn->entry_block();
    entry->add_instruction(std::make_unique<AllocaInstr>(x_local));
    entry->add_instruction(std::make_unique<LoadImmInstr>(ti1, c3));
    entry->add_instruction(std::make_unique<StoreInstr>(ti1, x_local));
    entry->add_instruction(std::make_unique<LoadInstr>(ti2, x_local));
    entry->add_instruction(std::make_unique<LoadImmInstr>(ti4, c5));
    entry->add_instruction(std::make_unique<CompareInstr>(ti3, CompareInstr::Predicate::Lt, ti2, ti4));
    entry->set_terminator(std::make_unique<CondBranchInstr>(ti3, then_lab, else_lab));

    auto then_bb = std::make_unique<BasicBlock>(then_lab);
    then_bb->add_instruction(std::make_unique<LoadInstr>(tj1, x_local));
    then_bb->add_instruction(std::make_unique<LoadImmInstr>(tj3, c1));
    then_bb->add_instruction(std::make_unique<BinaryOpInstr>(tj2, BinaryOpInstr::Op::Add, tj1, tj3));
    then_bb->set_terminator(std::make_unique<ReturnInstr>(tj2));

    auto else_bb = std::make_unique<BasicBlock>(else_lab);
    else_bb->add_instruction(std::make_unique<LoadImmInstr>(ek1, c0));
    else_bb->set_terminator(std::make_unique<ReturnInstr>(ek1));

    fn->add_block(std::move(then_bb));
    fn->add_block(std::move(else_bb));

    mod->add_function(std::move(fn));
    Function* fn_ptr = mod->functions()[0].get();

    // Build CFG.
    CFG cfg = CFG::build(*fn_ptr);

    // Entry has 2 successors.
    auto entry_succ = cfg.successors(fn_ptr->entry_block());
    ASSERT_EQ(entry_succ.size(), 2u);

    // Then block has 0 successors (return).
    // Find it by label.
    BasicBlock* then_block = nullptr;
    BasicBlock* else_block = nullptr;
    for (auto& bb : fn_ptr->blocks()) {
        std::string lbl = bb->label()->name();
        if (lbl.find("then") != std::string::npos) then_block = bb.get();
        if (lbl.find("else") != std::string::npos) else_block = bb.get();
    }
    ASSERT_NE(then_block, nullptr);
    ASSERT_NE(else_block, nullptr);

    EXPECT_TRUE(cfg.successors(then_block).empty());
    EXPECT_TRUE(cfg.successors(else_block).empty());

    // Both have entry as predecessor.
    auto then_pred = cfg.predecessors(then_block);
    EXPECT_TRUE(std::find(then_pred.begin(), then_pred.end(), fn_ptr->entry_block()) != then_pred.end());
    auto else_pred = cfg.predecessors(else_block);
    EXPECT_TRUE(std::find(else_pred.begin(), else_pred.end(), fn_ptr->entry_block()) != else_pred.end());
}
