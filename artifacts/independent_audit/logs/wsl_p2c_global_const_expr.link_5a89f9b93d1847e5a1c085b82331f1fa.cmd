@echo off
wsl -e bash -lc "riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static -T /tmp/rvtest_link.ld -o /mnt/e/TOYC/artifacts/independent_audit/java_output/p2c_global_const_expr.elf /tmp/rvtest_start.s /mnt/e/TOYC/artifacts/independent_audit/java_output/p2c_global_const_expr.s" > "E:\TOYC\artifacts\independent_audit\java_output\p2c_global_const_expr.link.log" 2>&1
