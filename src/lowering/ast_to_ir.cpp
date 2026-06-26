/// AST to IR Lowering — converts AST + SemanticModel to Canonical Slot IR.

#include "toyc/lowering/ast_to_ir.h"
#include "toyc/analysis/cfg.h"
#include "toyc/frontend/ast.h"
#include "toyc/ir/basic_block.h"
#include "toyc/ir/builder.h"
#include "toyc/ir/function.h"
#include "toyc/ir/instruction.h"
#include "toyc/ir/module.h"
#include "toyc/ir/opcode.h"
#include "toyc/sema/constant_evaluator.h"
#include "toyc/sema/semantic_model.h"
#include "toyc/sema/symbol.h"
#include "toyc/support/diagnostics.h"

#include <cassert>
#include <map>
#include <sstream>
#include <string>

namespace toyc {

// ── Lowering context ───────────────────────────────────────────────────────

struct LoweringContext {
  IRBuilder builder;
  Module& module;
  const SemanticModel& sema;
  DiagnosticEngine& diag;

  // Scope-local slot map: SymbolId → SlotId (innermost scope wins).
  std::vector<std::map<SymbolId, SlotId>> scopeStack;

  // Loop context for break/continue.
  struct LoopContext {
    BlockId breakTarget;
    BlockId continueTarget;
  };
  std::vector<LoopContext> loopStack;

  // Block numbering for stable labels.
  int blockCounter = 0;

  // Current function being lowered.
  Function* currentFunc = nullptr;

  // Track symbol → global id mapping for globals.
  std::map<SymbolId, GlobalId> symbolToGlobal;

  LoweringContext(Module& mod, const SemanticModel& sm, DiagnosticEngine& d)
      : module(mod), sema(sm), diag(d) {}

  void pushScope() { scopeStack.emplace_back(); }
  void popScope() { scopeStack.pop_back(); }

  void bindSymbol(SymbolId sym, SlotId slot) {
    if (!scopeStack.empty()) {
      scopeStack.back()[sym] = slot;
    }
  }

  std::optional<SlotId> lookupSlot(SymbolId sym) const {
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
      auto found = it->find(sym);
      if (found != it->end()) return found->second;
    }
    return std::nullopt;
  }

  std::string nextBlockLabel(const std::string& prefix) {
    return prefix + "." + std::to_string(blockCounter++);
  }

  void emitError(const std::string& msg) {
    diag.error(SourceLocation{}, msg);
  }
};

// ── Forward declarations ───────────────────────────────────────────────────

static ValueId emitValue(LoweringContext& ctx, const Expr& expr);
static void emitCond(LoweringContext& ctx, const Expr& expr, BlockId trueBlock, BlockId falseBlock);

// ── Expression lowering ────────────────────────────────────────────────────

/// Emit a logical expression in value context (returns 0 or 1 via temp slot).
static ValueId emitLogicalValue(LoweringContext& ctx, const Expr& expr) {
  auto* func = ctx.currentFunc;

  // Create temporary slot for the boolean result.
  SlotId tempSlot = func->createSlot(SlotKind::Temporary);

  BlockId trueBlock = ctx.builder.createBlock(ctx.nextBlockLabel("logic.true"));
  BlockId falseBlock = ctx.builder.createBlock(ctx.nextBlockLabel("logic.false"));
  BlockId mergeBlock = ctx.builder.createBlock(ctx.nextBlockLabel("logic.merge"));

  // Emit condition.
  emitCond(ctx, expr, trueBlock, falseBlock);

  // true block: store 1.
  ctx.builder.setInsertBlock(trueBlock);
  ValueId one = ctx.builder.emitConstInt(1);
  ctx.builder.emitStoreSlot(tempSlot, one);
  ctx.builder.emitBranch(mergeBlock);

  // false block: store 0.
  ctx.builder.setInsertBlock(falseBlock);
  ValueId zero = ctx.builder.emitConstInt(0);
  ctx.builder.emitStoreSlot(tempSlot, zero);
  ctx.builder.emitBranch(mergeBlock);

  // merge block: load temp.
  ctx.builder.setInsertBlock(mergeBlock);
  return ctx.builder.emitLoadSlot(tempSlot);
}

