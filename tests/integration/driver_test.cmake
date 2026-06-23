if(NOT DEFINED TOYCC OR NOT DEFINED INPUT OR NOT DEFINED EXPECTED_EXIT)
    message(FATAL_ERROR "TOYCC, INPUT, and EXPECTED_EXIT are required")
endif()

function(fail_with_io reason)
    message(FATAL_ERROR
        "${reason}\n"
        "actual exit=${actual_exit}\n"
        "stdout=[${actual_stdout}]\n"
        "stderr=[${actual_stderr}]")
endfunction()

execute_process(
    COMMAND "${TOYCC}" ${TOYCC_ARGS}
    INPUT_FILE "${INPUT}"
    RESULT_VARIABLE actual_exit
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr
)

if(NOT "${actual_exit}" STREQUAL "${EXPECTED_EXIT}")
    fail_with_io("unexpected exit code: expected ${EXPECTED_EXIT}, got ${actual_exit}")
endif()

if(EXPECT_STDOUT_EMPTY AND EXPECT_STDOUT_NONEMPTY)
    fail_with_io("EXPECT_STDOUT_EMPTY and EXPECT_STDOUT_NONEMPTY are mutually exclusive")
endif()

if(EXPECT_STDOUT_EMPTY AND NOT "${actual_stdout}" STREQUAL "")
    fail_with_io("expected empty stdout")
endif()

if(EXPECT_STDOUT_NONEMPTY AND "${actual_stdout}" STREQUAL "")
    fail_with_io("expected non-empty stdout")
endif()

if(DEFINED EXPECTED_STDOUT_PATTERN AND
   NOT actual_stdout MATCHES "${EXPECTED_STDOUT_PATTERN}")
    fail_with_io("stdout does not match '${EXPECTED_STDOUT_PATTERN}'")
endif()

if(EXPECT_STDERR_EMPTY AND NOT "${actual_stderr}" STREQUAL "")
    fail_with_io("expected empty stderr")
endif()

if(EXPECT_DIAGNOSTIC AND
   NOT actual_stderr MATCHES "[0-9]+:[0-9]+: (error|warning):")
    fail_with_io("missing formatted diagnostic")
endif()

if(DEFINED EXPECTED_STDERR_PATTERN AND
   NOT actual_stderr MATCHES "${EXPECTED_STDERR_PATTERN}")
    fail_with_io("stderr does not match '${EXPECTED_STDERR_PATTERN}'")
endif()
