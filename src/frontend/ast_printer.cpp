/// AST printer — P2 structured tree output.

#include "toyc/frontend/ast_printer.h"

namespace toyc {

// ═══════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════

void ASTPrinter::indent() {
  for (int i = 0; i < depth_; ++i) out_ << "  ";
}

void ASTPrinter::pushIndent() { ++depth_; }
void ASTPrinter::popIndent() { --depth_; }

// ═══════════════════════════════════════════════════════════════════════════
// Top-level
// ═══════════════════════════════════════════════════════════════════════════

void ASTPrinter::visitCompUnit(const CompUnit& node) {
  indent();
  out_ << "CompUnit\n";
  pushIndent();
  for (const auto& item : node.items()) {
    accept(*item, *this);
  }
  popIndent();
}

void ASTPrinter::visitGlobalDecl(const GlobalDecl& node) {
  accept(*node.declaration(), *this);
}

void ASTPrinter::visitFuncDef(const FuncDef& node) {
  indent();
  out_ << "FuncDef return=" << typeKindName(node.returnType())
       << " name=" << node.name() << "\n";
  pushIndent();

  indent();
  out_ << "Params\n";
  pushIndent();
  for (const auto& p : node.params()) {
    visitParamDecl(p);
  }
  popIndent();

  accept(*node.body(), *this);
  popIndent();
}

// ═══════════════════════════════════════════════════════════════════════════
// Declarations
// ═══════════════════════════════════════════════════════════════════════════

void ASTPrinter::visitConstDecl(const ConstDecl& node) {
  indent();
  out_ << "GlobalConstDecl name=" << node.name() << "\n";
  if (node.initializer()) {
    pushIndent();
    indent();
    out_ << "Initializer\n";
    pushIndent();
    accept(*node.initializer(), *this);
    popIndent();
    popIndent();
  }
}

void ASTPrinter::visitVarDecl(const VarDecl& node) {
  indent();
  out_ << "GlobalVarDecl name=" << node.name() << "\n";
  if (node.initializer()) {
    pushIndent();
    indent();
    out_ << "Initializer\n";
    pushIndent();
    accept(*node.initializer(), *this);
    popIndent();
    popIndent();
  }
}

void ASTPrinter::visitParamDecl(const ParamDecl& node) {
  indent();
  out_ << "Param name=" << node.name() << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// Statements
// ═══════════════════════════════════════════════════════════════════════════

void ASTPrinter::visitBlockStmt(const BlockStmt& node) {
  indent();
  out_ << "BlockStmt\n";
  pushIndent();
  for (const auto& s : node.statements()) {
    accept(*s, *this);
  }
  popIndent();
}

void ASTPrinter::visitEmptyStmt(const EmptyStmt& /*node*/) {
  indent();
  out_ << "EmptyStmt\n";
}

void ASTPrinter::visitExprStmt(const ExprStmt& node) {
  indent();
  out_ << "ExprStmt\n";
  pushIndent();
  accept(*node.expression(), *this);
  popIndent();
}

void ASTPrinter::visitAssignStmt(const AssignStmt& node) {
  indent();
  out_ << "AssignStmt target=" << node.targetName() << "\n";
  pushIndent();
  indent();
  out_ << "Value\n";
  pushIndent();
  accept(*node.value(), *this);
  popIndent();
  popIndent();
}

void ASTPrinter::visitDeclStmt(const DeclStmt& node) {
  // DeclStmt prints the inner declaration directly with Local prefix.
  const Decl* d = node.declaration();
  if (d->kind() == ASTKind::ConstDecl) {
    indent();
    out_ << "LocalConstDecl name=" << d->name() << "\n";
    if (d->initializer()) {
      pushIndent();
      indent();
      out_ << "Initializer\n";
      pushIndent();
      accept(*d->initializer(), *this);
      popIndent();
      popIndent();
    }
  } else {
    indent();
    out_ << "LocalVarDecl name=" << d->name() << "\n";
    if (d->initializer()) {
      pushIndent();
      indent();
      out_ << "Initializer\n";
      pushIndent();
      accept(*d->initializer(), *this);
      popIndent();
      popIndent();
    }
  }
}

void ASTPrinter::visitIfStmt(const IfStmt& node) {
  indent();
  out_ << "IfStmt\n";
  pushIndent();

  indent();
  out_ << "Condition\n";
  pushIndent();
  accept(*node.condition(), *this);
  popIndent();

  indent();
  out_ << "Then\n";
  pushIndent();
  accept(*node.thenBranch(), *this);
  popIndent();

  if (node.elseBranch()) {
    indent();
    out_ << "Else\n";
    pushIndent();
    accept(*node.elseBranch(), *this);
    popIndent();
  }

  popIndent();
}

void ASTPrinter::visitWhileStmt(const WhileStmt& node) {
  indent();
  out_ << "WhileStmt\n";
  pushIndent();

  indent();
  out_ << "Condition\n";
  pushIndent();
  accept(*node.condition(), *this);
  popIndent();

  indent();
  out_ << "Body\n";
  pushIndent();
  accept(*node.body(), *this);
  popIndent();

  popIndent();
}

void ASTPrinter::visitBreakStmt(const BreakStmt& /*node*/) {
  indent();
  out_ << "BreakStmt\n";
}

void ASTPrinter::visitContinueStmt(const ContinueStmt& /*node*/) {
  indent();
  out_ << "ContinueStmt\n";
}

void ASTPrinter::visitReturnStmt(const ReturnStmt& node) {
  indent();
  if (node.value()) {
    out_ << "ReturnStmt\n";
    pushIndent();
    accept(*node.value(), *this);
    popIndent();
  } else {
    out_ << "ReturnStmt\n";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Expressions
// ═══════════════════════════════════════════════════════════════════════════

void ASTPrinter::visitIntegerLiteralExpr(const IntegerLiteralExpr& node) {
  indent();
  out_ << "IntegerLiteral value=" << node.rawValue() << "\n";
}

void ASTPrinter::visitIdentifierExpr(const IdentifierExpr& node) {
  indent();
  out_ << "Identifier name=" << node.name() << "\n";
}

void ASTPrinter::visitUnaryExpr(const UnaryExpr& node) {
  indent();
  out_ << "UnaryExpr op=" << unaryOpName(node.op()) << "\n";
  pushIndent();
  accept(*node.operand(), *this);
  popIndent();
}

void ASTPrinter::visitBinaryExpr(const BinaryExpr& node) {
  indent();
  out_ << "BinaryExpr op=" << binaryOpName(node.op()) << "\n";
  pushIndent();
  accept(*node.lhs(), *this);
  accept(*node.rhs(), *this);
  popIndent();
}

void ASTPrinter::visitCallExpr(const CallExpr& node) {
  indent();
  out_ << "CallExpr callee=" << node.calleeName() << "\n";
  pushIndent();
  if (!node.arguments().empty()) {
    indent();
    out_ << "Arguments\n";
    pushIndent();
    for (const auto& arg : node.arguments()) {
      accept(*arg, *this);
    }
    popIndent();
  }
  popIndent();
}

// ═══════════════════════════════════════════════════════════════════════════
// dumpAst
// ═══════════════════════════════════════════════════════════════════════════

void dumpAst(const CompUnit& unit, std::ostream& output) {
  ASTPrinter printer(output);
  printer.visitCompUnit(unit);
}

} // namespace toyc
