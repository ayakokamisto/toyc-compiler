cmake_minimum_required(VERSION 3.20)

project(toyc_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(toyc_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(toyc_frontend
    src/lexer/lexer.cpp
)

target_include_directories(toyc_frontend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

toyc_apply_warnings(toyc_frontend)

add_library(toyc_codegen
    src/codegen/RiscvBackend.cpp
    src/codegen/RiscvEmitter.cpp
    src/codegen/StackFrame.cpp
    src/codegen/CallingConvention.cpp
    src/codegen/InstructionSelector.cpp
    src/codegen/VRegCollector.cpp
    src/codegen/FunctionEmitter.cpp
)

target_include_directories(toyc_codegen
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

toyc_apply_warnings(toyc_codegen)

add_executable(toycc
    src/driver/main.cpp
)

target_link_libraries(toycc PRIVATE toyc_frontend)
toyc_apply_warnings(toycc)

enable_testing()

add_executable(lexer_tests
    tests/frontend/lexer_tests.cpp
)

target_link_libraries(lexer_tests PRIVATE toyc_frontend)
toyc_apply_warnings(lexer_tests)

add_test(NAME lexer_tests COMMAND lexer_tests)

add_executable(token_stream_tests
    tests/frontend/token_stream_tests.cpp
)

target_link_libraries(token_stream_tests PRIVATE toyc_frontend)
toyc_apply_warnings(token_stream_tests)

add_test(NAME token_stream_tests COMMAND token_stream_tests)

add_executable(codegen_stack_frame_tests
    tests/unit/codegen_stack_frame_tests.cpp
)

target_link_libraries(codegen_stack_frame_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_stack_frame_tests)

add_test(NAME codegen_stack_frame_tests COMMAND codegen_stack_frame_tests)

add_executable(codegen_backend_smoke_tests
    tests/unit/codegen_backend_smoke_tests.cpp
)

target_link_libraries(codegen_backend_smoke_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_backend_smoke_tests)

add_test(NAME codegen_backend_smoke_tests COMMAND codegen_backend_smoke_tests)

add_executable(codegen_calling_convention_tests
    tests/unit/codegen_calling_convention_tests.cpp
)

target_link_libraries(codegen_calling_convention_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_calling_convention_tests)

add_test(NAME codegen_calling_convention_tests COMMAND codegen_calling_convention_tests)

add_executable(codegen_vreg_collector_tests
    tests/unit/codegen_vreg_collector_tests.cpp
)

target_link_libraries(codegen_vreg_collector_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_vreg_collector_tests)

add_test(NAME codegen_vreg_collector_tests COMMAND codegen_vreg_collector_tests)

add_executable(codegen_snapshot_tests
    tests/integration/codegen_snapshot_tests.cpp
)

target_link_libraries(codegen_snapshot_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_snapshot_tests)

add_test(NAME codegen_snapshot_tests COMMAND codegen_snapshot_tests)
