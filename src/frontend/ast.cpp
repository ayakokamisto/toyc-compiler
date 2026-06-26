/// AST implementation — enum names and visitor dispatch.

#include "toyc/frontend/ast.h"

namespace toyc {

// ═══════════════════════════════════════════════════════════════════════════
// Enum name functions
// ═══════════════════════════════════════════════════════════════════════════

const char* typeKindName(TypeKind k) {
  switch (k) {
    case TypeKind::Int:  return "int";
    case TypeKind::Void: return "void";
  }
  return "unknown";
}

const char* unaryOpName(UnaryOperator op) {
  switch (op) {
    case UnaryOperator::Plus:        return "Plus";
    case UnaryOperator::Minus:       return "Minus";
    case UnaryOperator::LogicalNot:  return "LogicalNot";
  }
  return "unknown";
}

const char* binaryOpName(BinaryOperator op) {
  switch (op) {
    case BinaryOperator::LogicalOr:      return "LogicalOr";
    case BinaryOperator::LogicalAnd:     return "LogicalAnd";
    case BinaryOperator::Equal:          return "Equal";
    case BinaryOperator::NotEqual:       return "NotEqual";
    case BinaryOperator::Less:           return "Less";
    case BinaryOperator::LessEqual:      return "LessEqual";
    case BinaryOperator::Greater:        return "Greater";
    case BinaryOperator::GreaterEqual:   return "GreaterEqual";
    case BinaryOperator::Add:            return "Add";
    case BinaryOperator::Subtract:       return "Subtract";
    case BinaryOperator::Multiply:       return "Multiply";
    case BinaryOperator::Divide:         return "Divide";
    case BinaryOperator::Modulo:         return "Modulo";
  }
  return "unknown";
}

// ═══════════════════════════════════════════════════════════════════════════
// Visitor dispatch
// ═══════════════════════════════════════════════════════════════════════════

void accept(const ASTNode& node, ASTVisitor& visitor) {
  switch (node.kind()) {
    case ASTKind::CompUnit:
      visitor.visitCompUnit(static_cast<const CompUnit&>(node)); break;
    case ASTKind::GlobalDecl:
      visitor.visitGlobalDecl(static_cast<const GlobalDecl&>(node)); break;
    case ASTKind::FuncDef:
      visitor.visitFuncDef(static_cast<const FuncDef&>(node)); break;
    case ASTKind::ConstDecl:
      visitor.visitConstDecl(static_cast<const ConstDecl&>(node)); break;
    case ASTKind::VarDecl:
      visitor.visitVarDecl(static_cast<const VarDecl&>(node)); break;
    case ASTKind::ParamDecl:
      visitor.visitParamDecl(static_cast<const ParamDecl&>(node)); break;
    case ASTKind::BlockStmt:
      visitor.visitBlockStmt(static_cast<const BlockStmt&>(node)); break;
    case ASTKind::EmptyStmt:
      visitor.visitEmptyStmt(static_cast<const EmptyStmt&>(node)); break;
    case ASTKind::ExprStmt:
      visitor.visitExprStmt(static_cast<const ExprStmt&>(node)); break;
    case ASTKind::AssignStmt:
      visitor.visitAssignStmt(static_cast<const AssignStmt&>(node)); break;
    case ASTKind::DeclStmt:
      visitor.visitDeclStmt(static_cast<const DeclStmt&>(node)); break;
    case ASTKind::IfStmt:
      visitor.visitIfStmt(static_cast<const IfStmt&>(node)); break;
    case ASTKind::WhileStmt:
      visitor.visitWhileStmt(static_cast<const WhileStmt&>(node)); break;
    case ASTKind::BreakStmt:
      visitor.visitBreakStmt(static_cast<const BreakStmt&>(node)); break;
    case ASTKind::ContinueStmt:
      visitor.visitContinueStmt(static_cast<const ContinueStmt&>(node)); break;
    case ASTKind::ReturnStmt:
      visitor.visitReturnStmt(static_cast<const ReturnStmt&>(node)); break;
    case ASTKind::IntegerLiteralExpr:
      visitor.visitIntegerLiteralExpr(static_cast<const IntegerLiteralExpr&>(node)); break;
    case ASTKind::IdentifierExpr:
      visitor.visitIdentifierExpr(static_cast<const IdentifierExpr&>(node)); break;
    case ASTKind::UnaryExpr:
      visitor.visitUnaryExpr(static_cast<const UnaryExpr&>(node)); break;
    case ASTKind::BinaryExpr:
      visitor.visitBinaryExpr(static_cast<const BinaryExpr&>(node)); break;
    case ASTKind::CallExpr:
      visitor.visitCallExpr(static_cast<const CallExpr&>(node)); break;
  }
}

} // namespace toyc
