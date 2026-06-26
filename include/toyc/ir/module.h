#pragma once
/// Module — the top-level IR container.
/// Holds all functions and global definitions for one compilation unit.

#include "toyc/ir/function.h"
#include "toyc/ir/ir_type.h"
#include "toyc/support/ids.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace toyc {

/// Global variable or constant kind.
enum class GlobalKind : uint8_t {
  Variable,       ///< Mutable global variable.
  Constant,       ///< Compile-time constant.
  InternalGuard,  ///< Internal guard for runtime init.
};

/// How a global is initialized at the IR level.
enum class IRGlobalInitKind : uint8_t {
  Static,                ///< Value known at compile time.
  RuntimeZeroInitialized, ///< Initialized at runtime (starts as zero).
};

/// A global definition in the module.
struct IRGlobal {
  GlobalId id;
  std::optional<SymbolId> sourceSymbol;  ///< None for internal globals.
  std::string name;
  IRType type = I32Type;
  GlobalKind kind = GlobalKind::Variable;
  IRGlobalInitKind initKind = IRGlobalInitKind::Static;
  int32_t staticInitialValue = 0;
  bool isInternal = false;
};

/// The top-level IR module.
class Module {
public:
  Module() = default;

  /// Create a new function in this module.
  Function* createFunction(std::string name, IRType returnType);

  /// Create a global definition.
  GlobalId createGlobal(IRGlobal global);

  /// Accessors.
  [[nodiscard]] const std::vector<std::unique_ptr<Function>>& functions() const { return funcs_; }
  [[nodiscard]] const std::vector<IRGlobal>& globals() const { return globals_; }

  /// Find a global by id.
  [[nodiscard]] const IRGlobal* findGlobal(GlobalId id) const;

  /// Find a function by id.
  [[nodiscard]] const Function* findFunction(FunctionId id) const;

  /// Find a function by name.
  [[nodiscard]] Function* findFunctionByName(const std::string& name);

private:
  std::vector<std::unique_ptr<Function>> funcs_;
  std::vector<IRGlobal> globals_;
  uint32_t nextFuncId_ = 0;
  uint32_t nextGlobalId_ = 0;
};

} // namespace toyc
