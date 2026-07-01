#pragma once

#include <string_view>

enum class Type { Int, Void };

constexpr std::string_view to_string(Type t) {
    switch (t) {
    case Type::Int: return "i32";
    case Type::Void: return "void";
    }
    return "?";
}
