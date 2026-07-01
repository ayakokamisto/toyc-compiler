@echo off
wsl -e bash -lc "timeout 5s /home/ayako/.local/bin/spike --isa=rv32im /mnt/e/TOYC/artifacts/independent_audit/java_output/p2_sum9.elf" > "E:\TOYC\artifacts\independent_audit\java_output\p2_sum9.spike.log" 2>&1
