#!/usr/bin/env python3
"""Java/src2 dual regression runner v2 - robust per-case error handling."""
import csv
import os
import re
import subprocess
import sys
import tempfile

TOYC = "/mnt/e/TOYC"
JAVA_JAR = f"{TOYC}/toy-c-compiler-master/target/toyc.jar"
SRC2_BIN = "/tmp/src2_fixed"
CASE_DIR = f"{TOYC}/tools/triple_diff/cases"
STARTUP = "/tmp/rvtest_start.s"
LINKER = "/tmp/rvtest_link.ld"
SPIKE = "/home/ayako/.local/bin/spike"
OUTDIR = "/tmp/dual_reg"

os.makedirs(OUTDIR, exist_ok=True)

def parse_tohost(text: str) -> tuple:
    """Parse Spike output, return (unsigned, signed, raw_info)."""
    if not text.strip():
        return (0, 0, "empty")
    m = re.search(r"tohost\s*=\s*(0x[0-9a-fA-F]+|-?\d+)", text)
    if not m:
        return (None, None, f"no_match: {text[:100]}")
    raw = m.group(1)
    value = int(raw, 0) & 0xFFFFFFFF
    if "*** FAILED ***" in text:
        value = ((value << 1) | 1) & 0xFFFFFFFF
    signed = value - 0x100000000 if value >= 0x80000000 else value
    return (value, signed, "ok")

def run():
    results = []
    tc_files = sorted(f for f in os.listdir(CASE_DIR) if f.endswith(".tc"))

    print(f"Found {len(tc_files)} test cases in {CASE_DIR}")

    for tc_name in [f.removesuffix(".tc") for f in tc_files]:
        tc_path = os.path.join(CASE_DIR, tc_name + ".tc")
        exp_path = os.path.join(CASE_DIR, tc_name + ".expected.exit")
        expect = open(exp_path).read().strip() if os.path.isfile(exp_path) else None

        for compiler in ("java", "src2"):
            prefix = f"{OUTDIR}/{compiler}_{tc_name}"
            asm_file = f"{prefix}.s"
            elf_file = f"{prefix}.elf"
            spike_log = f"{prefix}.spike.log"

            print(f"  [{compiler}] {tc_name}...", end=" ", flush=True)

            # Compile
            if compiler == "java":
                if not os.path.isfile(JAVA_JAR):
                    print("SKIP (no jar)")
                    continue
                ret = subprocess.run(
                    ["java", "-jar", JAVA_JAR, "-opt"],
                    stdin=open(tc_path), capture_output=True, timeout=30)
            else:
                if not os.path.isfile(SRC2_BIN):
                    print("SKIP (no bin)")
                    continue
                ret = subprocess.run(
                    [SRC2_BIN, "-opt=o3"],
                    stdin=open(tc_path), capture_output=True, timeout=30)

            if ret.returncode != 0:
                print("COMPILE FAIL")
                logfile = f"{prefix}.compile_err.txt"
                with open(logfile, "w") as f:
                    f.write(ret.stderr.decode())
                results.append((tc_name, compiler, "FAIL(compile)", "", expect))
                continue

            with open(asm_file, "wb") as f:
                f.write(ret.stdout)

            # Link
            r = subprocess.run(
                ["riscv64-unknown-elf-gcc", "-march=rv32im", "-mabi=ilp32",
                 "-nostdlib", "-static", "-T", LINKER, "-o", elf_file,
                 STARTUP, asm_file],
                capture_output=True, timeout=30)
            if r.returncode != 0:
                print("LINK FAIL")
                results.append((tc_name, compiler, "FAIL(link)", "", expect))
                continue

            # Spike
            try:
                r = subprocess.run(
                    [SPIKE, "--isa=rv32im", elf_file],
                    capture_output=True, timeout=20)
                combined = (r.stdout + b"\n" + r.stderr).decode("utf-8", errors="replace")
                with open(spike_log, "w") as f:
                    f.write(combined)

                if r.returncode == 124:
                    print("TIMEOUT")
                    results.append((tc_name, compiler, "FAIL(timeout)", "timeout", expect))
                    continue

                dec_unsigned, dec_signed, info = parse_tohost(combined)
                if dec_unsigned is None:
                    print(f"PARSE FAIL ({info})")
                    results.append((tc_name, compiler, "FAIL(parse)", info, expect))
                    continue

                match = ""
                if expect is not None:
                    exp = int(expect)
                    if dec_signed == exp:
                        match = "MATCH"
                    else:
                        match = f"MISMATCH(got={dec_signed})"

                status = "PASS" if match == "MATCH" else f"VAL({dec_signed})" if dec_signed is not None else "FAIL"
                print(f"→ {dec_signed} {match}")
                results.append((tc_name, compiler, status, f"val={dec_signed}", expect))

            except subprocess.TimeoutExpired:
                print("TIMEOUT")
                results.append((tc_name, compiler, "FAIL(timeout)", "timeout", expect))

    # Summary
    print(f"\n{'='*60}")
    print("  REGRESSION SUMMARY")
    print(f"{'='*60}")
    print(f"{'case':<40} {'java':<20} {'src2':<20} {'expect':<10}")
    print(f"{'-'*60}")

    case_names = sorted(set(r[0] for r in results))
    all_match = True
    for cn in case_names:
        j = [r for r in results if r[0] == cn and r[1] == "java"]
        s = [r for r in results if r[0] == cn and r[1] == "src2"]
        jv = j[0][3] if j else "-"
        sv = s[0][3] if s else "-"
        ex = j[0][4] if j else (s[0][4] if s else "")
        match_str = "✓" if (j and j[0][2] == "PASS") and (s and s[0][2] == "PASS") else ("✗" if "MISMATCH" in (j[0][2] if j else "") or "MISMATCH" in (s[0][2] if s else "") else "?")
        if match_str == "✗": all_match = False
        print(f"{cn:<40} {jv:<20} {sv:<20} {ex:<10} {match_str}")
    print(f"{'-'*60}")
    print(f"All Java/src2 matched: {'YES' if all_match else 'NO'}")

if __name__ == "__main__":
    run()
