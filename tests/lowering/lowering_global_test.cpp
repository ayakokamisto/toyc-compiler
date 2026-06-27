/// Global lowering tests — global variables, constants, and runtime init.

#include "toyc/analysis/cfg.h"
#include "toyc/frontend/ast.h"
#include "toyc/frontend/lexer.h"
#include "toyc/frontend/parser.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/ir/verifier.h"
#include "toyc/lowering/ast_to_ir.h"
#include "toyc/sema/semantic_analyzer.h"
#include "toyc/support/diagnostics.h"

#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace toyc {

static std::optional<Module> compileToIR(const std::string& source, DiagnosticEngine& diag) {
  Lexer lexer(source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) return std::nullopt;
  Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) return std::nullopt;
  SemanticAnalyzer sema(diag);
  auto model = sema.analyze(*ast);
  if (diag.hasErrors()) return std::nullopt;
  ASTToIRLowering lowering(*model, diag);
  auto ir = lowering.lower(*ast);
  if (!ir.has_value()) return std::nullopt;
  rebuildCFG(*ir);
  auto verifyResult = verifyModule(*ir);
  if (!verifyResult.ok) {
    for (const auto& err : verifyResult.errors) {
      diag.error(SourceLocation{}, "IR verification: " + err);
    }
    return std::nullopt;
  }
  return ir;
}

// ── Test: global constant ─────────────────────────────────────────────────

TEST(LoweringGlobalTest, GlobalConstant) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    const int c = 2 + 3;
    int main() {
      return c * 4;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // c should be a constant global with static value 5.
  const IRGlobal* cGlobal = nullptr;
  for (const auto& g : ir->globals()) {
    if (g.name == "c") cGlobal = &g;
  }
  ASSERT_NE(cGlobal, nullptr);
  EXPECT_EQ(cGlobal->kind, GlobalKind::Constant);
  EXPECT_EQ(cGlobal->initKind, IRGlobalInitKind::Static);
  EXPECT_EQ(cGlobal->staticInitialValue, 5);

  // Using c should generate ConstInt(5), not LoadGlobal.
  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  bool hasLoadGlobalC = false;
  bool hasConst5 = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalLoad) {
        // Check if it's loading from c.
        hasLoadGlobalC = true;
      }
      if (inst->opcode == Opcode::ConstInt && inst->constValue == 5) {
        hasConst5 = true;
      }
    }
  }
  EXPECT_TRUE(hasConst5);
  EXPECT_FALSE(hasLoadGlobalC);  // Constants should not be loaded.
}

// ── Test: global variable (static) ───────────────────────────────────────

TEST(LoweringGlobalTest, GlobalVariableStatic) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    const int c = 5;
    int g = c * 4;
    int main() {
      return g + c;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // g should be a variable global with static value 20.
  const IRGlobal* gGlobal = nullptr;
  for (const auto& g : ir->globals()) {
    if (g.name == "g") gGlobal = &g;
  }
  ASSERT_NE(gGlobal, nullptr);
  EXPECT_EQ(gGlobal->kind, GlobalKind::Variable);
  EXPECT_EQ(gGlobal->initKind, IRGlobalInitKind::Static);
  EXPECT_EQ(gGlobal->staticInitialValue, 20);

  // Using g should generate LoadGlobal.
  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  bool hasLoadGlobalG = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalLoad) {
        hasLoadGlobalG = true;
      }
    }
  }
  EXPECT_TRUE(hasLoadGlobalG);
}

// ── Test: global variable (runtime) ──────────────────────────────────────

TEST(LoweringGlobalTest, GlobalVariableRuntime) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int seed() { return 7; }
    int g = seed();
    int main() {
      return g;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // g should be runtime-zero-initialized.
  const IRGlobal* gGlobal = nullptr;
  for (const auto& g : ir->globals()) {
    if (g.name == "g") gGlobal = &g;
  }
  ASSERT_NE(gGlobal, nullptr);
  EXPECT_EQ(gGlobal->kind, GlobalKind::Variable);
  EXPECT_EQ(gGlobal->initKind, IRGlobalInitKind::RuntimeZeroInitialized);

  // Should have .Ltoyc.global_init_guard.
  bool hasGuard = false;
  for (const auto& g : ir->globals()) {
    if (g.name == ".Ltoyc.global_init_guard") hasGuard = true;
  }
  EXPECT_TRUE(hasGuard);

  // Should have .Ltoyc.global_init function.
  bool hasInitFunc = false;
  for (const auto& f : ir->functions()) {
    if (f->name() == ".Ltoyc.global_init") hasInitFunc = true;
  }
  EXPECT_TRUE(hasInitFunc);

  // main should call .Ltoyc.global_init.
  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  bool hasCallInit = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::Call) {
        const auto* callee = ir->findFunction(inst->callee);
        if (callee && callee->name() == ".Ltoyc.global_init") {
          hasCallInit = true;
        }
      }
    }
  }
  EXPECT_TRUE(hasCallInit);
}

