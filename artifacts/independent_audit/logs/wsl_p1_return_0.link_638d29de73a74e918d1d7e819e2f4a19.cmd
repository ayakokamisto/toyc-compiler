@echo off
wsl -e bash -lc "riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static -T /tmp/rvtest_link.ld -o /mnt/e/TOYC/artifacts/independent_audit/cpp_output/p1_return_0.elf /tmp/rvtest_start.s /mnt/e/TOYC/artifacts/independent_audit/cpp_output/p1_return_0.s" > "E:\TOYC\artifacts\independent_audit\cpp_output\p1_return_0.link.log" 2>&1
