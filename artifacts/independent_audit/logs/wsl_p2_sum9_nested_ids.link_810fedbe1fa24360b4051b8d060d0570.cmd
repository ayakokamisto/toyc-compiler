@echo off
wsl -e bash -lc "riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static -T /tmp/rvtest_link.ld -o /mnt/e/TOYC/artifacts/independent_audit/java_output/p2_sum9_nested_ids.elf /tmp/rvtest_start.s /mnt/e/TOYC/artifacts/independent_audit/java_output/p2_sum9_nested_ids.s" > "E:\TOYC\artifacts\independent_audit\java_output\p2_sum9_nested_ids.link.log" 2>&1
