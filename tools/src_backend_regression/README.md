# src backend regression

This directory contains source-level ABI and backend regression cases for `src`.

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools/src_backend_regression/run.ps1
```

The runner:

- configures and builds `build/toycc.exe`
- compiles each `cases/*.tc` file to RISC-V assembly
- checks callee-saved save/restore and unresolved backend placeholders
- probes for a local RISC-V compiler and qemu runner
- records `lw`, `sw`, `call`, `mul`, `div`, and `rem` instruction counts
- writes `tools/src_backend_regression/reports/latest.md`