/// Emit a call expression. Returns nullopt for void calls.
static std::optional<ValueId> emitCallExpr(LoweringContext& ctx, const CallExpr& call) {
  auto calleeSym = ctx.sema.resolvedSymbol(call);
  if (!calleeSym.has_value()) {
    ctx.emitError("Lowering: unresolved function '" + call.calleeName() + "'");
    return ctx.builder.emitConstInt(0);
  }

  const auto& calleeSymbol = ctx.sema.symbol(*calleeSym);
  Function* callee = nullptr;
  for (auto& f : ctx.module.functions()) {
    if (f->name() == calleeSymbol.name) {
      callee = f.get();
      break;
    }
  }
  if (!callee) {
    ctx.emitError("Lowering: function '" + calleeSymbol.name + "' not found in module");
    return ctx.builder.emitConstInt(0);
  }

  std::vector<ValueId> args;
  for (const auto& arg : call.arguments()) {
    args.push_back(emitValue(ctx, *arg));
  }

  bool calleeReturnsInt = callee->returnType().isI32();
  return ctx.builder.emitCall(callee->id(), args, calleeReturnsInt);
}

/// Parse an integer literal from its raw text.
static int32_t parseIntLiteral(const std::string& raw) {
  auto mag = parseUnsignedMagnitude(raw);
  if (!mag.has_value()) return 0;
  // Check for overflow.
  if (*mag > static_cast<uint64_t>(INT32_MAX)) return 0;
  return static_cast<int32_t>(*mag);
}

