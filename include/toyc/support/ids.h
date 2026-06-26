#pragma once
/// Strongly-typed IDs used across all compiler phases.
/// Each ID wraps an integer and prevents accidental mixing of ID domains.

#include <cstdint>
#include <functional>

namespace toyc {

/// Macro to define a strongly-typed ID wrapper.
#define TOYC_DEFINE_ID(Name)                                        \
  struct Name {                                                     \
    uint32_t value = static_cast<uint32_t>(-1);                     \
    constexpr Name() = default;                                     \
    constexpr explicit Name(uint32_t v) : value(v) {}               \
    constexpr bool valid() const { return value != static_cast<uint32_t>(-1); } \
    constexpr bool operator==(const Name& o) const { return value == o.value; } \
    constexpr bool operator!=(const Name& o) const { return value != o.value; } \
    constexpr bool operator<(const Name& o) const { return value < o.value; } \
  };

/// Source file identifier.
TOYC_DEFINE_ID(SourceId)

/// Symbol table entry identifier.
TOYC_DEFINE_ID(SymbolId)

/// Slot IR slot identifier (mutable slots before Mem2Reg).
TOYC_DEFINE_ID(SlotId)

/// SSA Value identifier.
TOYC_DEFINE_ID(ValueId)

/// Instruction identifier.
TOYC_DEFINE_ID(InstId)

/// Basic block identifier.
TOYC_DEFINE_ID(BlockId)

/// Function identifier.
TOYC_DEFINE_ID(FunctionId)

/// Global variable / constant identifier.
TOYC_DEFINE_ID(GlobalId)

/// Virtual register identifier (MIR).
TOYC_DEFINE_ID(VRegId)

/// Scope identifier (semantic analysis).
TOYC_DEFINE_ID(ScopeId)

#undef TOYC_DEFINE_ID

} // namespace toyc

/// Hash support for use in containers.
namespace std {
template <> struct hash<toyc::SourceId>   { size_t operator()(const toyc::SourceId& id)   const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::SymbolId>   { size_t operator()(const toyc::SymbolId& id)   const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::SlotId>     { size_t operator()(const toyc::SlotId& id)     const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::ValueId>    { size_t operator()(const toyc::ValueId& id)    const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::InstId>     { size_t operator()(const toyc::InstId& id)     const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::BlockId>    { size_t operator()(const toyc::BlockId& id)    const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::FunctionId> { size_t operator()(const toyc::FunctionId& id) const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::GlobalId>   { size_t operator()(const toyc::GlobalId& id)   const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::VRegId>     { size_t operator()(const toyc::VRegId& id)     const noexcept { return hash<uint32_t>{}(id.value); } };
template <> struct hash<toyc::ScopeId>    { size_t operator()(const toyc::ScopeId& id)    const noexcept { return hash<uint32_t>{}(id.value); } };
} // namespace std
