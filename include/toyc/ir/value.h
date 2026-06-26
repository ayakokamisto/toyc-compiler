#pragma once
/// SSA Value — the base of the IR value hierarchy.
///
/// Every SSA entity (instruction result, argument, constant) is a Value
/// identified by a unique ValueId. Def-use chains are maintained via Use.

#include "toyc/ir/ir_type.h"
#include "toyc/support/ids.h"

#include <cstdint>
#include <string>

namespace toyc {

class Use;

/// Base class for all SSA values (instructions, constants, arguments).
class Value {
public:
  Value() = default;
  virtual ~Value() = default;

  [[nodiscard]] ValueId id() const { return id_; }
  void setId(ValueId v) { id_ = v; }

  [[nodiscard]] IRType type() const { return type_; }
  void setType(IRType t) { type_ = t; }

  /// Use-list head (intrusive linked list).
  [[nodiscard]] Use* useList() const { return useList_; }
  void addUse(Use* u);
  void removeUse(Use* u);

  [[nodiscard]] virtual std::string toString() const;

protected:
  ValueId id_;
  IRType type_;
  Use* useList_ = nullptr;
};

} // namespace toyc