/// Emit an expression and return the resulting ValueId.
static ValueId emitValue(LoweringContext& ctx, const Expr& expr) {
  // Integer literal.
  if (auto* intLit = dynamic_cast<const IntegerLiteralExpr*>(&expr)) {
    int32_t value = parseIntLiteral(intLit->rawValue());
    return ctx.builder.emitConstInt(value);
  }

  // Identifier.
  if (auto* ident = dynamic_cast<const IdentifierExpr*>(&expr)) {
    auto sym = ctx.sema.resolvedSymbol(expr);
    if (!sym.has_value()) {
      ctx.emitError("Lowering: unresolved identifier '" + ident->name() + "'");
      return ctx.builder.emitConstInt(0);
    }

    const auto& symbol = ctx.sema.symbol(*sym);

    // Constant — materialize directly.
    if (symbol.isConstant()) {
      auto exprInfo = ctx.sema.exprInfo(expr);
      if (exprInfo && exprInfo->constantValue.has_value()) {
        return ctx.builder.emitConstInt(*exprInfo->constantValue);
      }
      // Fall back to symbol's constant value.
      if (symbol.constantValue.has_value()) {
        return ctx.builder.emitConstInt(*symbol.constantValue);
      }
      ctx.emitError("Lowering: constant '" + ident->name() + "' has no value");
      return ctx.builder.emitConstInt(0);
    }

    // Local variable or parameter — load from slot.
    if (symbol.isVariable() || symbol.kind == SymbolKind::Parameter) {
      auto slot = ctx.lookupSlot(*sym);
      if (slot.has_value()) {
        return ctx.builder.emitLoadSlot(*slot);
      }
      // Check if it's a global.
      auto git = ctx.symbolToGlobal.find(*sym);
      if (git != ctx.symbolToGlobal.end()) {
        return ctx.builder.emitLoadGlobal(git->second);
      }
      ctx.emitError("Lowering: variable '" + ident->name() + "' has no slot or global");
      return ctx.builder.emitConstInt(0);
    }

    // Global variable — load from global.
    if (symbol.kind == SymbolKind::GlobalVariable) {
      auto git = ctx.symbolToGlobal.find(*sym);
      if (git != ctx.symbolToGlobal.end()) {
        return ctx.builder.emitLoadGlobal(git->second);
      }
      ctx.emitError("Lowering: global '" + ident->name() + "' not found in module");
      return ctx.builder.emitConstInt(0);
    }

    // Function name in value context — error.
    if (symbol.kind == SymbolKind::Function) {
      ctx.emitError("Lowering: function name '" + ident->name() + "' used as value");
      return ctx.builder.emitConstInt(0);
    }

    ctx.emitError("Lowering: unexpected symbol kind for '" + ident->name() + "'");
    return ctx.builder.emitConstInt(0);
  }

  // Unary expression.
  if (auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
    ValueId operand = emitValue(ctx, *unary->operand());

    switch (unary->op()) {
      case UnaryOperator::Plus:
        return operand;  // +x is identity.
      case UnaryOperator::Minus:
        return ctx.builder.emitUnary(UnaryOpcode::Negate, operand);
      case UnaryOperator::LogicalNot:
        return ctx.builder.emitUnary(UnaryOpcode::LogicalNot, operand);
    }
    ctx.emitError("Lowering: unknown unary operator");
    return ctx.builder.emitConstInt(0);
  }

  // Binary expression.
  if (auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
    // Logical AND / OR — must use short-circuit in value context.
    if (binary->op() == BinaryOperator::LogicalAnd || binary->op() == BinaryOperator::LogicalOr) {
      return emitLogicalValue(ctx, expr);
    }

    ValueId lhs = emitValue(ctx, *binary->lhs());
    ValueId rhs = emitValue(ctx, *binary->rhs());

    switch (binary->op()) {
      case BinaryOperator::Add:      return ctx.builder.emitBinary(BinaryOpcode::Add, lhs, rhs);
      case BinaryOperator::Subtract: return ctx.builder.emitBinary(BinaryOpcode::Subtract, lhs, rhs);
      case BinaryOperator::Multiply: return ctx.builder.emitBinary(BinaryOpcode::Multiply, lhs, rhs);
      case BinaryOperator::Divide:   return ctx.builder.emitBinary(BinaryOpcode::Divide, lhs, rhs);
      case BinaryOperator::Modulo:   return ctx.builder.emitBinary(BinaryOpcode::Modulo, lhs, rhs);
      case BinaryOperator::Equal:        return ctx.builder.emitCompare(ComparePredicate::Equal, lhs, rhs);
      case BinaryOperator::NotEqual:     return ctx.builder.emitCompare(ComparePredicate::NotEqual, lhs, rhs);
      case BinaryOperator::Less:         return ctx.builder.emitCompare(ComparePredicate::Less, lhs, rhs);
      case BinaryOperator::LessEqual:    return ctx.builder.emitCompare(ComparePredicate::LessEqual, lhs, rhs);
      case BinaryOperator::Greater:      return ctx.builder.emitCompare(ComparePredicate::Greater, lhs, rhs);
      case BinaryOperator::GreaterEqual: return ctx.builder.emitCompare(ComparePredicate::GreaterEqual, lhs, rhs);
      default:
        ctx.emitError("Lowering: unknown binary operator");
        return ctx.builder.emitConstInt(0);
    }
  }

  // Call expression.
  if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
    auto result = emitCallExpr(ctx, *call);
    if (result.has_value()) {
      return *result;
    }
    // Void call in value context.
    ctx.emitError("Lowering: void function '" + call->calleeName() + "' used as value");
    return ctx.builder.emitConstInt(0);
  }

  ctx.emitError("Lowering: unsupported expression type");
  return ctx.builder.emitConstInt(0);
}

// ── Condition lowering (for control flow) ──────────────────────────────────

static void emitCond(LoweringContext& ctx, const Expr& expr, BlockId trueBlock, BlockId falseBlock) {
  // Logical AND: A && B → if A then test B else false.
  if (auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
    if (binary->op() == BinaryOperator::LogicalAnd) {
      BlockId rhsBlock = ctx.builder.createBlock(ctx.nextBlockLabel("logic.rhs"));
      emitCond(ctx, *binary->lhs(), rhsBlock, falseBlock);
      ctx.builder.setInsertBlock(rhsBlock);
      emitCond(ctx, *binary->rhs(), trueBlock, falseBlock);
      return;
    }

    // Logical OR: A || B → if A then true else test B.
    if (binary->op() == BinaryOperator::LogicalOr) {
      BlockId rhsBlock = ctx.builder.createBlock(ctx.nextBlockLabel("logic.rhs"));
      emitCond(ctx, *binary->lhs(), trueBlock, rhsBlock);
      ctx.builder.setInsertBlock(rhsBlock);
      emitCond(ctx, *binary->rhs(), trueBlock, falseBlock);
      return;
    }
  }

  // Logical NOT: !A → if A then false else true.
  if (auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
    if (unary->op() == UnaryOperator::LogicalNot) {
      emitCond(ctx, *unary->operand(), falseBlock, trueBlock);
      return;
    }
  }

  // Default: evaluate to value, then condbr.
  ValueId cond = emitValue(ctx, expr);
  ctx.builder.emitCondBranch(cond, trueBlock, falseBlock);
}

