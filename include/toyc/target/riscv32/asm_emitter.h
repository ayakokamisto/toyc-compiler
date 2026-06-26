#pragma once
/// RISC-V32 assembly emitter.
/// Writes RISC-V32 assembly to an output stream.
/// This is a P0 placeholder — real emission will be implemented in P5+.

#include <ostream>
#include <string>

namespace toyc {

class Module;

namespace riscv32 {

/// Emit RISC-V32 assembly for the given IR module.
/// P0 stub: writes a comment to the output.
void emitAssembly(const Module& module, std::ostream& out);

} // namespace riscv32
} // namespace toyc
