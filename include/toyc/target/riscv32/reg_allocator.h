#pragma once
/// Linear-scan register allocator for RV32I.
///
/// Assigns VRegs to s1-s11 and leaves the rest on VRegHome stack slots.

#include "toyc/mir/mir.h"
#include "toyc/target/riscv32/spill_all_allocator.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace toyc::riscv32 {

/// VReg → physical register name (e.g. "t2", "s1") or nullopt if spilled.
using RegAssignment = std::unordered_map<uint32_t, std::string>;

struct AllocatedMachineModule;

class RegisterAllocator {
public:
  explicit RegisterAllocator(bool enable) : enableOpt_(enable) {}

  AllocatedMachineModule allocate(MIRModule module);

private:
  bool enableOpt_;
};

} // namespace toyc::riscv32
