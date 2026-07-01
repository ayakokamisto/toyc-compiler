#include <gtest/gtest.h>

#include "toyc/ir/basic_block.h"
#include "toyc/ir/cfg.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/ir_printer.h"
#include "toyc/ir/module.h"
#include "toyc/ir/value.h"

#include <sstream>
#include <stdexcept>
#include <string>

// =============================================================================
// Helpers — build a trivial function for testing.
// =============================================================================

struct TestFixture {
    std::unique_ptr<Module> mod;
    Function* fn;

    TestFixture() {
        mod = std::make_unique<Module>();
        fn = new Function("test", Type::Int, std::vector<LocalVar*>{});
        mod->add_function(std::unique_ptr<Function>(fn));
    }

    ~TestFixture() {
        // mod owns fn, so we release our raw pointer before mod is destroyed.
        if (mod) {
            fn = nullptr;
        }
    }

    // Disable copy/move.
    TestFixture(const TestFixture&) = delete;
    TestFixture& operator=(const TestFixture&) = delete;
};

// =============================================================================
// Test 1: Value Identity
// =============================================================================

TEST(ValueIdentityTest, SameDisplayNameDifferentIdentity) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* t1 = fn->new_temp(Type::Int);  // %t0
    Temp* t2 = fn->new_temp(Type::Int);  // %t1

    // Different pointers = different identity (matching Java object identity).
    EXPECT_NE(t1, t2);

    // Names are different (sequential temp IDs).
    EXPECT_EQ(t1->name(), "%t0");
    EXPECT_EQ(t2->name(), "%t1");

    // Type is correct.
    EXPECT_EQ(t1->type(), Type::Int);
}

TEST(ValueIdentityTest, LocalVarIdentity) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    LocalVar* a = fn->new_local("x", false);
    LocalVar* b = fn->new_local("x", false);

    // Two locals with the same source name are different values.
    EXPECT_NE(a, b);
    EXPECT_EQ(a->source_name(), "x");
    EXPECT_EQ(b->source_name(), "x");
    EXPECT_EQ(a->name(), "%x.0");
    EXPECT_EQ(b->name(), "%x.1");
}

// =============================================================================
// Test 2: Instruction Use-Def
// =============================================================================

TEST(UseDefTest, AllocaProducesNoOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    LocalVar* x = fn->new_local("x", false);
    AllocaInstr ai(x);

    EXPECT_EQ(ai.result(), x);
    EXPECT_TRUE(ai.operands().empty());
    EXPECT_FALSE(ai.is_terminator());
    EXPECT_TRUE(ai.has_side_effect());
    EXPECT_EQ(ai.kind(), InstrKind::Alloca);
}

TEST(UseDefTest, LoadOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* t = fn->new_temp(Type::Int);
    LocalVar* x = fn->new_local("x", false);
    LoadInstr load(t, x);

    EXPECT_EQ(load.result(), t);
    auto ops = load.operands();
    ASSERT_EQ(ops.size(), 1u);
    EXPECT_EQ(ops[0], x);
    EXPECT_FALSE(load.is_terminator());
    EXPECT_FALSE(load.has_side_effect());
    EXPECT_EQ(load.kind(), InstrKind::Load);
}

TEST(UseDefTest, StoreOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* val = fn->new_temp(Type::Int);
    LocalVar* addr = fn->new_local("x", false);
    StoreInstr store(val, addr);

    EXPECT_EQ(store.result(), nullptr);
    auto ops = store.operands();
    ASSERT_EQ(ops.size(), 2u);
    EXPECT_EQ(ops[0], val);
    EXPECT_EQ(ops[1], addr);
    EXPECT_TRUE(store.has_side_effect());
    EXPECT_EQ(store.kind(), InstrKind::Store);
}

TEST(UseDefTest, BinaryOpOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* r = fn->new_temp(Type::Int);
    Temp* l = fn->new_temp(Type::Int);
    Temp* r2 = fn->new_temp(Type::Int);
    BinaryOpInstr binop(r, BinaryOpInstr::Op::Add, l, r2);

    EXPECT_EQ(binop.result(), r);
    auto ops = binop.operands();
    ASSERT_EQ(ops.size(), 2u);
    EXPECT_EQ(ops[0], l);
    EXPECT_EQ(ops[1], r2);
    EXPECT_FALSE(binop.is_terminator());
    EXPECT_FALSE(binop.has_side_effect());
    EXPECT_EQ(binop.kind(), InstrKind::BinaryOp);
}

TEST(UseDefTest, CallOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* r = fn->new_temp(Type::Int);
    Temp* a1 = fn->new_temp(Type::Int);
    Temp* a2 = fn->new_temp(Type::Int);
    CallInstr call(r, "foo", Type::Int, {a1, a2});

    EXPECT_EQ(call.result(), r);
    EXPECT_EQ(call.callee(), "foo");
    auto ops = call.operands();
    ASSERT_EQ(ops.size(), 2u);
    EXPECT_EQ(ops[0], a1);
    EXPECT_EQ(ops[1], a2);
    EXPECT_TRUE(call.has_side_effect());
}

