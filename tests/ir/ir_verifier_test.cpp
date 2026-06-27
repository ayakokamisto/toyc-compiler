/// IR Verifier tests — structural well-formedness checks.

#include "toyc/analysis/cfg.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/verifier.h"

#include <gtest/gtest.h>

namespace toyc {

// Helper to build a minimal valid function.
static Function* buildValidMain(Module& mod) {
  auto* func = mod.createFunction("main", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  auto val = builder.emitConstInt(0);
  builder.emitReturn(val);
  return func;
}

TEST(VerifierTest, ValidModulePasses) {
  Module mod;
  buildValidMain(mod);
  rebuildCFG(mod);
  auto result = verifyModule(mod);
  EXPECT_TRUE(result.ok) << result.errors[0];
}

TEST(VerifierTest, DetectsMissingTerminator) {
  Module mod;
  auto* func = mod.createFunction("main", I32Type);
  func->createBlock("entry");

  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("terminator") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsInvalidBranchTarget) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  // Branch to a non-existent block.
  builder.emitBranch(BlockId(99));

  rebuildCFG(*func);
  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Invalid branch target") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsUndefinedValue) {
  Module mod;
  auto* func = mod.createFunction("test", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  // Return a value that doesn't belong to this function.
  builder.emitReturn(ValueId(999));

  rebuildCFG(*func);
  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Undefined") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsWrongReturnType) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  auto val = builder.emitConstInt(0);
  // Return a value from a void function.
  builder.emitReturn(val);

  rebuildCFG(*func);
  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Void function") != std::string::npos ||
        e.find("return value") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsInvalidSlotId) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  // Load from a non-existent slot.
  builder.emitLoadSlot(SlotId(99));
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);
  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Invalid SlotId") != std::string::npos ||
        e.find("SlotId") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsInvalidGlobalId) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  // Load from a non-existent global.
  builder.emitLoadGlobal(GlobalId(99));
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);
  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Invalid GlobalId") != std::string::npos ||
        e.find("GlobalId") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsCallArgumentCountMismatch) {
  Module mod;
  auto* callee = mod.createFunction("add", I32Type);
  callee->addParam(SymbolId(0));
  callee->addParam(SymbolId(1));

  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);

  // Call with wrong number of arguments.
  auto arg = builder.emitConstInt(1);
  std::vector<ValueId> args = {arg};  // Should be 2.
  builder.emitCall(callee->id(), args);
  builder.emitReturn(std::nullopt);

  rebuildCFG(mod);
  auto result = verifyModule(mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("argument count") != std::string::npos ||
        e.find("mismatch") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsVoidCallWithResult) {
  Module mod;
  auto* callee = mod.createFunction("print", VoidIRType);

  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);

  auto arg = builder.emitConstInt(1);
  std::vector<ValueId> args = {arg};
  auto result_val = builder.emitCall(callee->id(), args);
  EXPECT_TRUE(result_val.has_value());  // Builder conservatively creates result.
  builder.emitReturn(std::nullopt);

  rebuildCFG(mod);
  auto verifyResult = verifyModule(mod);
  EXPECT_FALSE(verifyResult.ok);
  bool found = false;
  for (const auto& e : verifyResult.errors) {
    if (e.find("void") != std::string::npos && e.find("result") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsDuplicateBlockLabel) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  func->createBlock("entry");
  func->createBlock("entry");  // Duplicate label.

  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Duplicate block label") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsMissingReturnInIntFunction) {
  Module mod;
  auto* func = mod.createFunction("test", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  // Return from an int function without a value.
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);
  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("missing return value") != std::string::npos ||
        e.find("Int function") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

// ── Cross-function isolation tests ─────────────────────────────────────────

TEST(VerifierTest, DetectsCrossFunctionValueId) {
  Module mod;
  // Function A: create many values so valA has a high ValueId.
  auto* funcA = mod.createFunction("a", I32Type);
  IRBuilder builderA;
  builderA.setFunction(funcA);
  auto entryA = builderA.createBlock("entry");
  builderA.setInsertBlock(entryA);
  for (int i = 0; i < 10; ++i) builderA.emitConstInt(i);
  auto valA = builderA.emitConstInt(42);  // ValueId(10) in funcA.
  builderA.emitReturn(valA);

  // Function B has zero values — only a void return.
  auto* funcB = mod.createFunction("b", VoidIRType);
  IRBuilder builderB;
  builderB.setFunction(funcB);
  builderB.createBlock("entry");
  builderB.setInsertBlock(funcB->entryBlock()->id());
  // Manually construct an Inst that references valA (ValueId(10) from funcA).
  // funcB has no values, so ValueId(10) is undefined in funcB.
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::Unary;
  inst->resultType = I32Type;
  inst->result = funcB->createInstValue();  // ValueId(0) in funcB.
  inst->unaryOp = UnaryOpcode::Negate;
  inst->unaryOperand = valA;  // Cross-function: ValueId(10) from funcA.
  funcB->entryBlock()->appendInst(std::move(inst));
  builderB.emitReturn(std::nullopt);

  rebuildCFG(mod);
  auto result = verifyModule(mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Undefined") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsCrossFunctionSlotId) {
  Module mod;
  // Function A creates a slot.
  auto* funcA = mod.createFunction("a", VoidIRType);
  SlotId slotA = funcA->createSlot(SlotKind::LocalVariable);

  // Function B tries to load from A's slot.
  auto* funcB = mod.createFunction("b", VoidIRType);
  IRBuilder builderB;
  builderB.setFunction(funcB);
  auto entryB = builderB.createBlock("entry");
  builderB.setInsertBlock(entryB);
  builderB.emitLoadSlot(slotA);  // Cross-function slot reference.
  builderB.emitReturn(std::nullopt);

  rebuildCFG(mod);
  auto result = verifyModule(mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Invalid SlotId") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsCrossFunctionBlockId) {
  Module mod;
  // Function A has blocks 0 and 1.
  auto* funcA = mod.createFunction("a", VoidIRType);
  IRBuilder builderA;
  builderA.setFunction(funcA);
  builderA.createBlock("target");
  auto entryA = builderA.createBlock("entry");
  builderA.setInsertBlock(entryA);
  builderA.emitReturn(std::nullopt);

  // Function B has only one block (entry = BlockId(0)).
  // Use funcA's blockA (BlockId(0)) which coincidentally matches funcB's entry.
  // To avoid collision, use funcA's second block (BlockId(1)) which doesn't exist in funcB.
  auto* funcB = mod.createFunction("b", VoidIRType);
  IRBuilder builderB;
  builderB.setFunction(funcB);
  auto entryB = builderB.createBlock("entry");
  builderB.setInsertBlock(entryB);
  // blockA is BlockId(0), which matches funcB's entry. Use a non-existent BlockId instead.
  // Actually, to properly test cross-function, we need a BlockId that doesn't exist in funcB.
  // BlockId(1) exists in funcA but not in funcB (funcB only has BlockId(0)).
  builderB.emitBranch(BlockId(1));  // Cross-function: BlockId(1) belongs to funcA, not funcB.

  rebuildCFG(mod);
  auto result = verifyModule(mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Invalid branch target") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

TEST(VerifierTest, DetectsIllegalOpcode) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);

  // Manually construct an Inst with an illegal opcode value.
  auto inst = std::make_unique<Inst>();
  inst->opcode = static_cast<Opcode>(255);  // Illegal opcode.
  inst->resultType = VoidIRType;
  auto* bb = func->entryBlock();
  const_cast<std::vector<std::unique_ptr<Inst>>&>(bb->instructions()).push_back(std::move(inst));
  builder.emitReturn(std::nullopt);

  rebuildCFG(*func);
  auto result = verifyFunction(*func, mod);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Illegal opcode") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

// ── Module move safety tests ─────────────────────────────────────────────

TEST(ModuleMoveTest, FunctionCanAllocateAfterModuleMoveConstruct) {
  Module original;
  auto* func = original.createFunction("f", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  auto val = builder.emitConstInt(42);
  builder.emitReturn(val);

  // Record pre-move block ID.
  BlockId preMoveBlockId = entry;

  // Move construct.
  Module moved = std::move(original);

  auto* movedFunc = moved.findFunctionByName("f");
  ASSERT_NE(movedFunc, nullptr);

  // After move, Function must still be able to allocate IDs via the new Module.
  SlotId sid = movedFunc->createSlot(SlotKind::LocalVariable);
  EXPECT_TRUE(sid.valid());

  // Create a new block with terminator to keep verifier happy.
  auto* afterMoveBlock = movedFunc->createBlock("after_move");
  ASSERT_NE(afterMoveBlock, nullptr);
  BlockId bid = afterMoveBlock->id();
  EXPECT_TRUE(bid.valid());
  EXPECT_NE(bid.value, preMoveBlockId.value);

  ValueId vid = movedFunc->createInstValue();
  EXPECT_TRUE(vid.valid());

  // The new block needs a terminator — use the builder on the moved module.
  IRBuilder movedBuilder;
  movedBuilder.setFunction(movedFunc);
  movedBuilder.setInsertBlock(bid);
  auto ret = movedBuilder.emitConstInt(0);
  movedBuilder.emitReturn(ret);

  rebuildCFG(moved);
  auto result = verifyModule(moved);
  EXPECT_TRUE(result.ok) << result.errors[0];
}

TEST(ModuleMoveTest, FunctionCanAllocateAfterMoveAssignment) {
  Module original;
  auto* func = original.createFunction("f", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  builder.emitReturn(std::nullopt);

  Module target;
  target = std::move(original);

  auto* movedFunc = target.findFunctionByName("f");
  ASSERT_NE(movedFunc, nullptr);

  SlotId sid = movedFunc->createSlot(SlotKind::Temporary);
  EXPECT_TRUE(sid.valid());

  ValueId vid = movedFunc->createInstValue();
  EXPECT_TRUE(vid.valid());

  rebuildCFG(target);
  auto result = verifyModule(target);
  EXPECT_TRUE(result.ok) << result.errors[0];
}

TEST(ModuleMoveTest, FunctionCanAllocateAfterOptionalReturn) {
  auto buildModule = []() -> std::optional<Module> {
    Module module;
    auto* func = module.createFunction("main", I32Type);
    IRBuilder builder;
    builder.setFunction(func);
    auto entry = builder.createBlock("entry");
    builder.setInsertBlock(entry);
    auto val = builder.emitConstInt(0);
    builder.emitReturn(val);
    return module;
  };

  auto module = buildModule();
  ASSERT_TRUE(module.has_value());

  auto* func = module->findFunctionByName("main");
  ASSERT_NE(func, nullptr);

  // After optional return, Function must still allocate IDs correctly.
  SlotId sid = func->createSlot(SlotKind::LocalVariable, std::nullopt, "x");
  EXPECT_TRUE(sid.valid());

  ValueId vid = func->createInstValue();
  EXPECT_TRUE(vid.valid());

  // No new block created — the original block already has a terminator.
  rebuildCFG(*module);
  auto result = verifyModule(*module);
  EXPECT_TRUE(result.ok) << result.errors[0];
}

TEST(ModuleMoveTest, FunctionCanAllocateAfterVectorRelocation) {
  std::vector<Module> modules;
  modules.reserve(1);  // Force relocation on second push.

  {
    Module first;
    auto* f1 = first.createFunction("a", I32Type);
    IRBuilder b1;
    b1.setFunction(f1);
    auto e1 = b1.createBlock("entry");
    b1.setInsertBlock(e1);
    auto v1 = b1.emitConstInt(1);
    b1.emitReturn(v1);
    modules.push_back(std::move(first));
  }

  {
    Module second;
    auto* f2 = second.createFunction("b", I32Type);
    IRBuilder b2;
    b2.setFunction(f2);
    auto e2 = b2.createBlock("entry");
    b2.setInsertBlock(e2);
    auto v2 = b2.emitConstInt(2);
    b2.emitReturn(v2);
    modules.push_back(std::move(second));
  }

  // After vector relocation, both modules' Functions must still work.
  auto* funcA = modules[0].findFunctionByName("a");
  ASSERT_NE(funcA, nullptr);
  SlotId sidA = funcA->createSlot(SlotKind::LocalVariable);
  EXPECT_TRUE(sidA.valid());

  auto* funcB = modules[1].findFunctionByName("b");
  ASSERT_NE(funcB, nullptr);
  SlotId sidB = funcB->createSlot(SlotKind::LocalVariable);
  EXPECT_TRUE(sidB.valid());

  // Each module has its own ID space — IDs can be equal across modules.
  // The key test is that allocation succeeds and the verifier passes.
  EXPECT_TRUE(verifyModule(modules[0]).ok);
  EXPECT_TRUE(verifyModule(modules[1]).ok);
}

TEST(ModuleMoveTest, IdCounterContinuityAfterMove) {
  Module original;
  auto* func = original.createFunction("f", VoidIRType);
  func->createBlock("b0");
  func->createBlock("b1");
  func->createSlot(SlotKind::LocalVariable);
  func->createInstValue();

  // Record the IDs used before move.
  BlockId lastBlock = func->blocks().back()->id();
  SlotId lastSlot = func->slots().back().id;
  ValueId lastValue = func->values().back().id;

  Module moved = std::move(original);
  auto* movedFunc = moved.findFunctionByName("f");
  ASSERT_NE(movedFunc, nullptr);

  // New IDs must be strictly greater than pre-move IDs.
  auto* newBlockBB = movedFunc->createBlock("new_block");
  ASSERT_NE(newBlockBB, nullptr);
  EXPECT_GT(newBlockBB->id().value, lastBlock.value);

  SlotId newSlot = movedFunc->createSlot(SlotKind::Temporary);
  EXPECT_GT(newSlot.value, lastSlot.value);

  ValueId newValue = movedFunc->createInstValue();
  EXPECT_GT(newValue.value, lastValue.value);
}

TEST(ModuleMoveTest, CrossFunctionRejectionAfterMove) {
  Module original;
  auto* funcA = original.createFunction("a", I32Type);
  IRBuilder builderA;
  builderA.setFunction(funcA);
  auto entryA = builderA.createBlock("entry");
  builderA.setInsertBlock(entryA);
  auto valA = builderA.emitConstInt(42);
  builderA.emitReturn(valA);

  Module moved = std::move(original);

  // Create a new function in the moved module.
  auto* funcB = moved.createFunction("b", VoidIRType);
  IRBuilder builderB;
  builderB.setFunction(funcB);
  auto entryB = builderB.createBlock("entry");
  builderB.setInsertBlock(entryB);

  // Try to use funcA's value in funcB — must be rejected.
  auto inst = std::make_unique<Inst>();
  inst->opcode = Opcode::Unary;
  inst->resultType = I32Type;
  inst->result = funcB->createInstValue();
  inst->unaryOp = UnaryOpcode::Negate;
  inst->unaryOperand = valA;  // Cross-function reference.
  funcB->entryBlock()->appendInst(std::move(inst));
  builderB.emitReturn(std::nullopt);

  rebuildCFG(moved);
  auto result = verifyModule(moved);
  EXPECT_FALSE(result.ok);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.find("Undefined") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
}

} // namespace toyc
