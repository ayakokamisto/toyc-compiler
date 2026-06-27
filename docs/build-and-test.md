# Build and Test

## P6 Commands

```bash
./build/toycc --dump-ssa < input.tc
./build/toyc-analysis-tests
./build/toyc-ssa-tests
```

Full local validation:

```bash
./build/toyc-frontend-tests
./build/toyc-sema-tests
./build/toyc-ir-tests
./build/toyc-analysis-tests
./build/toyc-lowering-tests
./build/toyc-ssa-tests
./build/toyc-mir-tests
./build/toyc-riscv32-tests
./build/toyc-codegen-tests
```

## Prerequisites

- C++20 compiler (GCC 13+ or Clang 16+)
- CMake 3.28+ (or 4.0.3 for OJ alignment)
- GoogleTest (installed locally, for tests only)

## Configure and Build

```bash
# Basic build (no tests)
cmake -S . -B build
cmake --build build -j

# With tests
cmake -S . -B build -DTOYC_BUILD_TESTS=ON
cmake --build build -j
```

## Run Compiler

```bash
# Show help
./build/toycc --help

# Token dump (P1 — lexer debug mode, output to stderr)
./build/toycc --dump-tokens < input.tc

# AST dump (P2 — parser debug mode, output to stderr)
./build/toycc --dump-ast < input.tc

# Semantic model dump (P3 — sema debug mode, output to stderr)
./build/toycc --dump-sema < input.tc

# IR dump (P4 — IR debug mode, output to stderr)
./build/toycc --dump-ir < input.tc

# Compile ToyC to RISC-V32 assembly (RV32 backend not yet implemented)
./build/toycc < input.tc > output.s

# With optimization flag
./build/toycc -opt < input.tc > output.s

# Combined flags
./build/toycc -opt --dump-ast < input.tc
```

## Run Tests

```bash
# Frontend tests
./build/toyc-frontend-tests

# Sema tests
./build/toyc-sema-tests

# IR tests
./build/toyc-ir-tests

# Lowering tests
./build/toyc-lowering-tests

# All tests
./build/toyc-frontend-tests && ./build/toyc-sema-tests && ./build/toyc-ir-tests && ./build/toyc-lowering-tests
```

## Justfile Recipes

```bash
just configure          # Configure without tests
just configure-tests    # Configure with tests
just build              # Build without tests
just build-tests        # Build with tests
just frontend-test      # Run frontend tests
just ir-test            # Run IR tests
just test               # Run all tests
just clean              # Remove build directories
just format             # Format source with clang-format
just coverage           # Build with coverage and run tests
```

## Warnings as Errors

```bash
cmake -S . -B build -DTOYC_WARNINGS_AS_ERRORS=ON
cmake --build build -j
```