TEST(UseDefTest, PhiOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* r = fn->new_temp(Type::Int);
    Temp* v1 = fn->new_temp(Type::Int);
    Temp* v2 = fn->new_temp(Type::Int);
    Label* l1 = fn->new_label("bb1");
    Label* l2 = fn->new_label("bb2");

    PhiInstr phi(r);
    phi.add_incoming(l1, v1);
    phi.add_incoming(l2, v2);

    EXPECT_EQ(phi.result(), r);
    auto ops = phi.operands();
    ASSERT_EQ(ops.size(), 2u);
    EXPECT_EQ(ops[0], v1);
    EXPECT_EQ(ops[1], v2);

    auto& incoming = phi.incoming();
    ASSERT_EQ(incoming.size(), 2u);
    EXPECT_EQ(incoming[0].predecessor, l1);
    EXPECT_EQ(incoming[0].value, v1);
    EXPECT_EQ(incoming[1].predecessor, l2);
    EXPECT_EQ(incoming[1].value, v2);
}

TEST(UseDefTest, BranchHasNoOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Label* target = fn->new_label("dest");
    BranchInstr br(target);

    EXPECT_EQ(br.result(), nullptr);
    EXPECT_TRUE(br.operands().empty());
    EXPECT_TRUE(br.is_terminator());
    EXPECT_EQ(br.kind(), InstrKind::Branch);
    EXPECT_EQ(br.target(), target);
}

TEST(UseDefTest, CondBranchOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* cond = fn->new_temp(Type::Int);
    Label* t = fn->new_label("true");
    Label* f = fn->new_label("false");
    CondBranchInstr cbr(cond, t, f);

    EXPECT_EQ(cbr.result(), nullptr);
    auto ops = cbr.operands();
    ASSERT_EQ(ops.size(), 1u);
    EXPECT_EQ(ops[0], cond);
    EXPECT_TRUE(cbr.is_terminator());
    EXPECT_EQ(cbr.kind(), InstrKind::CondBranch);
}

TEST(UseDefTest, ReturnOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* val = fn->new_temp(Type::Int);
    ReturnInstr ret(val);

    EXPECT_EQ(ret.result(), nullptr);
    EXPECT_TRUE(ret.has_value());
    auto ops = ret.operands();
    ASSERT_EQ(ops.size(), 1u);
    EXPECT_EQ(ops[0], val);
    EXPECT_TRUE(ret.is_terminator());
    EXPECT_EQ(ret.kind(), InstrKind::Return);
}

TEST(UseDefTest, VoidReturnHasNoOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    ReturnInstr ret(nullptr);

    EXPECT_FALSE(ret.has_value());
    EXPECT_TRUE(ret.operands().empty());
    EXPECT_TRUE(ret.is_terminator());
}

TEST(UseDefTest, LoadImmOperands) {
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    Temp* r = fn->new_temp(Type::Int);
    Constant* c = fn->new_constant(42);
    LoadImmInstr li(r, c);

    EXPECT_EQ(li.result(), r);
    EXPECT_EQ(li.int_value(), 42);
    auto ops = li.operands();
    ASSERT_EQ(ops.size(), 1u);
    EXPECT_EQ(ops[0], c);
}

TEST(BasicBlockTerminatorInvariantTest, InsertEntryAllocaBeforeExistingTerminator) {
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});
    BasicBlock* entry = fn->entry_block();
    LocalVar* x = fn->new_local("x", false);
    Temp* t = fn->new_temp(Type::Int);

    entry->add_instruction(std::make_unique<LoadImmInstr>(t, fn->new_constant(7)));
    entry->set_terminator(std::make_unique<ReturnInstr>(t));
    entry->insert_instruction(0, std::make_unique<AllocaInstr>(x));

    auto all = entry->all_instrs();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0]->kind(), InstrKind::Alloca);
    EXPECT_EQ(all[1]->kind(), InstrKind::LoadImm);
    EXPECT_EQ(all[2]->kind(), InstrKind::Return);
}

TEST(BasicBlockTerminatorInvariantTest, InsertInstructionAfterTerminatorRejected) {
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});
    BasicBlock* entry = fn->entry_block();
    Temp* t = fn->new_temp(Type::Int);

    entry->set_terminator(std::make_unique<ReturnInstr>(t));
    EXPECT_THROW(
        entry->insert_instruction(1, std::make_unique<LoadImmInstr>(t, fn->new_constant(1))),
        std::out_of_range);
}

TEST(BasicBlockTerminatorInvariantTest, AppendInstructionAfterTerminatorRejected) {
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});
    BasicBlock* entry = fn->entry_block();
    Temp* t = fn->new_temp(Type::Int);

    entry->set_terminator(std::make_unique<ReturnInstr>(t));
    EXPECT_THROW(
        entry->add_instruction(std::make_unique<LoadImmInstr>(t, fn->new_constant(1))),
        std::logic_error);
}

