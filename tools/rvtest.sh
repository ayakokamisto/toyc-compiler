#!/bin/bash
# rvtest — compile a .tc file with toycc, assemble, link, and run with spike.
# Usage: ./tools/rvtest.sh <test.tc> [expected_exit_code]
# Requires: toycc in PATH, riscv64-unknown-elf-gcc, spike

set -e
TC="$1"
EXPECTED="${2:-0}"
NAME=$(basename "$TC" .tc)

# Compile ToyC → RISC-V assembly
toycc -opt < "$TC" > /tmp/rvtest_toycc.s 2>/tmp/rvtest_toycc.err

# Assemble + link
riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib -static \
    -o /tmp/rvtest_bin /tmp/rvtest_toycc.s 2>/tmp/rvtest_gcc.err

# Run with spike (use proxy kernel for syscall support)
ACTUAL=$(spike --isa=rv32i /usr/lib/riscv64-unknown-elf/bin/pk /tmp/rvtest_bin 2>/dev/null; echo $?)

if [ "$ACTUAL" -eq "$EXPECTED" ]; then
    echo "PASS $NAME (exit=$ACTUAL expected=$EXPECTED)"
    exit 0
else
    echo "FAIL $NAME (exit=$ACTUAL expected=$EXPECTED)"
    echo "--- assembly ---"
    cat /tmp/rvtest_toycc.s
    exit 1
fi
