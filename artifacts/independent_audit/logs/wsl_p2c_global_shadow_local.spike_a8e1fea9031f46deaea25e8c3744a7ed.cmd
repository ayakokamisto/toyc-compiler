@echo off
wsl -e bash -lc "timeout 5s /home/ayako/.local/bin/spike --isa=rv32im /mnt/e/TOYC/artifacts/independent_audit/cpp_output/p2c_global_shadow_local.elf" > "E:\TOYC\artifacts\independent_audit\cpp_output\p2c_global_shadow_local.spike.log" 2>&1