TEST(BasicBlockTerminatorInvariantTest, BodyTerminatorRejectedByOrdinaryApis) {
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});
    BasicBlock* entry = fn->entry_block();
    Temp* t = fn->new_temp(Type::Int);

    EXPECT_THROW(
        entry->add_instruction(std::make_unique<ReturnInstr>(t)),
        std::invalid_argument);
    EXPECT_THROW(
        entry->insert_instruction(0, std::make_unique<ReturnInstr>(t)),
        std::invalid_argument);
    std::vector<InstrPtr> body;
    body.push_back(std::make_unique<ReturnInstr>(t));
    EXPECT_THROW(entry->replace_body(std::move(body)), std::invalid_argument);
}

TEST(BasicBlockTerminatorInvariantTest, SetTerminatorRequiresTerminatorInstruction) {
    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});
    BasicBlock* entry = fn->entry_block();
    Temp* t = fn->new_temp(Type::Int);

    EXPECT_THROW(
        entry->set_terminator(std::make_unique<LoadImmInstr>(t, fn->new_constant(1))),
        std::invalid_argument);
}

// =============================================================================
// Test 3: IR Printer Stability
// =============================================================================

TEST(PrinterStabilityTest, SameFixturePrintsIdentically) {
    // Build a fixture manually.
    auto mod = std::make_unique<Module>();
    auto fn = std::make_unique<Function>("main", Type::Int, std::vector<LocalVar*>{});

    LocalVar* x = fn->new_local("x", false);
    Temp* t0 = fn->new_temp(Type::Int);
    Temp* t1 = fn->new_temp(Type::Int);
    Constant* c3 = fn->new_constant(3);
    Constant* c0 = fn->new_constant(0);

    BasicBlock* entry = fn->entry_block();
    entry->add_instruction(std::make_unique<AllocaInstr>(x));
    entry->add_instruction(std::make_unique<LoadImmInstr>(t0, c3));
    entry->add_instruction(std::make_unique<StoreInstr>(t0, x));
    entry->add_instruction(std::make_unique<LoadInstr>(t1, x));
    entry->set_terminator(std::make_unique<ReturnInstr>(t1));

    mod->add_function(std::move(fn));

    IRProgram program(std::move(mod));

    std::string result1 = IRPrinter::print(program);
    std::string result2 = IRPrinter::print(program);

    EXPECT_EQ(result1, result2);
    EXPECT_FALSE(result1.empty());

    // Verify the output contains key patterns.
    EXPECT_NE(result1.find("func @main"), std::string::npos);
    EXPECT_NE(result1.find("alloca"), std::string::npos);
    EXPECT_NE(result1.find("store"), std::string::npos);
    EXPECT_NE(result1.find("load"), std::string::npos);
    EXPECT_NE(result1.find("ret"), std::string::npos);
}

// =============================================================================
// Test 4: Ownership — destruction order does not cause errors.
// This test verifies that Module/Function/BasicBlock destruction works
// correctly (no double-free, use-after-free, or dangling references).
// =============================================================================

TEST(OwnershipLifetimeTest, ModuleDestructionNoErrors) {
    // Create a complex module, then let it go out of scope.
    // Sanitizer would catch any issues.
    auto mod = std::make_unique<Module>();

    auto fn = std::make_unique<Function>("f", Type::Int, std::vector<LocalVar*>{});

    // Add several blocks with various instructions.
    auto* then_label = fn->new_label("then");
    auto* else_label = fn->new_label("else");

    LocalVar* x = fn->new_local("x", false);
    Temp* t0 = fn->new_temp(Type::Int);
    Temp* t1 = fn->new_temp(Type::Int);
    Temp* t2 = fn->new_temp(Type::Int);
    Constant* c3 = fn->new_constant(3);
    Constant* c1 = fn->new_constant(1);

    BasicBlock* entry = fn->entry_block();
    entry->add_instruction(std::make_unique<AllocaInstr>(x));
    entry->add_instruction(std::make_unique<LoadImmInstr>(t0, c3));
    entry->add_instruction(std::make_unique<StoreInstr>(t0, x));
    entry->add_instruction(std::make_unique<LoadInstr>(t1, x));
    entry->set_terminator(std::make_unique<CondBranchInstr>(t1, then_label, else_label));

    auto then_bb = std::make_unique<BasicBlock>(then_label);
    then_bb->add_instruction(std::make_unique<LoadInstr>(t2, x));
    then_bb->set_terminator(std::make_unique<ReturnInstr>(t0));
    fn->add_block(std::move(then_bb));

    auto else_bb = std::make_unique<BasicBlock>(else_label);
    else_bb->set_terminator(std::make_unique<ReturnInstr>(c1));
    fn->add_block(std::move(else_bb));

    mod->add_function(std::move(fn));

    // Print (access all pointers) before destruction.
    std::string output = IRPrinter::print(*mod);
    EXPECT_FALSE(output.empty());

    // Module goes out of scope — everything destroyed.
    // No crash = test passes.
    SUCCEED();
}
