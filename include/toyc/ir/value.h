#pragma once

#include "type.h"

#include <cstdint>
#include <string>

// =============================================================================
// Value — IR value base class.
//
// Identity = pointer identity (Value*). Two Values with the same display name
// but different addresses are distinct values, matching Java's object identity.
//
// The name_ field is a *display name* only — it must NOT be used for identity
// comparisons in passes (use pointer equality: &v1 == &v2).
// =============================================================================

class Value {
public:
    Value(Type type, std::string name);
    virtual ~Value() = default;

    // Non-copyable, non-movable (identity is the pointer).
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;
    Value(Value&&) = delete;
    Value& operator=(Value&&) = delete;

    Type type() const { return type_; }
    const std::string& name() const { return name_; }

private:
    Type type_;
    std::string name_;
};

// -----------------------------------------------------------------------------
// Temp — SSA temporary value, displayed as %t<index>.
// -----------------------------------------------------------------------------

class Temp : public Value {
public:
    Temp(uint32_t index, Type type)
        : Value(type, make_name(index)), index_(index) {}

    uint32_t index() const { return index_; }

private:
    static std::string make_name(uint32_t index) {
        return "%t" + std::to_string(index);
    }
    uint32_t index_;
};

// -----------------------------------------------------------------------------
// LocalVar — stack-allocated local variable slot.
// Displayed as %<sourceName>.<index>.
// -----------------------------------------------------------------------------

class LocalVar : public Value {
public:
    LocalVar(uint32_t index, std::string source_name, bool is_parameter)
        : Value(Type::Int, make_name(source_name, index)),
          source_name_(std::move(source_name)),
          index_(index),
          is_parameter_(is_parameter) {}

    const std::string& source_name() const { return source_name_; }
    uint32_t index() const { return index_; }
    bool is_parameter() const { return is_parameter_; }

private:
    static std::string make_name(const std::string& s, uint32_t i) {
        return "%" + s + "." + std::to_string(i);
    }

    std::string source_name_;
    uint32_t index_;
    bool is_parameter_;
};

// -----------------------------------------------------------------------------
// GlobalVar — global mutable variable.
// Displayed as @<name>.
// -----------------------------------------------------------------------------

class GlobalVar : public Value {
public:
    GlobalVar(std::string name, int32_t initial_value)
        : Value(Type::Int, "@" + name),
          symbol_name_(std::move(name)),
          initial_value_(initial_value) {}

    const std::string& symbol_name() const { return symbol_name_; }
    int32_t initial_value() const { return initial_value_; }

private:
    std::string symbol_name_;
    int32_t initial_value_;
};

// -----------------------------------------------------------------------------
// Constant — integer literal.
// Factory methods: Constant::of(value) for unnamed, Constant::named(...).
// NOT interned: each call creates a distinct Value (matching Java semantics).
// -----------------------------------------------------------------------------

class Constant : public Value {
public:
    static Constant* of(uint32_t id, int32_t value) {
        return new Constant(id, Type::Int, std::to_string(value), value, false);
    }

    static Constant* named(uint32_t id, const std::string& name, int32_t value) {
        return new Constant(id, Type::Int, "@" + name, value, true);
    }

    int32_t value() const { return value_; }
    bool is_named() const { return named_; }
    uint32_t id() const { return id_; }

private:
    Constant(uint32_t id, Type type, std::string name, int32_t value, bool named)
        : Value(type, std::move(name)), id_(id), value_(value), named_(named) {}

    uint32_t id_;
    int32_t value_;
    bool named_;
};

// -----------------------------------------------------------------------------
// Label — BasicBlock identifier.
// Displayed as <name>.
// -----------------------------------------------------------------------------

class Label : public Value {
public:
    explicit Label(std::string name)
        : Value(Type::Void, std::move(name)) {}
};
