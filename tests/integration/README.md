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
- CodeGen integration can compile, assemble, execute, and compare the process exit code
  with `.expected` through `run_toyc_exec_cases.sh`.
- The execution script is intentionally not registered in CTest, so environments without
  a RISC-V GCC and `qemu-riscv32` can still run the normal test suite.

Execution check:

```powershell
cmake --build build --target toycc
```

```bash
bash tests/integration/run_toyc_exec_cases.sh --build-dir build
```

Function calls follow ToyC source-order rules: a callee is defined before its caller;
self-recursion is permitted.
