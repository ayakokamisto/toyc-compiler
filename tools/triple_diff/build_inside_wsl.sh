#!/usr/bin/env bash
set -euo pipefail

cd /mnt/e/TOYC

# Find all source files
SRCS=$(find src2 -name "*.cpp" | sort)
echo "Source files found: $(echo "$SRCS" | wc -l)"

# Compile
g++ -std=c++20 -O2 -Isrc2 $SRCS -o /tmp/src2_test 2>&1
echo "G++ EXIT: $?"
ls -la /tmp/src2_test 2>/dev/null && echo "BUILD OK" || echo "BUILD FAILED"
