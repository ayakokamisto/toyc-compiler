/// IR Printer tests — stable dump output.

#include "toyc/analysis/cfg.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/function.h"
#include "toyc/ir/module.h"
#include "toyc/ir/printer.h"
#include "toyc/support/ids.h"

#include <gtest/gtest.h>
#include <sstream>

namespace toyc {

TEST(PrinterTest, MinimalMain) {
  Module mod;
  auto* func = mod.createFunction("main", I32Type);
  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  auto val = builder.emitConstInt(0);
  builder.emitReturn(val);

  rebuildCFG(mod);

  std::ostringstream out;
  dumpIR(mod, out);
  std::string ir = out.str();

  EXPECT_NE(ir.find("IRModule"), std::string::npos);
  EXPECT_NE(ir.find("func @main"), std::string::npos);
  EXPECT_NE(ir.find("entry:"), std::string::npos);
  EXPECT_NE(ir.find("const 0"), std::string::npos);
  EXPECT_NE(ir.find("ret"), std::string::npos);
}

TEST(PrinterTest, FunctionWithSlots) {
  Module mod;
  auto* func = mod.createFunction("f", I32Type);
  func->addParam(SymbolId(0));
  func->createSlot(SlotKind::LocalVariable, SymbolId(1));

  IRBuilder builder;
  builder.setFunction(func);
  auto entry = builder.createBlock("entry");
  builder.setInsertBlock(entry);
  auto val = builder.emitConstInt(0);
  builder.emitReturn(val);

  rebuildCFG(mod);

  std::ostringstream out;
  dumpIR(mod, out);
  std::string ir = out.str();

  EXPECT_NE(ir.find("Slots"), std::string::npos);
  EXPECT_NE(ir.find("parameter"), std::string::npos);
  EXPECT_NE(ir.find("local"), std::string::npos);
}

TEST(PrinterTest, GlobalDefinitions) {
  Module mod;
  IRGlobal g;
  g.name = "x";
  g.kind = GlobalKind::Variable;
  g.initKind = IRGlobalInitKind::Static;
  g.staticInitialValue = 5;
  mod.createGlobal(std::move(g));

  IRGlobal c;
  c.name = "N";
  c.kind = GlobalKind::Constant;
  c.initKind = IRGlobalInitKind::Static;
  c.staticInitialValue = 42;
  mod.createGlobal(std::move(c));

  std::ostringstream out;
  dumpIR(mod, out);
  std::string ir = out.str();

  EXPECT_NE(ir.find("Globals"), std::string::npos);
  EXPECT_NE(ir.find("@x kind=variable"), std::string::npos);
  EXPECT_NE(ir.find("@N kind=constant"), std::string::npos);
}

TEST(PrinterTest, DeterministicOutput) {
  // Same module should produce same output.
  auto buildModule = []() {
    Module mod;
    auto* func = mod.createFunction("main", I32Type);
    IRBuilder builder;
    builder.setFunction(func);
    auto entry = builder.createBlock("entry");
    builder.setInsertBlock(entry);
    auto val = builder.emitConstInt(42);
    builder.emitReturn(val);
    rebuildCFG(mod);
    return mod;
  };

  std::ostringstream out1, out2;
  dumpIR(buildModule(), out1);
  dumpIR(buildModule(), out2);
  EXPECT_EQ(out1.str(), out2.str());
}

TEST(PrinterTest, ControlFlowDump) {
  Module mod;
  auto* func = mod.createFunction("test", VoidIRType);
  IRBuilder builder;
  builder.setFunction(func);

  auto entry = builder.createBlock("entry");
  auto thenB = builder.createBlock("if.then");
  auto mergeB = builder.createBlock("if.merge");

  builder.setInsertBlock(entry);
  auto cond = builder.emitConstInt(1);
  builder.emitCondBranch(cond, thenB, mergeB);

  builder.setInsertBlock(thenB);
  builder.emitBranch(mergeB);

  builder.setInsertBlock(mergeB);
  builder.emitReturn(std::nullopt);

  rebuildCFG(mod);

  std::ostringstream out;
  dumpIR(mod, out);
  std::string ir = out.str();

  EXPECT_NE(ir.find("condbr"), std::string::npos);
  EXPECT_NE(ir.find("if.then:"), std::string::npos);
  EXPECT_NE(ir.find("if.merge:"), std::string::npos);
}

} // namespace toyc