// ── Test: global assignment ──────────────────────────────────────────────

TEST(LoweringGlobalTest, GlobalAssignment) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int g = 0;
    int main() {
      g = 42;
      return g;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  bool hasStoreGlobal = false;
  bool hasLoadGlobal = false;
  for (const auto& bb : mainFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalStore) hasStoreGlobal = true;
      if (inst->opcode == Opcode::GlobalLoad) hasLoadGlobal = true;
    }
  }
  EXPECT_TRUE(hasStoreGlobal);
  EXPECT_TRUE(hasLoadGlobal);
}

// ── Test: multiple runtime globals ───────────────────────────────────────

TEST(LoweringGlobalTest, MultipleRuntimeGlobals) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int f() { return 1; }
    int g() { return 2; }
    int a = f();
    int b = g();
    int main() {
      return a + b;
    }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // Both a and b should be runtime-zero-initialized.
  int runtimeCount = 0;
  for (const auto& g : ir->globals()) {
    if (g.initKind == IRGlobalInitKind::RuntimeZeroInitialized) runtimeCount++;
  }
  EXPECT_GE(runtimeCount, 2);
}

// ── Case A: multiple runtime initializer ordering ─────────────────────────

TEST(LoweringGlobalTest, RuntimeInitializerOrdering) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int seed() { return 7; }
    int a = seed();
    int b = a + 1;
    int main() { return b; }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // a and b should both be RuntimeZeroInitialized.
  const IRGlobal* aGlobal = nullptr;
  const IRGlobal* bGlobal = nullptr;
  for (const auto& g : ir->globals()) {
    if (g.name == "a") aGlobal = &g;
    if (g.name == "b") bGlobal = &g;
  }
  ASSERT_NE(aGlobal, nullptr);
  ASSERT_NE(bGlobal, nullptr);
  EXPECT_EQ(aGlobal->initKind, IRGlobalInitKind::RuntimeZeroInitialized);
  EXPECT_EQ(bGlobal->initKind, IRGlobalInitKind::RuntimeZeroInitialized);

  // .Ltoyc.global_init should initialize a before b.
  const Function* initFunc = nullptr;
  for (const auto& f : ir->functions()) {
    if (f->name() == ".Ltoyc.global_init") initFunc = f.get();
  }
  ASSERT_NE(initFunc, nullptr);

  // Find StoreGlobal(a) and StoreGlobal(b) positions.
  int storeAPos = -1, storeBPos = -1;
  int pos = 0;
  for (const auto& bb : initFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalStore) {
        if (inst->global == aGlobal->id) storeAPos = pos;
        if (inst->global == bGlobal->id) storeBPos = pos;
      }
      ++pos;
    }
  }
  EXPECT_GE(storeAPos, 0);
  EXPECT_GE(storeBPos, 0);
  EXPECT_LT(storeAPos, storeBPos);  // a before b.

  // b's initializer should load from a.
  bool bLoadsA = false;
  for (const auto& bb : initFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalStore && inst->global == bGlobal->id) {
        // The value stored to b comes from a Binary(Add, LoadGlobal(a), const 1).
        // Check if there's a LoadGlobal(a) in this function.
        for (const auto& inst2 : bb->instructions()) {
          if (inst2->opcode == Opcode::GlobalLoad && inst2->global == aGlobal->id) {
            bLoadsA = true;
          }
        }
      }
    }
  }
  EXPECT_TRUE(bLoadsA);
}

// ── Case B: static and runtime interleaved ────────────────────────────────

