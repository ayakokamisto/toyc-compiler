# Repository Instructions

## Testing Policy

- Do not use CTest in this repository.
- Do not add `enable_testing()`, `add_test()`, `include(GoogleTest)`, or `gtest_discover_tests()` to CMake.
- All tests must be GoogleTest binaries run directly.
- The canonical frontend test command is `build/toyc-frontend-tests`.
- The canonical IR/backend test command is `build/toyc-ir-tests`.
- Common build, run, test, and coverage workflows are recorded in the root `Justfile`.

## Build Configuration

- `TOYC_BUILD_TESTS` is a CMake option, default `OFF`.
- Configure with tests enabled:
  ```bash
  cmake -S . -B build -DTOYC_BUILD_TESTS=ON
  cmake --build build -j
  ```
- GTest must be resolved **only** via `find_package(GTest REQUIRED)` against the system/user installation.
- Do **not** use `FetchContent`, `ExternalProject`, `Git submodule`, or any automatic download mechanism for GTest.
- The test dependency must already be installed locally before configuring with `-DTOYC_BUILD_TESTS=ON`.

## Document Priority

When requirements or behavior conflict across documents, follow this precedence:

1. `任务要求.md` — authoritative language & grading specification
2. `docs/architecture.md` — architecture and design decisions
3. `AGENTS.md` — project constraints and agent rules
4. `CLAUDE.md` — toolchain and build instructions
5. `词汇翻译表.md` — lexer/parser token reference
