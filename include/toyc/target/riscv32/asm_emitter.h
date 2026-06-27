#pragma once
/// RISC-V32 Assembly Emitter.
///
/// Takes a MIRModule and emits RISC-V32 assembly text.
/// Internally applies SpillAll expansion: every VReg is stored in a
/// frame slot, and each MIR instruction is expanded to load/operate/store
/// sequences using scratch registers.

#include <string>

namespace toyc {

struct MIRModule;

namespace riscv32 {

/// Emit RISC-V32 assembly for the given MIR module.
/// Returns the assembly text as a string.
std::string emitAssembly(const MIRModule& module);

} // namespace riscv32
} // namespace toyc
