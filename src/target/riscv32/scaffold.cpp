/// RISC-V32 backend scaffold — P0 stub implementations.

#include "toyc/target/riscv32/registers.h"
#include "toyc/target/riscv32/abi.h"
#include "toyc/target/riscv32/asm_emitter.h"
#include "toyc/ir/module.h"

namespace toyc::riscv32 {

// ── Register names ──────────────────────────────────────────────────────────

std::string_view regName(GPRegister reg) {
  switch (reg) {
    case ZERO: return "zero";
    case RA:   return "ra";
    case SP:   return "sp";
    case GP:   return "gp";
    case TP:   return "tp";
    case T0:   return "t0";
    case T1:   return "t1";
    case T2:   return "t2";
    case S0:   return "s0";
    case S1:   return "s1";
    case A0:   return "a0";
    case A1:   return "a1";
    case A2:   return "a2";
    case A3:   return "a3";
    case A4:   return "a4";
    case A5:   return "a5";
    case A6:   return "a6";
    case A7:   return "a7";
    case S2:   return "s2";
    case S3:   return "s3";
    case S4:   return "s4";
    case S5:   return "s5";
    case S6:   return "s6";
    case S7:   return "s7";
    case S8:   return "s8";
    case S9:   return "s9";
    case S10:  return "s10";
    case S11:  return "s11";
    case T3:   return "t3";
    case T4:   return "t4";
    case T5:   return "t5";
    case T6:   return "t6";
  }
  return "???";
}

// ── FrameLayout (P0 stub) ──────────────────────────────────────────────────

void FrameLayout::compute() {
  // P0 stub — frame layout computation not yet implemented.
  frameSize = 0;
}

// ── Assembly emitter (P0 stub) ─────────────────────────────────────────────

void emitAssembly(const Module& /*module*/, std::ostream& out) {
  out << "# RISC-V32 assembly emitter: P0 stub — no assembly generated\n";
}

} // namespace toyc::riscv32
