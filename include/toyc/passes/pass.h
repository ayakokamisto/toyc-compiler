#pragma once
/// Pass interface — base class for all optimization and transformation passes.

#include <string>

namespace toyc {

class Module;

/// Base class for all compiler passes.
class Pass {
public:
  virtual ~Pass() = default;

  /// Human-readable name of this pass.
  [[nodiscard]] virtual const char* name() const = 0;

  /// Run this pass on the given module.
  /// Returns true if the module was modified.
  virtual bool run(Module& module) = 0;
};

} // namespace toyc