// ── Statement lowering ─────────────────────────────────────────────────────

static void lowerStmt(LoweringContext& ctx, const Stmt& stmt);

static void lowerBlockStmt(LoweringContext& ctx, const BlockStmt& block) {
  ctx.pushScope();
  for (const auto& stmt : block.statements()) {
    auto* bb = ctx.builder.insertBlock();
    if (bb && bb->hasTerminator()) break;
    lowerStmt(ctx, *stmt);
  }
  ctx.popScope();
}

static void lowerDeclStmt(LoweringContext& ctx, const DeclStmt& declStmt) {
  const Decl* decl = declStmt.declaration();

  // Variable declaration.
  if (auto* varDecl = dynamic_cast<const VarDecl*>(decl)) {
    auto sym = ctx.sema.resolvedSymbol(*decl);
    SlotId slot = ctx.currentFunc->createSlot(SlotKind::LocalVariable, sym);
    if (sym.has_value()) {
      ctx.bindSymbol(*sym, slot);
    }

    ValueId initVal = emitValue(ctx, *varDecl->initializer());
    ctx.builder.emitStoreSlot(slot, initVal);
    return;
  }

  // Constant declaration — no slot needed.
  if (auto* constDecl = dynamic_cast<const ConstDecl*>(decl)) {
    // Constants are directly materialized from SemanticModel.
    return;
  }
}

static void lowerAssignStmt(LoweringContext& ctx, const AssignStmt& assign) {
  ValueId rhs = emitValue(ctx, *assign.value());

  // AssignStmt stores the target name; we need to resolve it via sema.
  auto sym = ctx.sema.resolvedSymbol(assign);
  if (!sym.has_value()) {
    ctx.emitError("Lowering: unresolved assignment target '" + assign.targetName() + "'");
    return;
  }

  const auto& symbol = ctx.sema.symbol(*sym);

  if (symbol.isVariable() || symbol.kind == SymbolKind::Parameter) {
    auto slot = ctx.lookupSlot(*sym);
    if (slot.has_value()) {
      ctx.builder.emitStoreSlot(*slot, rhs);
      return;
    }
    // Check global.
    auto git = ctx.symbolToGlobal.find(*sym);
    if (git != ctx.symbolToGlobal.end()) {
      ctx.builder.emitStoreGlobal(git->second, rhs);
      return;
    }
    ctx.emitError("Lowering: assignment target '" + assign.targetName() + "' has no slot or global");
    return;
  }

  if (symbol.isConstant()) {
    ctx.emitError("Lowering: cannot assign to constant '" + assign.targetName() + "'");
    return;
  }

  ctx.emitError("Lowering: unsupported assignment target '" + assign.targetName() + "'");
}

static void lowerExprStmt(LoweringContext& ctx, const ExprStmt& exprStmt) {
  // For void calls, use emitCallExpr directly to avoid the "void in value context" error.
  if (auto* call = dynamic_cast<const CallExpr*>(exprStmt.expression())) {
    emitCallExpr(ctx, *call);
    return;
  }
  emitValue(ctx, *exprStmt.expression());
}

