# Justfile — ToyC compiler build & test recipes

# Default build directory
build_dir := "build"
build_tests_dir := "build-tests"

# Configure without tests
configure:
  cmake -S . -B {{build_dir}}

# Configure with tests enabled
configure-tests:
  cmake -S . -B {{build_tests_dir}} -DTOYC_BUILD_TESTS=ON

# Build (no tests)
build: configure
  cmake --build {{build_dir}} -j

# Build with tests
build-tests: configure-tests
  cmake --build {{build_tests_dir}} -j

# Run frontend tests
frontend-test: build-tests
  {{build_tests_dir}}/toyc-frontend-tests

# Run sema tests
sema-test: build-tests
  {{build_tests_dir}}/toyc-sema-tests

# Run IR tests
ir-test: build-tests
  {{build_tests_dir}}/toyc-ir-tests

# Run lowering tests
lowering-test: build-tests
  {{build_tests_dir}}/toyc-lowering-tests

# Run MIR tests
mir-test: build-tests
  {{build_tests_dir}}/toyc-mir-tests

# Run RISC-V32 tests
riscv32-test: build-tests
  {{build_tests_dir}}/toyc-riscv32-tests

# Run codegen tests
codegen-test: build-tests
  {{build_tests_dir}}/toyc-codegen-tests

# Run all tests
test: frontend-test sema-test ir-test lowering-test mir-test riscv32-test codegen-test

# Clean build directories
clean:
  rm -rf {{build_dir}} {{build_tests_dir}}

# Format all source files (requires clang-format)
format:
  find include src tests -name '*.h' -o -name '*.cpp' | xargs clang-format -i

# Build with coverage instrumentation
coverage:
  cmake -S . -B {{build_tests_dir}} -DTOYC_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage"
  cmake --build {{build_tests_dir}} -j
  {{build_tests_dir}}/toyc-frontend-tests
  {{build_tests_dir}}/toyc-sema-tests
  {{build_tests_dir}}/toyc-ir-tests
  {{build_tests_dir}}/toyc-lowering-tests
  {{build_tests_dir}}/toyc-mir-tests
  {{build_tests_dir}}/toyc-riscv32-tests
  {{build_tests_dir}}/toyc-codegen-tests
