#pragma once
/// RISC-V 32 register definitions.

#include <cstdint>
#include <string_view>

namespace toyc::riscv32 {

/// RISC-V 32-bit general-purpose registers.
enum GPRegister : uint8_t {
  ZERO = 0,  // x0  — hardwired zero
  RA   = 1,  // x1  — return address
  SP   = 2,  // x2  — stack pointer
  GP   = 3,  // x3  — global pointer
  TP   = 4,  // x4  — thread pointer
  T0   = 5,  // x5  — temporaries
  T1   = 6,  // x6
  T2   = 7,  // x7
  S0   = 8,  // x8  — saved / frame pointer (FP)
  S1   = 9,  // x9  — saved
  A0   = 10, // x10 — arguments / return values
  A1   = 11, // x11
  A2   = 12, // x12
  A3   = 13, // x13
  A4   = 14, // x14
  A5   = 15, // x15
  A6   = 16, // x16
  A7   = 17, // x17
  S2   = 18, // x18 — saved
  S3   = 19, // x19
  S4   = 20, // x20
  S5   = 21, // x21
  S6   = 22, // x22
  S7   = 23, // x23
  S8   = 24, // x24
  S9   = 25, // x25
  S10  = 26, // x26
  S11  = 27, // x27
  T3   = 28, // x28 — temporaries
  T4   = 29, // x29
  T5   = 30, // x30
  T6   = 31, // x31
};

/// Get the ABI name of a register (e.g., "a0", "sp").
std::string_view regName(GPRegister reg);

/// Number of argument registers available for parameter passing.
inline constexpr int kArgRegs = 8;  // a0–a7

/// Caller-saved register count.
inline constexpr int kCallerSaved = 15;  // t0–t6, a0–a7, ra

/// Callee-saved register count.
inline constexpr int kCalleeSaved = 12;  // s0–s11

} // namespace toyc::riscv32