TEST(LoweringGlobalTest, StaticAndRuntimeInterleaved) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int seed() { return 4; }
    const int c = 3;
    int a = seed();
    int b = c + 2;
    int d = a + b;
    int main() { return d; }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // c and b should be Static; a and d should be Runtime.
  const IRGlobal* cG = nullptr;
  const IRGlobal* aG = nullptr;
  const IRGlobal* bG = nullptr;
  const IRGlobal* dG = nullptr;
  for (const auto& g : ir->globals()) {
    if (g.name == "c") cG = &g;
    if (g.name == "a") aG = &g;
    if (g.name == "b") bG = &g;
    if (g.name == "d") dG = &g;
  }
  ASSERT_NE(cG, nullptr);
  ASSERT_NE(aG, nullptr);
  ASSERT_NE(bG, nullptr);
  ASSERT_NE(dG, nullptr);

  EXPECT_EQ(cG->initKind, IRGlobalInitKind::Static);
  EXPECT_EQ(bG->initKind, IRGlobalInitKind::Static);
  EXPECT_EQ(aG->initKind, IRGlobalInitKind::RuntimeZeroInitialized);
  EXPECT_EQ(dG->initKind, IRGlobalInitKind::RuntimeZeroInitialized);

  // Helper should only initialize a and d.
  const Function* initFunc = nullptr;
  for (const auto& f : ir->functions()) {
    if (f->name() == ".Ltoyc.global_init") initFunc = f.get();
  }
  ASSERT_NE(initFunc, nullptr);

  bool hasStoreA = false, hasStoreD = false;
  bool hasStoreB = false, hasStoreC = false;
  for (const auto& bb : initFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalStore) {
        if (inst->global == aG->id) hasStoreA = true;
        if (inst->global == dG->id) hasStoreD = true;
        if (inst->global == bG->id) hasStoreB = true;
        if (inst->global == cG->id) hasStoreC = true;
      }
    }
  }
  EXPECT_TRUE(hasStoreA);
  EXPECT_TRUE(hasStoreD);
  EXPECT_FALSE(hasStoreB);
  EXPECT_FALSE(hasStoreC);

  // a should be initialized before d.
  int storeAPos = -1, storeDPos = -1;
  int pos = 0;
  for (const auto& bb : initFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::GlobalStore) {
        if (inst->global == aG->id) storeAPos = pos;
        if (inst->global == dG->id) storeDPos = pos;
      }
      ++pos;
    }
  }
  EXPECT_LT(storeAPos, storeDPos);
}

// ── Case C: runtime init depends on pre-declared function ─────────────────

TEST(LoweringGlobalTest, RuntimeInitCallsPredeclaredFunction) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int twice(int x) { return x * 2; }
    int g = twice(5);
    int main() { return g; }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // .Ltoyc.global_init should call twice.
  const Function* initFunc = nullptr;
  const Function* twiceFunc = nullptr;
  for (const auto& f : ir->functions()) {
    if (f->name() == ".Ltoyc.global_init") initFunc = f.get();
    if (f->name() == "twice") twiceFunc = f.get();
  }
  ASSERT_NE(initFunc, nullptr);
  ASSERT_NE(twiceFunc, nullptr);

  bool callsTwice = false;
  for (const auto& bb : initFunc->blocks()) {
    for (const auto& inst : bb->instructions()) {
      if (inst->opcode == Opcode::Call && inst->callee == twiceFunc->id()) {
        callsTwice = true;
      }
    }
  }
  EXPECT_TRUE(callsTwice);
}

// ── Test: no runtime globals means no init helper ─────────────────────────

TEST(LoweringGlobalTest, NoRuntimeGlobalsNoHelper) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    const int c = 5;
    int main() { return c; }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  // No .Ltoyc.global_init function should exist.
  bool hasInitFunc = false;
  for (const auto& f : ir->functions()) {
    if (f->name() == ".Ltoyc.global_init") hasInitFunc = true;
  }
  EXPECT_FALSE(hasInitFunc);

  // No guard global should exist.
  bool hasGuard = false;
  for (const auto& g : ir->globals()) {
    if (g.name == ".Ltoyc.global_init_guard") hasGuard = true;
  }
  EXPECT_FALSE(hasGuard);
}

// ── Test: main entry first instruction is call global init ────────────────

TEST(LoweringGlobalTest, MainEntryFirstInstIsCallInit) {
  DiagnosticEngine diag;
  auto ir = compileToIR(R"(
    int seed() { return 7; }
    int g = seed();
    int main() { return g; }
  )", diag);
  ASSERT_TRUE(ir.has_value());

  auto* mainFunc = ir->findFunctionByName("main");
  ASSERT_NE(mainFunc, nullptr);
  auto* entry = mainFunc->entryBlock();
  ASSERT_NE(entry, nullptr);
  ASSERT_FALSE(entry->instructions().empty());

  // First instruction should be call .Ltoyc.global_init.
  const auto& firstInst = *entry->instructions()[0];
  EXPECT_EQ(firstInst.opcode, Opcode::Call);
  const auto* callee = ir->findFunction(firstInst.callee);
  ASSERT_NE(callee, nullptr);
  EXPECT_EQ(callee->name(), ".Ltoyc.global_init");
}

} // namespace toyc
