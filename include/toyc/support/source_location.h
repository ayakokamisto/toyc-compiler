#pragma once
/// Source location and range for diagnostics and AST nodes.

#include <cstdint>

namespace toyc {

/// A position in source text (byte offset, 1-based line and column).
struct SourceLocation {
  uint32_t offset = 0;  ///< Byte offset from start of source.
  uint32_t line   = 1;  ///< 1-based line number.
  uint32_t column = 1;  ///< 1-based column number.

  constexpr SourceLocation() = default;
  constexpr SourceLocation(uint32_t off, uint32_t ln, uint32_t col)
      : offset(off), line(ln), column(col) {}

  constexpr bool operator==(const SourceLocation& o) const {
    return offset == o.offset;
  }
  constexpr bool operator<(const SourceLocation& o) const {
    return offset < o.offset;
  }
};

/// A half-open range [begin, end) in source text.
struct SourceRange {
  SourceLocation begin;
  SourceLocation end;

  constexpr SourceRange() = default;
  constexpr SourceRange(SourceLocation b, SourceLocation e)
      : begin(b), end(e) {}

  constexpr bool empty() const { return begin == end; }
};

} // namespace toyc
