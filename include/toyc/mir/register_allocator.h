#pragma once
/// Register allocation interface.
/// This is a P0 placeholder — will be implemented in P8.

#include <cstdint>

namespace toyc {

struct MIRFunction;

/// Register allocator base class.
class RegisterAllocator {
public:
  virtual ~RegisterAllocator() = default;

  /// Allocate registers for the given MIR function.
  /// P0 stub: does nothing.
  virtual void allocate(MIRFunction& func) = 0;
};

/// Simple linear scan allocator (P0 placeholder).
class LinearScanAllocator : public RegisterAllocator {
public:
  void allocate(MIRFunction& func) override;
};

} // namespace toyc