static void lowerIfStmt(LoweringContext& ctx, const IfStmt& ifStmt) {
  BlockId thenBlock = ctx.builder.createBlock(ctx.nextBlockLabel("if.then"));
  BlockId mergeBlock = ctx.builder.createBlock(ctx.nextBlockLabel("if.merge"));

  if (ifStmt.elseBranch()) {
    BlockId elseBlock = ctx.builder.createBlock(ctx.nextBlockLabel("if.else"));
    emitCond(ctx, *ifStmt.condition(), thenBlock, elseBlock);

    // Then block.
    ctx.builder.setInsertBlock(thenBlock);
    lowerStmt(ctx, *ifStmt.thenBranch());
    if (!ctx.builder.insertBlock()->hasTerminator()) {
      ctx.builder.emitBranch(mergeBlock);
    }

    // Else block.
    ctx.builder.setInsertBlock(elseBlock);
    lowerStmt(ctx, *ifStmt.elseBranch());
    if (!ctx.builder.insertBlock()->hasTerminator()) {
      ctx.builder.emitBranch(mergeBlock);
    }
  } else {
    emitCond(ctx, *ifStmt.condition(), thenBlock, mergeBlock);

    // Then block.
    ctx.builder.setInsertBlock(thenBlock);
    lowerStmt(ctx, *ifStmt.thenBranch());
    if (!ctx.builder.insertBlock()->hasTerminator()) {
      ctx.builder.emitBranch(mergeBlock);
    }
  }

  ctx.builder.setInsertBlock(mergeBlock);
}

static void lowerWhileStmt(LoweringContext& ctx, const WhileStmt& whileStmt) {
  BlockId headerBlock = ctx.builder.createBlock(ctx.nextBlockLabel("while.header"));
  BlockId bodyBlock = ctx.builder.createBlock(ctx.nextBlockLabel("while.body"));
  BlockId exitBlock = ctx.builder.createBlock(ctx.nextBlockLabel("while.exit"));

  // Jump to header.
  ctx.builder.emitBranch(headerBlock);

  // Header: evaluate condition.
  ctx.builder.setInsertBlock(headerBlock);
  emitCond(ctx, *whileStmt.condition(), bodyBlock, exitBlock);

  // Push loop context for break/continue.
  ctx.loopStack.push_back({exitBlock, headerBlock});

  // Body.
  ctx.builder.setInsertBlock(bodyBlock);
  lowerStmt(ctx, *whileStmt.body());
  if (!ctx.builder.insertBlock()->hasTerminator()) {
    ctx.builder.emitBranch(headerBlock);
  }

  ctx.loopStack.pop_back();

  // Continue lowering after the loop.
  ctx.builder.setInsertBlock(exitBlock);
}

static void lowerReturnStmt(LoweringContext& ctx, const ReturnStmt& ret) {
  if (ret.value()) {
    ValueId val = emitValue(ctx, *ret.value());
    ctx.builder.emitReturn(val);
  } else {
    ctx.builder.emitReturn(std::nullopt);
  }
}

static void lowerBreakStmt(LoweringContext& ctx) {
  if (ctx.loopStack.empty()) {
    ctx.emitError("Lowering: break outside of loop");
    return;
  }
  ctx.builder.emitBranch(ctx.loopStack.back().breakTarget);
}

static void lowerContinueStmt(LoweringContext& ctx) {
  if (ctx.loopStack.empty()) {
    ctx.emitError("Lowering: continue outside of loop");
    return;
  }
  ctx.builder.emitBranch(ctx.loopStack.back().continueTarget);
}

static void lowerStmt(LoweringContext& ctx, const Stmt& stmt) {
  if (auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
    lowerBlockStmt(ctx, *block);
    return;
  }
  if (auto* decl = dynamic_cast<const DeclStmt*>(&stmt)) {
    lowerDeclStmt(ctx, *decl);
    return;
  }
  if (auto* assign = dynamic_cast<const AssignStmt*>(&stmt)) {
    lowerAssignStmt(ctx, *assign);
    return;
  }
  if (auto* expr = dynamic_cast<const ExprStmt*>(&stmt)) {
    lowerExprStmt(ctx, *expr);
    return;
  }
  if (auto* ifS = dynamic_cast<const IfStmt*>(&stmt)) {
    lowerIfStmt(ctx, *ifS);
    return;
  }
  if (auto* whileS = dynamic_cast<const WhileStmt*>(&stmt)) {
    lowerWhileStmt(ctx, *whileS);
    return;
  }
  if (auto* retS = dynamic_cast<const ReturnStmt*>(&stmt)) {
    lowerReturnStmt(ctx, *retS);
    return;
  }
  if (dynamic_cast<const BreakStmt*>(&stmt)) {
    lowerBreakStmt(ctx);
    return;
  }
  if (dynamic_cast<const ContinueStmt*>(&stmt)) {
    lowerContinueStmt(ctx);
    return;
  }
  if (dynamic_cast<const EmptyStmt*>(&stmt)) {
    return;  // No-op.
  }

  ctx.emitError("Lowering: unsupported statement type");
}

