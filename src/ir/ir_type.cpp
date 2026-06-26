/// IRType implementation.

#include "toyc/ir/ir_type.h"

namespace toyc {

std::string IRType::toString() const {
  switch (kind) {
    case IRTypeKind::I32:   return "i32";
    case IRTypeKind::Void:  return "void";
    case IRTypeKind::Label: return "label";
    case IRTypeKind::Error: return "<error>";
  }
  return "<unknown>";
}

} // namespace toyc
