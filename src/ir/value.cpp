#include "toyc/ir/value.h"

Value::Value(Type type, std::string name)
    : type_(type), name_(std::move(name)) {}
