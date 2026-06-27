#pragma once
/// RISC-V32 Assembly Emitter.
///
/// Takes an allocated RV32 machine module and emits RISC-V32 assembly text.

#include <string>

namespace toyc {

namespace riscv32 {

struct AllocatedMachineModule;

/// Emit RISC-V32 assembly for the given MIR module.
/// Returns the assembly text as a string.
std::string emitAssembly(const AllocatedMachineModule& module);

} // namespace riscv32
} // namespace toyc
