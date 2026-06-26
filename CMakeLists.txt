cmake_minimum_required(VERSION 3.28.3)
project(toyc_compiler LANGUAGES CXX)

# ── Global settings ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ──────────────────────────────────────────────────────────────────
option(TOYC_BUILD_TESTS "Build local GoogleTest binaries" OFF)
option(TOYC_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# ── Compiler warnings ────────────────────────────────────────────────────────
include(cmake/CompilerWarnings.cmake)

# ── Output binaries to build/ root ──────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ── Include path (public for all targets) ───────────────────────────────────
set(TOYC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ── Helper: create a library with standard settings ─────────────────────────
function(toyc_add_library name)
  add_library(${name} STATIC ${ARGN})
  target_include_directories(${name} PUBLIC ${TOYC_INCLUDE_DIR})
  toyc_set_warnings(${name})
endfunction()

# ── Libraries (single-direction dependency chain) ───────────────────────────

# support (base, no deps)
toyc_add_library(toyc_support
  src/support/diagnostics.cpp
  src/driver/options.cpp
)

# frontend → support
toyc_add_library(toyc_frontend
  src/frontend/token.cpp
  src/frontend/lexer.cpp
  src/frontend/ast.cpp
  src/frontend/ast_printer.cpp
  src/frontend/parser.cpp
)
target_link_libraries(toyc_frontend PUBLIC toyc_support)

# sema → frontend
toyc_add_library(toyc_sema
  src/sema/semantic_model.cpp
  src/sema/scope.cpp
  src/sema/constant_evaluator.cpp
  src/sema/semantic_analyzer.cpp
  src/sema/semantic_model_printer.cpp
)
target_link_libraries(toyc_sema PUBLIC toyc_frontend)

# ir → support
toyc_add_library(toyc_ir
  src/ir/scaffold.cpp
)
target_link_libraries(toyc_ir PUBLIC toyc_support)

# analysis → ir
toyc_add_library(toyc_analysis
  src/analysis/scaffold.cpp
)
target_link_libraries(toyc_analysis PUBLIC toyc_ir)

# passes → analysis
toyc_add_library(toyc_passes
  src/passes/scaffold.cpp
)
target_link_libraries(toyc_passes PUBLIC toyc_analysis)

# mir → support
toyc_add_library(toyc_mir
  src/mir/scaffold.cpp
)
target_link_libraries(toyc_mir PUBLIC toyc_support)

# riscv32 → mir
toyc_add_library(toyc_riscv32
  src/target/riscv32/scaffold.cpp
)
target_link_libraries(toyc_riscv32 PUBLIC toyc_mir)

# ── Main executable ─────────────────────────────────────────────────────────
add_executable(toycc
  src/driver/main.cpp
  src/driver/compiler_pipeline.cpp
)
target_link_libraries(toycc PRIVATE
  toyc_frontend
  toyc_sema
  toyc_ir
  toyc_analysis
  toyc_passes
  toyc_mir
  toyc_riscv32
)
toyc_set_warnings(toycc)

# ── Tests (only when TOYC_BUILD_TESTS=ON) ──────────────────────────────────
if(TOYC_BUILD_TESTS)
  find_package(GTest REQUIRED)

  add_executable(toyc-frontend-tests
    tests/frontend/frontend_smoke_test.cpp
    tests/frontend/token_test.cpp
    tests/frontend/lexer_test.cpp
    tests/frontend/lexer_error_test.cpp
    tests/frontend/driver_dump_tokens_test.cpp
    tests/frontend/ast_test.cpp
    tests/frontend/parser_test.cpp
    tests/frontend/parser_error_test.cpp
    tests/frontend/driver_dump_ast_test.cpp
  )
  target_link_libraries(toyc-frontend-tests PRIVATE
    GTest::gtest_main
    toyc_frontend
  )
  toyc_set_warnings(toyc-frontend-tests)

  add_executable(toyc-ir-tests
    tests/ir/ir_smoke_test.cpp
  )
  target_link_libraries(toyc-ir-tests PRIVATE
    GTest::gtest_main
    toyc_ir
  )
  toyc_set_warnings(toyc-ir-tests)

  add_executable(toyc-sema-tests
    tests/sema/scope_test.cpp
    tests/sema/symbol_test.cpp
    tests/sema/constant_evaluator_test.cpp
    tests/sema/semantic_analyzer_test.cpp
    tests/sema/semantic_error_test.cpp
    tests/sema/control_flow_test.cpp
    tests/sema/driver_dump_sema_test.cpp
  )
  target_link_libraries(toyc-sema-tests PRIVATE
    GTest::gtest_main
    toyc_sema
  )
  toyc_set_warnings(toyc-sema-tests)
endif()
