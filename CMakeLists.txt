cmake_minimum_required(VERSION 3.20)

project(toyc_compiler LANGUAGES CXX)

option(TOYC_ENABLE_RISCV_EXEC_TESTS "Run RISC-V QEMU execution integration tests" OFF)

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
    src/codegen/lower/FoldableConsts.cpp
    src/codegen/frame/RegisterAllocator.cpp
    src/codegen/opt/PeepholeOptimizer.cpp
    src/codegen/opt/IrOptimizer.cpp
)

target_include_directories(toyc_codegen
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

toyc_apply_warnings(toyc_codegen)

add_library(toyc_sema
    src/sema/semantic_model.cpp
    src/sema/sema.cpp
    src/sema/const_eval.cpp
    src/sema/return_check.cpp
)

target_include_directories(toyc_sema
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(toyc_sema
    PUBLIC toyc_frontend
)

toyc_apply_warnings(toyc_sema)

add_library(toyc_ir
    src/ir/contract_ir_generator.cpp
)

target_include_directories(toyc_ir
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(toyc_ir
    PUBLIC toyc_sema
)

toyc_apply_warnings(toyc_ir)

add_executable(toycc
    src/driver/main.cpp
)

target_link_libraries(toycc PRIVATE toyc_ir toyc_codegen toyc_sema toyc_frontend)
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
        -DEXPECT_STDOUT_NONEMPTY=ON
        -DEXPECTED_STDOUT_PATTERN=main:
        -DEXPECT_STDERR_EMPTY=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME syntax_error_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/syntax_error.tc
        -DEXPECTED_EXIT=1
        -DEXPECT_STDOUT_EMPTY=ON
        -DEXPECT_DIAGNOSTIC=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME semantic_errors_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/semantic_errors.tc
        -DEXPECTED_EXIT=1
        -DEXPECT_STDOUT_EMPTY=ON
        -DEXPECT_DIAGNOSTIC=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME opt_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/valid_parser.tc
        -DEXPECTED_EXIT=0
        -DTOYCC_ARGS=-opt
        -DEXPECT_STDOUT_NONEMPTY=ON
        -DEXPECTED_STDOUT_PATTERN=main:
        -DEXPECT_STDERR_EMPTY=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME dump_tokens_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/valid_parser.tc
        -DEXPECTED_EXIT=0
        -DTOYCC_ARGS=--dump-tokens
        -DEXPECT_STDOUT_EMPTY=ON
        -DEXPECTED_STDERR_PATTERN=EOF
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
)

add_test(
    NAME unknown_argument_driver
    COMMAND ${CMAKE_COMMAND}
        -DTOYCC=$<TARGET_FILE:toycc>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver/valid_parser.tc
        -DEXPECTED_EXIT=2
        -DTOYCC_ARGS=--bad-option
        -DEXPECT_STDOUT_EMPTY=ON
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
    many_params_call
    nested_call_pressure
    stack_pressure_expr
    global_read_write_mix
    loop_call_control
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
            -DEXPECT_STDOUT_NONEMPTY=ON
            -DEXPECTED_STDOUT_PATTERN=main:
            -DEXPECT_STDERR_EMPTY=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/driver_test.cmake
    )
endforeach()

if(TOYC_ENABLE_RISCV_EXEC_TESTS)
    find_program(TOYC_BASH_EXECUTABLE bash)
    find_program(TOYC_RISCV_GCC_EXECUTABLE riscv64-linux-gnu-gcc)
    find_program(TOYC_QEMU_RISCV32_EXECUTABLE qemu-riscv32)
    find_program(TOYC_TIMEOUT_EXECUTABLE timeout)

    if(NOT TOYC_BASH_EXECUTABLE)
        message(FATAL_ERROR
            "TOYC_ENABLE_RISCV_EXEC_TESTS=ON requires bash. "
            "Configure inside WSL/Unix, or leave the option OFF and run "
            "tests/integration/run_toyc_exec_cases.sh manually from WSL.")
    endif()

    if(NOT TOYC_RISCV_GCC_EXECUTABLE)
        message(FATAL_ERROR
            "TOYC_ENABLE_RISCV_EXEC_TESTS=ON requires riscv64-linux-gnu-gcc "
            "in PATH.")
    endif()

    if(NOT TOYC_QEMU_RISCV32_EXECUTABLE)
        message(FATAL_ERROR
            "TOYC_ENABLE_RISCV_EXEC_TESTS=ON requires qemu-riscv32 in PATH.")
    endif()

    if(NOT TOYC_TIMEOUT_EXECUTABLE)
        message(FATAL_ERROR
            "TOYC_ENABLE_RISCV_EXEC_TESTS=ON requires timeout in PATH.")
    endif()

    execute_process(
        COMMAND "${TOYC_TIMEOUT_EXECUTABLE}" --version
        RESULT_VARIABLE TOYC_TIMEOUT_VERSION_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT TOYC_TIMEOUT_VERSION_RESULT EQUAL 0)
        message(FATAL_ERROR
            "TOYC_ENABLE_RISCV_EXEC_TESTS=ON requires a GNU/coreutils-compatible "
            "timeout executable.")
    endif()

    if(WIN32)
        message(WARNING
            "RISC-V execution CTest is registered with a detected bash on Windows. "
            "If path conversion fails under native PowerShell CTest, configure and "
            "run this option from WSL instead.")
    endif()

    add_test(
        NAME toycc_riscv_exec_cases
        COMMAND "${TOYC_BASH_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/run_toyc_exec_cases.sh"
            --build-dir "${CMAKE_CURRENT_BINARY_DIR}"
            --compiler "${TOYC_RISCV_GCC_EXECUTABLE}"
            --runner "${TOYC_QEMU_RISCV32_EXECUTABLE}"
            --timeout-tool "${TOYC_TIMEOUT_EXECUTABLE}"
    )
endif()

add_executable(sema_symbol_tests
    tests/unit/sema_symbol_tests.cpp
)

target_link_libraries(sema_symbol_tests PRIVATE toyc_sema)
toyc_apply_warnings(sema_symbol_tests)

add_test(NAME sema_symbol_tests COMMAND sema_symbol_tests)

add_executable(sema_model_tests
    tests/unit/sema_model_tests.cpp
)

target_link_libraries(sema_model_tests PRIVATE toyc_sema)
toyc_apply_warnings(sema_model_tests)

add_test(NAME sema_model_tests COMMAND sema_model_tests)

add_executable(sema_const_eval_tests
    tests/unit/sema_const_eval_tests.cpp
)

target_link_libraries(sema_const_eval_tests PRIVATE toyc_sema)
toyc_apply_warnings(sema_const_eval_tests)

add_test(NAME sema_const_eval_tests COMMAND sema_const_eval_tests)

add_executable(sema_return_check_tests
    tests/unit/sema_return_check_tests.cpp
)

target_link_libraries(sema_return_check_tests PRIVATE toyc_sema)
toyc_apply_warnings(sema_return_check_tests)

add_test(NAME sema_return_check_tests COMMAND sema_return_check_tests)

add_executable(sema_analysis_tests
    tests/unit/sema_analysis_tests.cpp
)

target_link_libraries(sema_analysis_tests PRIVATE toyc_sema)
toyc_apply_warnings(sema_analysis_tests)

add_test(NAME sema_analysis_tests COMMAND sema_analysis_tests)

add_executable(ir_contract_generator_tests
    tests/unit/ir_contract_generator_tests.cpp
)

target_link_libraries(ir_contract_generator_tests PRIVATE toyc_ir)
toyc_apply_warnings(ir_contract_generator_tests)

add_test(NAME ir_contract_generator_tests COMMAND ir_contract_generator_tests)

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

add_executable(ir_optimizer_tests
    tests/unit/ir_optimizer_tests.cpp
)

target_link_libraries(ir_optimizer_tests PRIVATE toyc_ir toyc_codegen)
toyc_apply_warnings(ir_optimizer_tests)

add_test(NAME ir_optimizer_tests COMMAND ir_optimizer_tests)
