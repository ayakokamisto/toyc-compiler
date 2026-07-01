# Triple Diff Framework

Compares Java reference, `src2`, and a configurable new C++ compiler by assembly shape and Spike behavior.

Default inputs:

- Java root: `toy-c-compiler-master`
- `src2` root: `src2`
- new C++ compiler: `build/toycc.exe`
- cases: Java `smoke` and `bench` test resources

The new C++ compiler is optional for the current audit stage. If it is missing, the row is recorded as `SKIP` and Java/src2 comparison still runs.

Run from repo root:

```powershell
powershell -ExecutionPolicy Bypass -File tools\triple_diff\run.ps1
```

Useful options:

```powershell
powershell -ExecutionPolicy Bypass -File tools\triple_diff\run.ps1 -NoBuildJava -NoBuildSrc2
powershell -ExecutionPolicy Bypass -File tools\triple_diff\run.ps1 -NewCppCompiler build\toycc.exe
powershell -ExecutionPolicy Bypass -File tools\triple_diff\run.ps1 -CasesDir toy-c-compiler-master\src\test\resources\bench
```

Outputs:

- `tools/triple_diff/reports/latest.md`
- `tools/triple_diff/reports/runtime/rows.psv`
- per-case `.s`, `.elf`, `.compile.log`, `.link.log`, `.spike.log`, `.result.txt`

The WSL runtime expects:

- `riscv64-unknown-elf-gcc`
- `$HOME/.local/bin/spike`
- `/tmp/rvtest_start.s`
- `/tmp/rvtest_link.ld`

This framework is audit infrastructure. It does not modify Java, `src2`, or the new C++ compiler.
