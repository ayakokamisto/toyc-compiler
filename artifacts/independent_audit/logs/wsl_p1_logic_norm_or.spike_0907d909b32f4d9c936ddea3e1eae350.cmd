@echo off
wsl -e bash -lc "timeout 5s /home/ayako/.local/bin/spike --isa=rv32im /mnt/e/TOYC/artifacts/independent_audit/java_output/p1_logic_norm_or.elf" > "E:\TOYC\artifacts\independent_audit\java_output\p1_logic_norm_or.spike.log" 2>&1