// ── Global initialization ──────────────────────────────────────────────────

static void lowerRuntimeGlobals(LoweringContext& ctx,
                                 const std::vector<std::pair<const Decl*, GlobalId>>& runtimeGlobals) {
  if (runtimeGlobals.empty()) return;

  // Create guard global.
  IRGlobal guardGlobal;
  guardGlobal.name = ".Ltoyc.global_init_guard";
  guardGlobal.kind = GlobalKind::InternalGuard;
  guardGlobal.initKind = IRGlobalInitKind::Static;
  guardGlobal.staticInitialValue = 0;
  guardGlobal.isInternal = true;
  GlobalId guardId = ctx.module.createGlobal(std::move(guardGlobal));

  // Create internal init function.
  Function* initFunc = ctx.module.createFunction(".Ltoyc.global_init", VoidIRType);
  initFunc->setInternal(true);

  // Build init function.
  ctx.builder.setFunction(initFunc);

  BlockId entryBlock = ctx.builder.createBlock("entry");
  BlockId initBlock = ctx.builder.createBlock("initialize");
  BlockId doneBlock = ctx.builder.createBlock("done");
  ctx.builder.setInsertBlock(entryBlock);

  // Load guard.
  ValueId guardVal = ctx.builder.emitLoadGlobal(guardId);

  // If guard != 0, skip initialization.
  ValueId zero = ctx.builder.emitConstInt(0);
  ValueId cmpResult = ctx.builder.emitCompare(ComparePredicate::NotEqual, guardVal, zero);
  ctx.builder.emitCondBranch(cmpResult, doneBlock, initBlock);

  // Initialize block.
  ctx.builder.setInsertBlock(initBlock);

  // Set guard to 1.
  ValueId one = ctx.builder.emitConstInt(1);
  ctx.builder.emitStoreGlobal(guardId, one);

  // Lower each runtime global initializer in source order.
  for (const auto& [decl, gid] : runtimeGlobals) {
    if (auto* varDecl = dynamic_cast<const VarDecl*>(decl)) {
      ValueId initVal = emitValue(ctx, *varDecl->initializer());
      ctx.builder.emitStoreGlobal(gid, initVal);
    }
  }
  ctx.builder.emitBranch(doneBlock);

  // Done block.
  ctx.builder.setInsertBlock(doneBlock);
  ctx.builder.emitReturn(std::nullopt);

  // Rebuild CFG for init function.
  rebuildCFG(*initFunc);
}

// ── Main lowering entry point ──────────────────────────────────────────────

ASTToIRLowering::ASTToIRLowering(const SemanticModel& semanticModel, DiagnosticEngine& diagnostics)
    : sema_(semanticModel), diag_(diagnostics) {}

