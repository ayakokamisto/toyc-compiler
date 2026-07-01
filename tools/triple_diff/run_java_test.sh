#!/usr/bin/env bash
set -euo pipefail

cd /mnt/e/TOYC
tc_file="$1"
name="$(basename "$tc_file" .tc)"
echo "=== Testing: $tc_file ==="

# Compile with Java
java -jar toy-c-compiler-master/target/toyc.jar -opt < "$tc_file" > "/tmp/java_${name}.s" 2>"/tmp/java_${name}.compile.log" || {
  echo "COMPILE FAIL:"; cat "/tmp/java_${name}.compile.log"; exit 1; }

# Link
riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static \
  -T /tmp/rvtest_link.ld -o "/tmp/java_${name}.elf" /tmp/rvtest_start.s "/tmp/java_${name}.s" 2>"/tmp/java_${name}.link.log" || {
  echo "LINK FAIL:"; cat "/tmp/java_${name}.link.log"; exit 1; }

# Run in Spike
timeout 10s "$HOME/.local/bin/spike" --isa=rv32im "/tmp/java_${name}.elf" > "/tmp/java_${name}.spike.log" 2>&1
spike_exit=$?
echo "Spike output:"
cat "/tmp/java_${name}.spike.log"
echo "SPIKE EXIT: $spike_exit"
echo ""
