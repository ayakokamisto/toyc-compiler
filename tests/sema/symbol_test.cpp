/// Symbol tests — P3 verification.

#include "toyc/sema/symbol.h"

#include <gtest/gtest.h>

namespace toyc {

TEST(SymbolTest, GlobalVariable) {
  Symbol sym;
  sym.name = "x";
  sym.kind = SymbolKind::GlobalVariable;
  sym.type = TypeKind::Int;
  EXPECT_TRUE(sym.isVariable());
  EXPECT_FALSE(sym.isConstant());
  EXPECT_TRUE(sym.isAssignable());
}

TEST(SymbolTest, GlobalConstant) {
  Symbol sym;
  sym.name = "c";
  sym.kind = SymbolKind::GlobalConstant;
  sym.type = TypeKind::Int;
  sym.constantValue = 42;
  EXPECT_FALSE(sym.isVariable());
  EXPECT_TRUE(sym.isConstant());
  EXPECT_FALSE(sym.isAssignable());
}

TEST(SymbolTest, LocalVariable) {
  Symbol sym;
  sym.kind = SymbolKind::LocalVariable;
  EXPECT_TRUE(sym.isVariable());
  EXPECT_TRUE(sym.isAssignable());
}

TEST(SymbolTest, LocalConstant) {
  Symbol sym;
  sym.kind = SymbolKind::LocalConstant;
  EXPECT_TRUE(sym.isConstant());
  EXPECT_FALSE(sym.isAssignable());
}

TEST(SymbolTest, Parameter) {
  Symbol sym;
  sym.kind = SymbolKind::Parameter;
  EXPECT_FALSE(sym.isVariable());
  EXPECT_FALSE(sym.isConstant());
  EXPECT_TRUE(sym.isAssignable());
}

TEST(SymbolTest, Function) {
  Symbol sym;
  sym.kind = SymbolKind::Function;
  sym.type = TypeKind::Int;
  sym.parameterTypes = {TypeKind::Int, TypeKind::Int};
  sym.parameterNames = {"a", "b"};
  EXPECT_FALSE(sym.isVariable());
  EXPECT_FALSE(sym.isConstant());
  EXPECT_FALSE(sym.isAssignable());
}

} // namespace toyc
