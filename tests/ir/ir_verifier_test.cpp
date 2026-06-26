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

} // namespace toyc
