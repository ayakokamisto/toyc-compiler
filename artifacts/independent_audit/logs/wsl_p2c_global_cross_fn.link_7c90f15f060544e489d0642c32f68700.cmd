@echo off
wsl -e bash -lc "riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static -T /tmp/rvtest_link.ld -o /mnt/e/TOYC/artifacts/independent_audit/cpp_output/p2c_global_cross_fn.elf /tmp/rvtest_start.s /mnt/e/TOYC/artifacts/independent_audit/cpp_output/p2c_global_cross_fn.s" > "E:\TOYC\artifacts\independent_audit\cpp_output\p2c_global_cross_fn.link.log" 2>&1
