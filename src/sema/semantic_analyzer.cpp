/// SemanticAnalyzer implementation — P3 semantic analysis.

#include "toyc/sema/semantic_analyzer.h"
#include "toyc/sema/constant_evaluator.h"

#include <climits>

namespace toyc {

SemanticAnalyzer::SemanticAnalyzer(DiagnosticEngine& diagnostics)
    : diag_(diagnostics) {}

void SemanticAnalyzer::reportError(SourceLocation loc, std::string message) {
  diag_.error(loc, std::move(message));
  hasError_ = true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Top-level analysis
// ═══════════════════════════════════════════════════════════════════════════

std::optional<SemanticModel> SemanticAnalyzer::analyze(const CompUnit& unit) {
  for (const auto& item : unit.items()) {
    analyzeTopLevelItem(*item);
  }

  // Check main function.
  bool hasMain = false;
  for (const auto& sym : model_.symbols()) {
    if (sym.name == "main" && sym.kind == SymbolKind::Function) {
      hasMain = true;
      if (sym.type != TypeKind::Int) {
        reportError(sym.declaration->range().begin,
                    "main function must return int");
      }
      if (!sym.parameterNames.empty()) {
        reportError(sym.declaration->range().begin,
                    "main function must not have parameters");
      }
      break;
    }
  }
  if (!hasMain) {
    reportError(SourceLocation{}, "program must contain a 'main' function");
  }

  return std::move(model_);
}

void SemanticAnalyzer::analyzeTopLevelItem(const TopLevelItem& item) {
  switch (item.kind()) {
    case ASTKind::GlobalDecl:
      analyzeGlobalDecl(static_cast<const GlobalDecl&>(item));
      break;
    case ASTKind::FuncDef:
      analyzeFuncDef(static_cast<const FuncDef&>(item));
      break;
    default:
      break;
  }
}

void SemanticAnalyzer::analyzeGlobalDecl(const GlobalDecl& gl) {
  const Decl* d = gl.declaration();
  if (!d) return;
  if (d->kind() == ASTKind::ConstDecl) {
    analyzeConstDecl(static_cast<const ConstDecl&>(*d), true);
  } else {
    analyzeVarDecl(static_cast<const VarDecl&>(*d), true);
  }
}

void SemanticAnalyzer::analyzeFuncDef(const FuncDef& func) {
  // Check redefinition.
  auto existing = scopes_.lookupCurrent(func.name());
  if (existing.has_value()) {
    reportError(func.range().begin,
                "redefinition of '" + func.name() + "'");
    return;
  }

  // Create function symbol.
  Symbol funcSym;
  funcSym.name = func.name();
  funcSym.kind = SymbolKind::Function;
  funcSym.type = func.returnType();
  funcSym.scope = scopes_.currentScopeId();
  funcSym.declaration = &func;
  funcSym.isDefined = true;
  for (const auto& p : func.params()) {
    funcSym.parameterTypes.push_back(TypeKind::Int);
    funcSym.parameterNames.push_back(p.name());
  }

  SymbolId funcId = model_.addSymbol(std::move(funcSym));
  scopes_.declare(func.name(), funcId);
  model_.resolveNode(&func, funcId);

  // Push parameter scope.
  scopes_.pushScope();
  for (const auto& param : func.params()) {
    auto paramExisting = scopes_.lookupCurrent(param.name());
    if (paramExisting.has_value()) {
      reportError(param.range().begin,
                  "redefinition of parameter '" + param.name() + "'");
      continue;
    }
    Symbol paramSym;
    paramSym.name = param.name();
    paramSym.kind = SymbolKind::Parameter;
    paramSym.type = TypeKind::Int;
    paramSym.scope = scopes_.currentScopeId();
    paramSym.declaration = &param;
    paramSym.isDefined = true;

    SymbolId paramId = model_.addSymbol(std::move(paramSym));
    scopes_.declare(param.name(), paramId);
    model_.resolveNode(&param, paramId);
  }

  // Analyze body.
  FlowSummary bodyFlow = analyzeBlock(*func.body(), func.returnType(), false);

  // Check return paths.
  if (func.returnType() == TypeKind::Int && bodyFlow.fallsThrough) {
    reportError(func.range().end,
                "not all control paths return a value in function '" + func.name() + "'");
  }

  scopes_.popScope();
}

// ═══════════════════════════════════════════════════════════════════════════
// Declarations
// ═══════════════════════════════════════════════════════════════════════════

SymbolId SemanticAnalyzer::analyzeConstDecl(const ConstDecl& decl, bool isGlobal) {
  // Evaluate initializer BEFORE declaring the constant.
  auto constVal = evalConstWithSymbols(*decl.initializer());

  // Also analyze for type checking.
  analyzeExpr(*decl.initializer());

  // Check redefinition.
  auto existing = scopes_.lookupCurrent(decl.name());
  if (existing.has_value()) {
    reportError(decl.range().begin,
                "redefinition of '" + decl.name() + "'");
    return existing.value();
  }

  Symbol sym;
  sym.name = decl.name();
  sym.kind = isGlobal ? SymbolKind::GlobalConstant : SymbolKind::LocalConstant;
  sym.type = TypeKind::Int;
  sym.scope = scopes_.currentScopeId();
  sym.declaration = &decl;
  sym.isDefined = true;
  sym.constantValue = constVal;

  SymbolId id = model_.addSymbol(std::move(sym));
  scopes_.declare(decl.name(), id);
  model_.resolveNode(&decl, id);

  // Global init classification.
  if (isGlobal) {
    GlobalInitInfo gii;
    if (constVal.has_value()) {
      gii.kind = GlobalInitKind::StaticConstant;
      gii.staticValue = constVal;
    } else {
      gii.kind = GlobalInitKind::Runtime;
    }
    model_.setGlobalInitInfo(&decl, gii);
  }

  return id;
}

SymbolId SemanticAnalyzer::analyzeVarDecl(const VarDecl& decl, bool isGlobal) {
  auto initInfo = analyzeExpr(*decl.initializer());

  auto existing = scopes_.lookupCurrent(decl.name());
  if (existing.has_value()) {
    reportError(decl.range().begin,
                "redefinition of '" + decl.name() + "'");
    return existing.value();
  }

  if (initInfo.type == SemanticType::Void) {
    reportError(decl.initializer()->range().begin,
                "cannot initialize variable with void expression");
  }

  Symbol sym;
  sym.name = decl.name();
  sym.kind = isGlobal ? SymbolKind::GlobalVariable : SymbolKind::LocalVariable;
  sym.type = TypeKind::Int;
  sym.scope = scopes_.currentScopeId();
  sym.declaration = &decl;
  sym.isDefined = true;

  SymbolId id = model_.addSymbol(std::move(sym));
  scopes_.declare(decl.name(), id);
  model_.resolveNode(&decl, id);

  if (isGlobal) {
    GlobalInitInfo gii;
    auto constVal = probeStaticInitValue(*decl.initializer());
    if (constVal.has_value()) {
      gii.kind = GlobalInitKind::StaticConstant;
      gii.staticValue = constVal;
    } else {
      gii.kind = GlobalInitKind::Runtime;
    }
    model_.setGlobalInitInfo(&decl, gii);
  }

  return id;
}

// ═══════════════════════════════════════════════════════════════════════════
// Statements
// ═══════════════════════════════════════════════════════════════════════════

FlowSummary SemanticAnalyzer::analyzeStmt(const Stmt& stmt,
                                           TypeKind enclosingFuncReturn,
                                           bool inLoop) {
  switch (stmt.kind()) {
    case ASTKind::BlockStmt:
      return analyzeBlock(static_cast<const BlockStmt&>(stmt),
                          enclosingFuncReturn, inLoop);

    case ASTKind::EmptyStmt:
      return {true, false};

    case ASTKind::ExprStmt: {
      const auto& es = static_cast<const ExprStmt&>(stmt);
      analyzeExpr(*es.expression());
      return {true, false};
    }

    case ASTKind::AssignStmt: {
      const auto& as = static_cast<const AssignStmt&>(stmt);
      auto targetSym = scopes_.lookupVisible(as.targetName());
      if (!targetSym.has_value()) {
        reportError(as.range().begin,
                    "undefined identifier '" + as.targetName() + "'");
      } else {
        const auto& sym = model_.symbol(targetSym.value());
        if (sym.isConstant()) {
          reportError(as.range().begin,
                      "cannot assign to constant '" + as.targetName() + "'");
        } else if (sym.kind == SymbolKind::Function) {
          reportError(as.range().begin,
                      "function '" + as.targetName() + "' cannot be assignment target");
        }
        model_.resolveNode(&as, targetSym.value());
      }
      auto valInfo = analyzeExpr(*as.value());
      if (valInfo.type == SemanticType::Void) {
        reportError(as.value()->range().begin,
                    "cannot assign void expression");
      }
      return {true, false};
    }

    case ASTKind::DeclStmt: {
      const auto& ds = static_cast<const DeclStmt&>(stmt);
      const Decl* d = ds.declaration();
      if (d->kind() == ASTKind::ConstDecl) {
        analyzeConstDecl(static_cast<const ConstDecl&>(*d), false);
      } else {
        analyzeVarDecl(static_cast<const VarDecl&>(*d), false);
      }
      return {true, false};
    }

    case ASTKind::IfStmt:
      return analyzeIfStmt(static_cast<const IfStmt&>(stmt),
                           enclosingFuncReturn, inLoop);

    case ASTKind::WhileStmt:
      return analyzeWhileStmt(static_cast<const WhileStmt&>(stmt),
                              enclosingFuncReturn);

    case ASTKind::BreakStmt:
      if (!inLoop) {
        reportError(stmt.range().begin, "break statement not in loop");
      }
      return {false, true};

    case ASTKind::ContinueStmt:
      if (!inLoop) {
        reportError(stmt.range().begin, "continue statement not in loop");
      }
      return {false, false};

    case ASTKind::ReturnStmt:
      return analyzeReturnStmt(static_cast<const ReturnStmt&>(stmt),
                               enclosingFuncReturn);

    default:
      return {true, false};
  }
}

FlowSummary SemanticAnalyzer::analyzeBlock(const BlockStmt& block,
                                            TypeKind enclosingFuncReturn,
                                            bool inLoop) {
  scopes_.pushScope();

  FlowSummary result{true, false};
  for (const auto& stmt : block.statements()) {
    if (!result.fallsThrough) {
      analyzeStmt(*stmt, enclosingFuncReturn, inLoop);
      continue;
    }
    auto stmtFlow = analyzeStmt(*stmt, enclosingFuncReturn, inLoop);
    result.fallsThrough = stmtFlow.fallsThrough;
    if (stmtFlow.breaksCurrentLoop) {
      result.breaksCurrentLoop = true;
    }
  }

  scopes_.popScope();
  return result;
}

FlowSummary SemanticAnalyzer::analyzeIfStmt(const IfStmt& stmt,
                                             TypeKind enclosingFuncReturn,
                                             bool inLoop) {
  auto condInfo = analyzeExpr(*stmt.condition());
  if (condInfo.type == SemanticType::Void) {
    reportError(stmt.condition()->range().begin,
                "if condition must be int, got void");
  }

  auto thenFlow = analyzeStmt(*stmt.thenBranch(), enclosingFuncReturn, inLoop);

  if (!stmt.elseBranch()) {
    return {true, thenFlow.breaksCurrentLoop};
  }

  auto elseFlow = analyzeStmt(*stmt.elseBranch(), enclosingFuncReturn, inLoop);
  return {
    thenFlow.fallsThrough || elseFlow.fallsThrough,
    thenFlow.breaksCurrentLoop || elseFlow.breaksCurrentLoop
  };
}

FlowSummary SemanticAnalyzer::analyzeWhileStmt(const WhileStmt& stmt,
                                                TypeKind enclosingFuncReturn) {
  auto condInfo = analyzeExpr(*stmt.condition());
  if (condInfo.type == SemanticType::Void) {
    reportError(stmt.condition()->range().begin,
                "while condition must be int, got void");
  }

  ++loopDepth_;
  auto bodyFlow = analyzeStmt(*stmt.body(), enclosingFuncReturn, true);
  --loopDepth_;

  // while(0) body never executes — falls through past the loop.
  if (condInfo.constantValue.has_value() && condInfo.constantValue.value() == 0) {
    return {true, false};
  }

  // while(nonzero-const): if body has no break, the loop never exits.
  // (body falling through just means it loops again — still infinite)
  if (condInfo.constantValue.has_value() && condInfo.constantValue.value() != 0) {
    if (!bodyFlow.breaksCurrentLoop) {
      return {false, false};
    }
    // Body can break — loop may exit.
    return {true, false};
  }

  // Condition is not constant — loop may or may not execute.
  return {true, false};
}

FlowSummary SemanticAnalyzer::analyzeReturnStmt(const ReturnStmt& stmt,
                                                 TypeKind enclosingFuncReturn) {
  if (stmt.value()) {
    auto valInfo = analyzeExpr(*stmt.value());
    if (enclosingFuncReturn == TypeKind::Void) {
      reportError(stmt.range().begin,
                  "void function should not return a value");
    } else if (valInfo.type == SemanticType::Void) {
      reportError(stmt.value()->range().begin,
                  "cannot return void expression from int function");
    }
  } else {
    if (enclosingFuncReturn == TypeKind::Int) {
      reportError(stmt.range().begin,
                  "int function must return a value");
    }
  }
  return {false, false};
}

// ═══════════════════════════════════════════════════════════════════════════
// Expressions
// ═══════════════════════════════════════════════════════════════════════════

ExprSemanticInfo SemanticAnalyzer::analyzeExpr(const Expr& expr) {
  ExprSemanticInfo info;
  switch (expr.kind()) {
    case ASTKind::IntegerLiteralExpr:
      info = analyzeIntegerLiteral(static_cast<const IntegerLiteralExpr&>(expr));
      break;
    case ASTKind::IdentifierExpr:
      info = analyzeIdentifier(static_cast<const IdentifierExpr&>(expr));
      break;
    case ASTKind::CallExpr:
      info = analyzeCallExpr(static_cast<const CallExpr&>(expr));
      break;
    case ASTKind::UnaryExpr:
      info = analyzeUnaryExpr(static_cast<const UnaryExpr&>(expr));
      break;
    case ASTKind::BinaryExpr:
      info = analyzeBinaryExpr(static_cast<const BinaryExpr&>(expr));
      break;
    default:
      info.type = SemanticType::Error;
      break;
  }
  model_.setExprInfo(&expr, info);
  return info;
}

ExprSemanticInfo SemanticAnalyzer::analyzeIntegerLiteral(
    const IntegerLiteralExpr& expr) {
  auto mag = parseUnsignedMagnitude(expr.rawValue());
  if (!mag.has_value()) {
    reportError(expr.range().begin, "integer literal out of range");
    return {SemanticType::Error, std::nullopt};
  }
  if (mag.value() > static_cast<uint64_t>(INT32_MAX)) {
    reportError(expr.range().begin, "integer literal out of range");
    return {SemanticType::Error, std::nullopt};
  }
  return {SemanticType::Int, static_cast<int32_t>(mag.value())};
}

ExprSemanticInfo SemanticAnalyzer::analyzeIdentifier(
    const IdentifierExpr& expr) {
  auto sym = scopes_.lookupVisible(expr.name());
  if (!sym.has_value()) {
    reportError(expr.range().begin,
                "undefined identifier '" + expr.name() + "'");
    return {SemanticType::Error, std::nullopt};
  }
  const auto& symbol = model_.symbol(sym.value());
  model_.resolveNode(&expr, sym.value());

  if (symbol.kind == SymbolKind::Function) {
    reportError(expr.range().begin,
                "function '" + expr.name() + "' cannot be used as a value");
    return {SemanticType::Error, std::nullopt};
  }

  ExprSemanticInfo info{SemanticType::Int, std::nullopt};
  if (symbol.isConstant() && symbol.constantValue.has_value()) {
    info.constantValue = symbol.constantValue;
  }
  return info;
}

ExprSemanticInfo SemanticAnalyzer::analyzeCallExpr(const CallExpr& expr) {
  auto calleeSym = scopes_.lookupVisible(expr.calleeName());
  if (!calleeSym.has_value()) {
    reportError(expr.range().begin,
                "undefined function '" + expr.calleeName() + "'");
    for (const auto& arg : expr.arguments()) analyzeExpr(*arg);
    return {SemanticType::Error, std::nullopt};
  }

  const auto& symbol = model_.symbol(calleeSym.value());
  model_.resolveNode(&expr, calleeSym.value());

  if (symbol.kind != SymbolKind::Function) {
    reportError(expr.range().begin,
                "'" + expr.calleeName() + "' is not a function");
    for (const auto& arg : expr.arguments()) analyzeExpr(*arg);
    return {SemanticType::Error, std::nullopt};
  }

  if (expr.arguments().size() != symbol.parameterTypes.size()) {
    reportError(expr.range().begin,
                "function '" + expr.calleeName() + "' expects " +
                std::to_string(symbol.parameterTypes.size()) +
                " arguments, got " + std::to_string(expr.arguments().size()));
  }

  for (const auto& arg : expr.arguments()) {
    auto argInfo = analyzeExpr(*arg);
    if (argInfo.type == SemanticType::Void) {
      reportError(arg->range().begin,
                  "cannot pass void expression as argument");
    }
  }

  return {symbol.type == TypeKind::Int ? SemanticType::Int : SemanticType::Void,
          std::nullopt};
}

ExprSemanticInfo SemanticAnalyzer::analyzeUnaryExpr(const UnaryExpr& expr) {
  // Special handling for -(integer literal) to support INT32_MIN.
  if (expr.op() == UnaryOperator::Minus &&
      expr.operand()->kind() == ASTKind::IntegerLiteralExpr) {
    auto& lit = static_cast<const IntegerLiteralExpr&>(*expr.operand());
    auto mag = parseUnsignedMagnitude(lit.rawValue());
    if (mag.has_value()) {
      if (mag.value() == 2147483648ULL) {
        // Special case: -2147483648 = INT32_MIN
        model_.setExprInfo(expr.operand(), {SemanticType::Int, std::nullopt});
        return {SemanticType::Int, INT32_MIN};
      }
      if (mag.value() <= static_cast<uint64_t>(INT32_MAX)) {
        int64_t neg = -static_cast<int64_t>(mag.value());
        model_.setExprInfo(expr.operand(),
                           {SemanticType::Int, static_cast<int32_t>(mag.value())});
        return {SemanticType::Int, static_cast<int32_t>(neg)};
      }
      // 2147483649+ with minus: the literal itself is out of range.
      reportError(lit.range().begin, "integer literal out of range");
      model_.setExprInfo(expr.operand(), {SemanticType::Error, std::nullopt});
      return {SemanticType::Error, std::nullopt};
    }
    reportError(lit.range().begin, "integer literal out of range");
    model_.setExprInfo(expr.operand(), {SemanticType::Error, std::nullopt});
    return {SemanticType::Error, std::nullopt};
  }

  auto operandInfo = analyzeExpr(*expr.operand());

  if (operandInfo.type == SemanticType::Void) {
    reportError(expr.operand()->range().begin,
                "unary operator requires int operand");
    return {SemanticType::Error, std::nullopt};
  }

  if (expr.op() == UnaryOperator::Plus) {
    return operandInfo;
  }

  if (expr.op() == UnaryOperator::LogicalNot) {
    if (operandInfo.constantValue.has_value()) {
      return {SemanticType::Int, operandInfo.constantValue.value() == 0 ? 1 : 0};
    }
    return {SemanticType::Int, std::nullopt};
  }

  // UnaryOperator::Minus
  if (operandInfo.constantValue.has_value()) {
    int64_t v = operandInfo.constantValue.value();
    int64_t r = -v;
    if (r < INT32_MIN || r > INT32_MAX) {
      reportError(expr.range().begin, "constant expression overflow");
      return {SemanticType::Int, std::nullopt};
    }
    return {SemanticType::Int, static_cast<int32_t>(r)};
  }

  return {SemanticType::Int, std::nullopt};
}

ExprSemanticInfo SemanticAnalyzer::analyzeBinaryExpr(const BinaryExpr& expr) {
  auto lhsInfo = analyzeExpr(*expr.lhs());

  // Short-circuit &&
  if (expr.op() == BinaryOperator::LogicalAnd) {
    if (lhsInfo.type == SemanticType::Void) {
      reportError(expr.lhs()->range().begin,
                  "logical operator requires int operands");
    }
    if (lhsInfo.constantValue.has_value() && lhsInfo.constantValue.value() == 0) {
      auto rhsInfo = analyzeExpr(*expr.rhs());
      if (rhsInfo.type == SemanticType::Void) {
        reportError(expr.rhs()->range().begin,
                    "logical operator requires int operands");
      }
      return {SemanticType::Int, 0};
    }
    auto rhsInfo = analyzeExpr(*expr.rhs());
    if (rhsInfo.type == SemanticType::Void) {
      reportError(expr.rhs()->range().begin,
                  "logical operator requires int operands");
    }
    if (lhsInfo.constantValue.has_value() && rhsInfo.constantValue.has_value()) {
      return {SemanticType::Int,
              (lhsInfo.constantValue.value() != 0 && rhsInfo.constantValue.value() != 0) ? 1 : 0};
    }
    return {SemanticType::Int, std::nullopt};
  }

  // Short-circuit ||
  if (expr.op() == BinaryOperator::LogicalOr) {
    if (lhsInfo.type == SemanticType::Void) {
      reportError(expr.lhs()->range().begin,
                  "logical operator requires int operands");
    }
    if (lhsInfo.constantValue.has_value() && lhsInfo.constantValue.value() != 0) {
      auto rhsInfo = analyzeExpr(*expr.rhs());
      if (rhsInfo.type == SemanticType::Void) {
        reportError(expr.rhs()->range().begin,
                    "logical operator requires int operands");
      }
      return {SemanticType::Int, 1};
    }
    auto rhsInfo = analyzeExpr(*expr.rhs());
    if (rhsInfo.type == SemanticType::Void) {
      reportError(expr.rhs()->range().begin,
                  "logical operator requires int operands");
    }
    if (lhsInfo.constantValue.has_value() && rhsInfo.constantValue.has_value()) {
      return {SemanticType::Int,
              (lhsInfo.constantValue.value() != 0 || rhsInfo.constantValue.value() != 0) ? 1 : 0};
    }
    return {SemanticType::Int, std::nullopt};
  }

  // Non-short-circuit.
  auto rhsInfo = analyzeExpr(*expr.rhs());

  if (lhsInfo.type == SemanticType::Void || rhsInfo.type == SemanticType::Void) {
    reportError(expr.range().begin, "binary operator requires int operands");
    return {SemanticType::Error, std::nullopt};
  }

  if (lhsInfo.constantValue.has_value() && rhsInfo.constantValue.has_value()) {
    int64_t l = lhsInfo.constantValue.value();
    int64_t r = rhsInfo.constantValue.value();
    int64_t result = 0;

    switch (expr.op()) {
      case BinaryOperator::Add:        result = l + r; break;
      case BinaryOperator::Subtract:   result = l - r; break;
      case BinaryOperator::Multiply:   result = l * r; break;
      case BinaryOperator::Divide:
        if (r == 0) {
          reportError(expr.range().begin, "division by zero");
          return {SemanticType::Int, std::nullopt};
        }
        if (l == INT32_MIN && r == -1) {
          reportError(expr.range().begin, "constant expression overflow");
          return {SemanticType::Int, std::nullopt};
        }
        result = l / r;
        break;
      case BinaryOperator::Modulo:
        if (r == 0) {
          reportError(expr.range().begin, "modulo by zero");
          return {SemanticType::Int, std::nullopt};
        }
        if (l == INT32_MIN && r == -1) {
          reportError(expr.range().begin, "constant expression overflow");
          return {SemanticType::Int, std::nullopt};
        }
        result = l % r;
        break;
      case BinaryOperator::Equal:        result = (l == r) ? 1 : 0; break;
      case BinaryOperator::NotEqual:     result = (l != r) ? 1 : 0; break;
      case BinaryOperator::Less:         result = (l < r) ? 1 : 0; break;
      case BinaryOperator::LessEqual:    result = (l <= r) ? 1 : 0; break;
      case BinaryOperator::Greater:      result = (l > r) ? 1 : 0; break;
      case BinaryOperator::GreaterEqual: result = (l >= r) ? 1 : 0; break;
      default: break;
    }

    if (expr.op() == BinaryOperator::Add ||
        expr.op() == BinaryOperator::Subtract ||
        expr.op() == BinaryOperator::Multiply) {
      if (result < INT32_MIN || result > INT32_MAX) {
        reportError(expr.range().begin, "constant expression overflow");
        return {SemanticType::Int, std::nullopt};
      }
    }

    return {SemanticType::Int, static_cast<int32_t>(result)};
  }

  return {SemanticType::Int, std::nullopt};
}

// ═══════════════════════════════════════════════════════════════════════════
// Constant evaluation with symbol resolution
// ═══════════════════════════════════════════════════════════════════════════

std::optional<int32_t> SemanticAnalyzer::evalConstWithSymbols(const Expr& expr) {
  ConstLookup lookup = [this](std::string_view name) -> std::optional<int32_t> {
    auto sym = scopes_.lookupVisible(name);
    if (!sym.has_value()) return std::nullopt;
    const auto& symbol = model_.symbol(sym.value());
    if (symbol.isConstant() && symbol.constantValue.has_value()) {
      return symbol.constantValue;
    }
    return std::nullopt;
  };

  auto result = evaluateConstExpr(expr, diag_, lookup);
  if (result.state == ConstEvalState::Known) {
    return result.value;
  }
  return std::nullopt;
}

std::optional<int32_t> SemanticAnalyzer::probeStaticInitValue(const Expr& expr) {
  // Use a separate DiagnosticEngine to suppress errors during probing.
  // Non-constant identifiers return nullopt (unknown) without error.
  DiagnosticEngine probeDiag;
  ConstLookup lookup = [this](std::string_view name) -> std::optional<int32_t> {
    auto sym = scopes_.lookupVisible(name);
    if (!sym.has_value()) return std::nullopt;
    const auto& symbol = model_.symbol(sym.value());
    if (symbol.isConstant() && symbol.constantValue.has_value()) {
      return symbol.constantValue;
    }
    return std::nullopt;
  };

  auto result = evaluateConstExpr(expr, probeDiag, lookup);
  if (result.state == ConstEvalState::Known) {
    return result.value;
  }
  return std::nullopt;
}

} // namespace toyc
