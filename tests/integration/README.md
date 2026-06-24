# Integration Cases

Each regression case consists of:

```text
cases/<name>.tc
cases/<name>.expected
```

An `.expected` file contains one decimal integer followed by a newline. The value is the
expected `main` exit code after executing generated code and must be between 0 and 255.

Current phase:

- `.tc` and `.expected` files define the end-to-end regression specification.
- Parser integration verifies that every `.tc` file is syntactically accepted.
- Sema integration will verify that every case passes semantic analysis.
- Default CTest integration invokes `toycc` and checks that each `.tc` case succeeds,
  emits non-empty assembly, and contains `main:`.
- Default CTest does not assemble, link, run, or compare the `main` exit code.
- RISC-V execution integration can compile, assemble, execute, and compare the process
  exit code with `.expected` through `run_toyc_exec_cases.sh`.
- The execution script is opt-in for CTest, so environments without a RISC-V GCC and
  `qemu-riscv32` can still run the normal test suite.

Execution check:

Manual WSL/Unix execution:

```powershell
cmake --build build --target toycc
```

```bash
bash tests/integration/run_toyc_exec_cases.sh --build-dir build
```

The script writes per-case logs under `<build-dir>/toyc-exec-cases/`:

- `*.cmd`: command line
- `*.stdout`: captured stdout
- `*.stderr`: captured stderr
- `*.exit`: process exit code

`--timeout SECONDS` applies to each `toycc`, RISC-V GCC, and QEMU command.
`--timeout-tool PATH` can pin the GNU/coreutils-compatible timeout executable used
by the script. Failures are reported as command timeout, tool failure, QEMU
startup failure, or target exit code mismatch.

Optional CTest execution:

```bash
cmake -S . -B build -G Ninja -DTOYC_ENABLE_RISCV_EXEC_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

When `TOYC_ENABLE_RISCV_EXEC_TESTS=ON`, CMake looks for `bash`,
`riscv64-linux-gnu-gcc`, and `qemu-riscv32`. If any are missing, configuration
fails with a tool-specific message. On Windows, prefer configuring and running
this option inside WSL; native PowerShell CTest is not assumed to have a usable
bash/RISC-V toolchain path.

`-opt` codegen measurement:

```bash
python3 tests/integration/measure_codegen_opt.py --build-dir build
```

Use `--no-run` to generate only the default/`-opt` assembly statistics without
requiring the RISC-V GCC and `qemu-riscv32` execution chain.

Current reproducible static optimization metrics are based on `lw`, `sw`, and
per-case `lw+sw`: 17/17 cases reduce `lw`, aggregate `lw` changes from 350 to
238, aggregate `sw` changes from 330 to 304, and `lw+sw increases: none`.

Function calls follow ToyC source-order rules: a callee is defined before its caller;
self-recursion is permitted.
