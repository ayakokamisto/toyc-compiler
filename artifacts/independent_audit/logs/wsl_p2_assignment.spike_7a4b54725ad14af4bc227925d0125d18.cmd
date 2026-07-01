@echo off
wsl -e bash -lc "timeout 5s /home/ayako/.local/bin/spike --isa=rv32im /mnt/e/TOYC/artifacts/independent_audit/cpp_output/p2_assignment.elf" > "E:\TOYC\artifacts\independent_audit\cpp_output\p2_assignment.spike.log" 2>&1
