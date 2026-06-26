#pragma once
/// ToyC type system.
/// ToyC only has int (32-bit signed) and void types.

#include <cstdint>
#include <string>

namespace toyc {

/// Built-in types in ToyC.
enum class BuiltinTypeKind : uint8_t {
  Int,    ///< 32-bit signed integer.
  Void,   ///< No value (function return type).
  Error,  ///< Placeholder for type errors.
};

/// A ToyC type (placeholder — will be expanded with arrays, pointers later).
struct Type {
  BuiltinTypeKind kind = BuiltinTypeKind::Error;

  [[nodiscard]] bool isInt() const { return kind == BuiltinTypeKind::Int; }
  [[nodiscard]] bool isVoid() const { return kind == BuiltinTypeKind::Void; }
  [[nodiscard]] bool isError() const { return kind == BuiltinTypeKind::Error; }

  [[nodiscard]] std::string toString() const;
};

/// Convenience constants.
inline constexpr Type IntType{BuiltinTypeKind::Int};
inline constexpr Type VoidType{BuiltinTypeKind::Void};

} // namespace toyc
