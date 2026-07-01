@echo off
wsl -e bash -lc "timeout 5s /home/ayako/.local/bin/spike --isa=rv32im /mnt/e/TOYC/artifacts/independent_audit/java_output/p2_dangling_else.elf" > "E:\TOYC\artifacts\independent_audit\java_output\p2_dangling_else.spike.log" 2>&1
