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
