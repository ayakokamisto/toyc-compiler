#pragma once
/// IR-level types for the ToyC compiler.
/// ToyC has only i32 and void at the IR level.

#include <cstdint>
#include <string>

namespace toyc {

/// IR type kinds.
enum class IRTypeKind : uint8_t {
  I32,     ///< 32-bit signed integer.
  Void,    ///< No value.
  Label,   ///< Basic block label (for branch targets).
  Error,   ///< Placeholder for type errors.
};

/// An IR-level type.
struct IRType {
  IRTypeKind kind = IRTypeKind::Error;

  [[nodiscard]] bool isI32() const { return kind == IRTypeKind::I32; }
  [[nodiscard]] bool isVoid() const { return kind == IRTypeKind::Void; }
  [[nodiscard]] bool isLabel() const { return kind == IRTypeKind::Label; }

  [[nodiscard]] std::string toString() const;
};

inline constexpr IRType I32Type{IRTypeKind::I32};
inline constexpr IRType VoidIRType{IRTypeKind::Void};
inline constexpr IRType LabelType{IRTypeKind::Label};

} // namespace toyc
