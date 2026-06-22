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
