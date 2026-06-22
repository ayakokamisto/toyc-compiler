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
- CodeGen integration will compile, assemble, execute, and compare the process exit code
  with `.expected`.

Function calls follow ToyC source-order rules: a callee is defined before its caller;
self-recursion is permitted.
