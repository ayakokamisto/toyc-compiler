#pragma once
/// Module — the top-level IR container.
/// Holds all functions and global definitions for one compilation unit.

#include "toyc/ir/function.h"
#include "toyc/support/ids.h"

#include <memory>
#include <string>
#include <vector>

namespace toyc {

/// A global variable or constant definition.
struct GlobalDef {
  GlobalId id;
  std::string name;
  IRType type;
  bool isConst = false;
  int64_t initValue = 0;  ///< Compile-time initial value (for constants & initialized globals).
};

/// The top-level IR module.
class Module {
public:
  Module() = default;

  /// Create a new function in this module.
  Function* createFunction(std::string name, IRType returnType);

  /// Create a global definition.
  GlobalId createGlobal(std::string name, IRType type, bool isConst, int64_t initValue);

  /// Accessors.
  [[nodiscard]] const std::vector<std::unique_ptr<Function>>& functions() const { return funcs_; }
  [[nodiscard]] const std::vector<GlobalDef>& globals() const { return globals_; }

private:
  std::vector<std::unique_ptr<Function>> funcs_;
  std::vector<GlobalDef> globals_;
  uint32_t nextFuncId_ = 0;
  uint32_t nextGlobalId_ = 0;
};

} // namespace toyc
