if(NOT DEFINED TOYCC OR NOT DEFINED INPUT OR NOT DEFINED EXPECTED_EXIT)
    message(FATAL_ERROR "TOYCC, INPUT, and EXPECTED_EXIT are required")
endif()

execute_process(
    COMMAND "${TOYCC}" ${TOYCC_ARGS}
    INPUT_FILE "${INPUT}"
    RESULT_VARIABLE actual_exit
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr
)

if(NOT "${actual_exit}" STREQUAL "${EXPECTED_EXIT}")
    message(FATAL_ERROR
        "unexpected exit code: expected ${EXPECTED_EXIT}, got ${actual_exit}; stderr=${actual_stderr}")
endif()

if(NOT "${actual_stdout}" STREQUAL "")
    message(FATAL_ERROR "driver wrote to stdout: ${actual_stdout}")
endif()

if(EXPECT_PARSER_ERROR)
    if(NOT actual_stderr MATCHES "[0-9]+:[0-9]+: error:")
        message(FATAL_ERROR "missing formatted parser diagnostic: ${actual_stderr}")
    endif()
else()
    if(actual_stderr MATCHES "[0-9]+:[0-9]+: error:")
        message(FATAL_ERROR "unexpected parser diagnostic: ${actual_stderr}")
    endif()
endif()

if(DEFINED EXPECTED_STDERR_PATTERN AND
   NOT actual_stderr MATCHES "${EXPECTED_STDERR_PATTERN}")
    message(FATAL_ERROR
        "stderr does not match '${EXPECTED_STDERR_PATTERN}': ${actual_stderr}")
endif()
