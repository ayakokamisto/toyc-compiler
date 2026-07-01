cmake_minimum_required(VERSION 3.20)
project(toyc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.2)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)
enable_testing()

set(IR_SOURCES
    src/ir/value.cpp src/ir/instruction.cpp src/ir/basic_block.cpp
    src/ir/function.cpp src/ir/module.cpp src/ir/ir_printer.cpp
    src/ir/cfg.cpp src/ir/dominator_tree.cpp src/ir/def_use.cpp
    src/ir/alloca_promotability.cpp src/ir/mem2reg.cpp
    src/ir/ir_builder.cpp src/ir/ir_verifier.cpp
)
set(FRONTEND_SOURCES
    src/frontend/lexer.cpp src/frontend/parser.cpp src/frontend/semantic_analyzer.cpp
)
set(BACKEND_SOURCES
    src/backend/frame_layout.cpp src/backend/riscv_emitter.cpp
)

add_library(toyc_ir STATIC ${IR_SOURCES})
target_include_directories(toyc_ir PUBLIC include)
target_compile_features(toyc_ir PUBLIC cxx_std_20)

add_executable(toycc src/driver/main.cpp ${FRONTEND_SOURCES} ${BACKEND_SOURCES})
target_link_libraries(toycc PRIVATE toyc_ir)
target_include_directories(toycc PRIVATE include)

option(TOYC_BUILD_TESTS "Build tests" ON)
if(TOYC_BUILD_TESTS)
    foreach(TEST_SRC
        tests/ir/ir_model_test.cpp
        tests/ir/cfg_test.cpp
        tests/ir/analysis_test.cpp
        tests/ir/mem2reg_test.cpp
        tests/frontend/lexer_test.cpp
        tests/frontend/parser_test.cpp
        tests/ir/ir_builder_test.cpp
        tests/backend/riscv_emitter_test.cpp
    )
        get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_SRC})
        target_link_libraries(${TEST_NAME} PRIVATE toyc_ir GTest::gtest_main)
        target_include_directories(${TEST_NAME} PRIVATE include)
        target_sources(${TEST_NAME} PRIVATE ${FRONTEND_SOURCES} ${BACKEND_SOURCES})
        gtest_discover_tests(${TEST_NAME})
    endforeach()
endif()
