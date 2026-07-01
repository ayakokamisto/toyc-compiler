@echo off
wsl -e bash -lc "timeout 5s /home/ayako/.local/bin/spike --isa=rv32im /mnt/e/TOYC/artifacts/independent_audit/cpp_output/p1_return_minus_1.elf" > "E:\TOYC\artifacts\independent_audit\cpp_output\p1_return_minus_1.spike.log" 2>&1
