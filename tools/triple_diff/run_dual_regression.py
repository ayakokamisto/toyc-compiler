#!/usr/bin/env python3
"""Java/src2 dual regression runner."""
import csv
import os
import re
import subprocess
import sys
import tempfile

TOYC = "/mnt/e/TOYC"
JAVA_JAR = f"{TOYC}/toy-c-compiler-master/target/toyc.jar"
SRC2_BIN = "/tmp/src2_fixed"
CASES = [
    f"{TOYC}/tools/triple_diff/cases",
    f"{TOYC}/toy-c-compiler-master/src/test/resources/smoke",
]
STARTUP = "/tmp/rvtest_start.s"
LINKER = "/tmp/rvtest_link.ld"
SPIKE = "/home/ayako/.local/bin/spike"
OUTDIR = "/tmp/dual_reg"

os.makedirs(OUTDIR, exist_ok=True)


def parse_tohost(text: str) -> tuple[int, int]:
    """Parse Spike output, return (unsigned, signed)."""
    if not text.strip():
        return (0, 0)
    m = re.search(r"tohost\s*=\s*(0x[0-9a-fA-F]+|-?\d+)", text)
    if not m:
        raise ValueError(f"cannot parse tohost from: {text!r}")
    raw = m.group(1)
    value = int(raw, 0) & 0xFFFFFFFF
    if "*** FAILED ***" in text:
        value = ((value << 1) | 1) & 0xFFFFFFFF
    signed = value - 0x100000000 if value >= 0x80000000 else value
    return (value, signed)


def count_opcode(asm_path: str, op: str) -> int:
    if not os.path.isfile(asm_path):
        return 0
    count = 0
    with open(asm_path) as f:
        for line in f:
            parts = line.strip().split()
            if parts and parts[0] == op:
                count += 1
    return count


def count_asm_lines(asm_path: str) -> int:
    if not os.path.isfile(asm_path):
        return 0
    count = 0
    with open(asm_path) as f:
        for line in f:
            s = line.strip()
            if s and not s.startswith(".") and not s.startswith("#"):
                count += 1
    return count


def find_expected(tc_path: str) -> str | None:
    base = tc_path.removesuffix(".tc")
    for candidate in [base + ".expected.exit", f"{TOYC}/tools/triple_diff/cases/{os.path.basename(base)}.expected.exit"]:
        if os.path.isfile(candidate):
            with open(candidate) as f:
                return f.read().strip()
    return None


def collect_cases() -> list[tuple[str, str]]:
    """Return list of (tc_path, case_name)."""
    seen = set()
    cases = []
    for root in CASES:
        if not os.path.isdir(root):
            print(f"  [WARN] cases dir not found: {root}", file=sys.stderr)
            continue
        for fn in sorted(os.listdir(root)):
            if not fn.endswith(".tc"):
                continue
            if fn in seen:
                continue
            seen.add(fn)
            cases.append((os.path.join(root, fn), fn.removesuffix(".tc")))
    return cases


