/// Scope and ScopeStack tests — P3 verification.

#include "toyc/sema/scope.h"

#include <gtest/gtest.h>

namespace toyc {

TEST(ScopeTest, GlobalScopeCreated) {
  ScopeStack scopes;
  EXPECT_TRUE(scopes.isGlobalScope());
}

TEST(ScopeTest, PushCreatesChildScope) {
  ScopeStack scopes;
  auto child = scopes.pushScope();
  EXPECT_TRUE(child.valid());
  EXPECT_FALSE(scopes.isGlobalScope());
}

TEST(ScopeTest, PopReturnsToGlobal) {
  ScopeStack scopes;
  scopes.pushScope();
  scopes.popScope();
  EXPECT_TRUE(scopes.isGlobalScope());
}

TEST(ScopeTest, DeclareAndLookupCurrent) {
  ScopeStack scopes;
  SymbolId id(42);
  EXPECT_TRUE(scopes.declare("x", id));

  auto found = scopes.lookupCurrent("x");
  EXPECT_TRUE(found.has_value());
  EXPECT_EQ(found.value(), id);
}

TEST(ScopeTest, LookupCurrentMissesParent) {
  ScopeStack scopes;
  SymbolId id(42);
  scopes.declare("x", id);

  scopes.pushScope();
  auto found = scopes.lookupCurrent("x");
  EXPECT_FALSE(found.has_value());
}

TEST(ScopeTest, LookupVisibleFindsParent) {
  ScopeStack scopes;
  SymbolId id(42);
  scopes.declare("x", id);

  scopes.pushScope();
  auto found = scopes.lookupVisible("x");
  EXPECT_TRUE(found.has_value());
  EXPECT_EQ(found.value(), id);
}

TEST(ScopeTest, InnerShadowsOuter) {
  ScopeStack scopes;
  SymbolId outer(1);
  scopes.declare("x", outer);

  scopes.pushScope();
  SymbolId inner(2);
  scopes.declare("x", inner);

  auto found = scopes.lookupVisible("x");
  EXPECT_TRUE(found.has_value());
  EXPECT_EQ(found.value(), inner);
}

TEST(ScopeTest, DuplicateDeclarationFails) {
  ScopeStack scopes;
  SymbolId id1(1);
  SymbolId id2(2);
  EXPECT_TRUE(scopes.declare("x", id1));
  EXPECT_FALSE(scopes.declare("x", id2));
}

TEST(ScopeTest, NestedScopesMultipleLevels) {
  ScopeStack scopes;
  SymbolId global(1);
  scopes.declare("g", global);

  scopes.pushScope();
  SymbolId func(2);
  scopes.declare("f", func);

  scopes.pushScope();
  SymbolId block(3);
  scopes.declare("b", block);

  // Innermost sees all.
  EXPECT_EQ(scopes.lookupVisible("b").value(), block);
  EXPECT_EQ(scopes.lookupVisible("f").value(), func);
  EXPECT_EQ(scopes.lookupVisible("g").value(), global);

  scopes.popScope();
  EXPECT_FALSE(scopes.lookupCurrent("b").has_value());
  EXPECT_EQ(scopes.lookupVisible("f").value(), func);

  scopes.popScope();
  EXPECT_EQ(scopes.lookupVisible("g").value(), global);
}

} // namespace toyc
