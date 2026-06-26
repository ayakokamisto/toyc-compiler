#pragma once
/// RISC-V 32 ABI conventions.
/// This is a P0 placeholder.

#include <cstdint>

namespace toyc::riscv32 {

/// Stack frame layout information (placeholder).
struct FrameLayout {
  int32_t localSize = 0;     ///< Space for local variables.
  int32_t spillSize = 0;     ///< Space for register spills.
  int32_t frameSize = 0;     ///< Total frame size (aligned to 16 bytes).

  /// Compute the frame layout for a function.
  /// P0 stub.
  void compute();
};

/// ABI constants.
inline constexpr int kStackAlignment = 16;
inline constexpr int kWordSize = 4;  ///< 32-bit = 4 bytes.

} // namespace toyc::riscv32