def run():
    results = []
    cases = collect_cases()
    print(f"Found {len(cases)} test cases")

    for tc_path, tc_name in cases:
        expect = find_expected(tc_path)

        for compiler in ("java", "src2"):
            prefix = f"{OUTDIR}/{compiler}_{tc_name}"
            asm_file = f"{prefix}.s"
            compile_log = f"{prefix}.compile.log"
            spike_log = f"{prefix}.spike.log"
            link_log = f"{prefix}.link.log"
            elf_file = f"{prefix}.elf"

            # Compile
            if compiler == "java":
                if not os.path.isfile(JAVA_JAR):
                    results.append((tc_name, compiler, "SKIP(missing jar)", 0, 0, 0, 0, 0, 0, 0, 0, "", None, None, expect, ""))
                    continue
                ret = subprocess.run(
                    ["java", "-jar", JAVA_JAR, "-opt"],
                    stdin=open(tc_path),
                    capture_output=True, timeout=30,
                )
                with open(asm_file, "wb") as f:
                    f.write(ret.stdout)
                with open(compile_log, "wb") as f:
                    f.write(ret.stderr)
                if ret.returncode != 0:
                    al = count_asm_lines(asm_file)
                    results.append((tc_name, compiler, "FAIL(compile)", al, 0, 0, 0, 0, 0, 0, 0, "", None, None, expect, ""))
                    continue
            else:  # src2
                if not os.path.isfile(SRC2_BIN):
                    results.append((tc_name, compiler, "SKIP(missing bin)", 0, 0, 0, 0, 0, 0, 0, 0, "", None, None, expect, ""))
                    continue
                ret = subprocess.run(
                    [SRC2_BIN, "-opt=o3"],
                    stdin=open(tc_path),
                    capture_output=True, timeout=30,
                )
                with open(asm_file, "wb") as f:
                    f.write(ret.stdout)
                with open(compile_log, "wb") as f:
                    f.write(ret.stderr)
                if ret.returncode != 0:
                    al = count_asm_lines(asm_file)
                    results.append((tc_name, compiler, "FAIL(compile)", al, 0, 0, 0, 0, 0, 0, 0, "", None, None, expect, ""))
                    continue

            al = count_asm_lines(asm_file)
            lw = count_opcode(asm_file, "lw")
            sw = count_opcode(asm_file, "sw")
            call = count_opcode(asm_file, "call")
            ret = count_opcode(asm_file, "ret")
            mul = count_opcode(asm_file, "mul")
            div = count_opcode(asm_file, "div")
            rem = count_opcode(asm_file, "rem")

            # Link
            r = subprocess.run(
                ["riscv64-unknown-elf-gcc", "-march=rv32im", "-mabi=ilp32",
                 "-nostdlib", "-static", "-T", LINKER, "-o", elf_file,
                 STARTUP, asm_file],
                capture_output=True, timeout=30,
            )
            with open(link_log, "wb") as f:
                f.write(r.stderr)
            if r.returncode != 0:
                results.append((tc_name, compiler, "FAIL(link)", al, lw, sw, call, ret, mul, div, rem, "", None, None, expect, ""))
                continue

            # Spike
            spike_display = ""
            try:
                r = subprocess.run(
                    [SPIKE, "--isa=rv32im", elf_file],
                    capture_output=True, timeout=20,
                )
                # Spike outputs to stderr; combine both streams
                spike_output = (r.stdout + b"\n" + r.stderr).decode("utf-8", errors="replace")
                with open(spike_log, "w") as f:
                    f.write(spike_output)
                spike_text = spike_output
                spike_display = spike_text.replace("\n", " ").strip()

                # Parse tohost
                dec_unsigned, dec_signed = parse_tohost(spike_text)
                match_str = ""
                if expect is not None:
                    exp = int(expect)
                    if dec_signed == exp or dec_unsigned == exp:
                        match_str = "MATCH"
                    else:
                        match_str = "MISMATCH"
                results.append((tc_name, compiler, "PASS", al, lw, sw, call, ret, mul, div, rem, spike_display, dec_unsigned, dec_signed, expect, match_str))
            except subprocess.TimeoutExpired:
                results.append((tc_name, compiler, "FAIL(timeout)", al, lw, sw, call, ret, mul, div, rem, "timeout", None, None, expect, ""))

    # Write PSV
    psv_path = f"{OUTDIR}/results.psv"
    with open(psv_path, "w", newline="") as f:
        w = csv.writer(f, delimiter="|")
        w.writerow(["case", "compiler", "result", "asm_lines", "lw", "sw", "call", "ret", "mul", "div", "rem", "spike_display", "unsigned", "signed", "expect", "match"])
        for row in results:
            w.writerow(row)

    # Summary table
    print(f"\n{'='*70}")
    print(f"  DUAL REGRESSION SUMMARY")
    print(f"{'='*70}")
    print(f"{'case':<35} {'java':<15} {'src2':<15} {'expect':<8} {'match':<8}")
    print(f"{'-'*70}")

    case_names = sorted(set(r[0] for r in results))
    for cn in case_names:
        jr = [r for r in results if r[0] == cn and r[1] == "java"]
        sr = [r for r in results if r[0] == cn and r[1] == "src2"]
        j_stat = jr[0][2] if jr else ""
        j_val = jr[0][12] if jr and jr[0][12] is not None else ""
        s_stat = sr[0][2] if sr else ""
        s_val = sr[0][12] if sr and sr[0][12] is not None else ""
        expect_str = jr[0][14] if jr else sr[0][14] if sr else ""
        match_str = sr[0][15] if sr else ""

        j_display = f"{j_stat}" + (f"({j_val})" if j_val != "" else "")
        s_display = f"{s_stat}" + (f"({s_val})" if s_val != "" else "")
        print(f"{cn:<35} {j_display:<15} {s_display:<15} {expect_str:<8} {match_str:<8}")

    print(f"{'-'*70}")

    # Summary counts
    total = len(set(r[0] for r in results))
    java_pass = len([r for r in results if r[1] == "java" and r[2] == "PASS"])
    src2_pass = len([r for r in results if r[1] == "src2" and r[2] == "PASS"])
    java_skip = len([r for r in results if r[1] == "java" and r[2].startswith("SKIP")])
    src2_skip = len([r for r in results if r[1] == "src2" and r[2].startswith("SKIP")])
    matched = len([r for r in results if r[15] == "MATCH"])
    mismatched = len([r for r in results if r[15] == "MISMATCH"])

    print(f"\nTotal cases: {total}")
    print(f"Java PASS: {java_pass}/{total} (SKIP: {java_skip})")
    print(f"src2 PASS: {src2_pass}/{total} (SKIP: {src2_skip})")
    print(f"Java/src2 MATCH: {matched}")
    print(f"Java/src2 MISMATCH: {mismatched}")


if __name__ == "__main__":
    run()
