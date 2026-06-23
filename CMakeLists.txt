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
    src/parser/parser.cpp
)

target_include_directories(toyc_frontend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

toyc_apply_warnings(toyc_frontend)

add_library(toyc_codegen
    src/codegen/RiscvBackend.cpp
    src/codegen/IrOperandUtils.cpp
    src/codegen/emit/RiscvEmitter.cpp
    src/codegen/frame/StackFrame.cpp
    src/codegen/abi/CallingConvention.cpp
    src/codegen/lower/InstructionSelector.cpp
    src/codegen/frame/VRegCollector.cpp
    src/codegen/frame/VRegAnalysis.cpp
    src/codegen/frame/VRegAssignment.cpp
    src/codegen/lower/FunctionEmitter.cpp
    src/codegen/lower/BranchFusionAnalysis.cpp
    src/codegen/frame/RegisterAllocator.cpp
    src/codegen/opt/PeepholeOptimizer.cpp
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

add_executable(parser_p1_tests
    tests/frontend/parser_p1_tests.cpp
)

target_link_libraries(parser_p1_tests PRIVATE toyc_frontend)
toyc_apply_warnings(parser_p1_tests)

add_test(NAME parser_p1_tests COMMAND parser_p1_tests)

add_executable(parser_p2_tests
    tests/frontend/parser_p2_tests.cpp
)

target_link_libraries(parser_p2_tests PRIVATE toyc_frontend)
toyc_apply_warnings(parser_p2_tests)

add_test(NAME parser_p2_tests COMMAND parser_p2_tests)

add_executable(parser_p3_tests
    tests/frontend/parser_p3_tests.cpp
)

target_link_libraries(parser_p3_tests PRIVATE toyc_frontend)
toyc_apply_warnings(parser_p3_tests)

add_test(NAME parser_p3_tests COMMAND parser_p3_tests)

add_executable(parser_p4_tests
    tests/frontend/parser_p4_tests.cpp
)

target_link_libraries(parser_p4_tests PRIVATE toyc_frontend)
toyc_apply_warnings(parser_p4_tests)

add_test(NAME parser_p4_tests COMMAND parser_p4_tests)

add_executable(parser_p5_tests
    tests/frontend/parser_p5_tests.cpp
)

target_link_libraries(parser_p5_tests PRIVATE toyc_frontend)
toyc_apply_warnings(parser_p5_tests)

add_test(NAME parser_p5_tests COMMAND parser_p5_tests)

add_test(
    NAME valid_parser_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/valid_parser.tc
        -DEXPECTED_EXIT=0
        -DEXPECT_PARSER_ERROR=OFF
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME syntax_error_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/syntax_error.tc
        -DEXPECTED_EXIT=1
        -DEXPECT_PARSER_ERROR=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

# This program is syntactically valid. Sema will reject it after Sema is integrated.
add_test(
    NAME parser_accepts_semantic_errors
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/semantic_errors.tc
        -DEXPECTED_EXIT=0
        -DEXPECT_PARSER_ERROR=OFF
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME opt_syntax_error_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/syntax_error.tc
        -DEXPECTED_EXIT=1
        -DEXPECT_PARSER_ERROR=ON
        -DTOYCC_ARGS=-opt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME dump_tokens_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/valid_parser.tc
        -DEXPECTED_EXIT=0
        -DEXPECT_PARSER_ERROR=OFF
        -DTOYCC_ARGS=--dump-tokens
        -DEXPECTED_STDERR_PATTERN=EOF
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME unknown_option_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/valid_parser.tc
        -DEXPECTED_EXIT=2
        -DEXPECT_PARSER_ERROR=OFF
        -DTOYCC_ARGS=--bad-option
        -DEXPECTED_STDERR_PATTERN=unknown.option
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

set(TOYC_INTEGRATION_CASES
    basic_return
    global_const
    global_var
    function_call
    recursion
    nested_scope_shadowing
    arithmetic_precedence
    if_else
    while_break_continue
    short_circuit_and
    short_circuit_or
    void_function
)

foreach(case_name IN LISTS TOYC_INTEGRATION_CASES)
    add_test(
        NAME parser_case_${case_name}
        COMMAND ${CMAKE_COMMAND}
            -DTOYCC=$<TARGET_FILE:toycc>
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/cases/${case_name}.tc
            -DEXPECTED_EXIT=0
            -DEXPECT_PARSER_ERROR=OFF
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
    )
endforeach()

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
    tests/integration/codegen/snapshot_tests.cpp
)

target_link_libraries(codegen_snapshot_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_snapshot_tests)

add_test(NAME codegen_snapshot_tests COMMAND codegen_snapshot_tests)

add_executable(codegen_golden_tests
    tests/integration/codegen/golden_tests.cpp
)

target_link_libraries(codegen_golden_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_golden_tests)

add_test(NAME codegen_golden_tests COMMAND codegen_golden_tests)

add_executable(codegen_contract_exec_cases
    tests/integration/codegen/contract_exec_cases.cpp
)

target_link_libraries(codegen_contract_exec_cases PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_contract_exec_cases)

add_executable(codegen_peephole_tests
    tests/unit/codegen_peephole_tests.cpp
)

target_link_libraries(codegen_peephole_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_peephole_tests)

add_test(NAME codegen_peephole_tests COMMAND codegen_peephole_tests)

add_executable(codegen_vreg_cache_tests
    tests/unit/codegen_vreg_cache_tests.cpp
)

target_link_libraries(codegen_vreg_cache_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_vreg_cache_tests)

add_test(NAME codegen_vreg_cache_tests COMMAND codegen_vreg_cache_tests)

add_executable(codegen_branch_fusion_tests
    tests/unit/codegen_branch_fusion_tests.cpp
)

target_link_libraries(codegen_branch_fusion_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_branch_fusion_tests)

add_test(NAME codegen_branch_fusion_tests COMMAND codegen_branch_fusion_tests)

add_executable(codegen_register_allocator_tests
    tests/unit/codegen_register_allocator_tests.cpp
)

target_link_libraries(codegen_register_allocator_tests PRIVATE toyc_codegen)
toyc_apply_warnings(codegen_register_allocator_tests)

add_test(NAME codegen_register_allocator_tests COMMAND codegen_register_allocator_tests)