std::optional<Module> ASTToIRLowering::lower(const CompUnit& unit) {
  Module module;
  LoweringContext ctx(module, sema_, diag_);

  // Track runtime globals for init function.
  std::vector<std::pair<const Decl*, GlobalId>> runtimeGlobals;

  // ── Phase 1: Create function stubs ───────────────────────────────────

  for (const auto& item : unit.items()) {
    if (auto* funcDef = dynamic_cast<const FuncDef*>(item.get())) {
      IRType retType = VoidIRType;
      if (funcDef->returnType() == TypeKind::Int) {
        retType = I32Type;
      }
      module.createFunction(funcDef->name(), retType);
    }
  }

  // ── Phase 2: Create global stubs ─────────────────────────────────────

  for (const auto& item : unit.items()) {
    if (auto* globalDecl = dynamic_cast<const GlobalDecl*>(item.get())) {
      const Decl* decl = globalDecl->declaration();
      auto sym = sema_.resolvedSymbol(*decl);

      if (auto* varDecl = dynamic_cast<const VarDecl*>(decl)) {
        auto initInfo = sema_.globalInitInfo(*decl);

        IRGlobal global;
        global.name = varDecl->name();
        global.kind = GlobalKind::Variable;
        global.sourceSymbol = sym;

        if (initInfo.has_value() && initInfo->kind == GlobalInitKind::StaticConstant) {
          global.initKind = IRGlobalInitKind::Static;
          global.staticInitialValue = initInfo->staticValue.value_or(0);
        } else {
          global.initKind = IRGlobalInitKind::RuntimeZeroInitialized;
          global.staticInitialValue = 0;
          // Will push to runtimeGlobals after we know the GlobalId.

        }

        GlobalId gid = module.createGlobal(std::move(global));
        if (sym.has_value()) {
          ctx.symbolToGlobal[*sym] = gid;
        }

        if (initInfo.has_value() && initInfo->kind != GlobalInitKind::StaticConstant) {
          runtimeGlobals.push_back({decl, gid});
        }
      }
      if (auto* constDecl = dynamic_cast<const ConstDecl*>(decl)) {
        auto exprInfo = sema_.exprInfo(*constDecl->initializer());

        IRGlobal global;
        global.name = constDecl->name();
        global.kind = GlobalKind::Constant;
        global.sourceSymbol = sym;
        global.initKind = IRGlobalInitKind::Static;
        global.staticInitialValue = exprInfo ? exprInfo->constantValue.value_or(0) : 0;

        GlobalId gid = module.createGlobal(std::move(global));
        if (sym.has_value()) {
          ctx.symbolToGlobal[*sym] = gid;
        }
      }
    }
  }

  // ── Phase 3: Lower runtime globals ───────────────────────────────────

  lowerRuntimeGlobals(ctx, runtimeGlobals);

  // ── Phase 4: Lower each function ─────────────────────────────────────

  for (const auto& item : unit.items()) {
    auto* funcDef = dynamic_cast<const FuncDef*>(item.get());
    if (!funcDef) continue;

    // Find the function in the module.
    Function* func = module.findFunctionByName(funcDef->name());
    if (!func) {
      ctx.emitError("Lowering: function '" + funcDef->name() + "' not found");
      continue;
    }

    ctx.currentFunc = func;
    ctx.builder.setFunction(func);
    ctx.pushScope();

    // Create entry block.
    BlockId entryBlock = ctx.builder.createBlock("entry");
    ctx.builder.setInsertBlock(entryBlock);

    // Create parameter slots and store argument values.
    for (size_t i = 0; i < funcDef->params().size(); ++i) {
      const auto& param = funcDef->params()[i];
      auto sym = sema_.resolvedSymbol(param);
      ParamInfo paramInfo = func->addParam(sym.value_or(SymbolId{0}));
      if (sym.has_value()) {
        ctx.bindSymbol(*sym, paramInfo.slotId);
      }

      // Store argument value to parameter slot.
      ctx.builder.emitStoreSlot(paramInfo.slotId, paramInfo.valueId);
    }

    // Lower function body.
    lowerStmt(ctx, *funcDef->body());

    // Ensure function ends with a terminator.
    auto* lastBlock = ctx.builder.insertBlock();
    if (lastBlock && !lastBlock->hasTerminator()) {
      if (func->returnType().isVoid()) {
        ctx.builder.emitReturn(std::nullopt);
      } else {
        ValueId zero = ctx.builder.emitConstInt(0);
        ctx.builder.emitReturn(zero);
      }
    }

    ctx.popScope();

    // Rebuild CFG.
    rebuildCFG(*func);
  }

  // ── Phase 5: Insert global init call in main ─────────────────────────

  if (!runtimeGlobals.empty()) {
    Function* mainFunc = module.findFunctionByName("main");
    Function* initFunc = module.findFunctionByName(".Ltoyc.global_init");
    if (mainFunc && initFunc) {
      auto& entryBlock = mainFunc->blocks().front();
      auto callInst = std::make_unique<Inst>();
      callInst->opcode = Opcode::Call;
      callInst->resultType = VoidIRType;
      callInst->result = std::nullopt;
      callInst->callee = initFunc->id();

      // Insert at the beginning of the instruction list.
      auto& insts = const_cast<std::vector<std::unique_ptr<Inst>>&>(entryBlock->instructions());
      insts.insert(insts.begin(), std::move(callInst));
    }
  }

  if (hasError_) {
    return std::nullopt;
  }

  return module;
}

} // namespace toyc
