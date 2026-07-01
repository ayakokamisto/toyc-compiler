#!/usr/bin/env python3
import re
import subprocess
import sys
from pathlib import Path


CASES = [
    ("return_0", 0),
    ("return_1", 1),
    ("return_minus_1", -1),
    ("return_42", 42),
    ("return_int_min", -2147483648),
]


def wsl_path(path: Path) -> str:
    path = path.resolve()
    drive = path.drive[:1].lower()
    tail = path.as_posix()[3:]
    return f"/mnt/{drive}/{tail}"


def decode_tohost(text: str) -> tuple[int, int, int]:
    match = re.search(r"tohost\s*=\s*(0x[0-9a-fA-F]+|-?\d+)", text)
    if not match:
        raise ValueError("missing tohost value")
    encoded = int(match.group(1), 0) & 0xffffffffffffffff
    low = encoded & 0xffffffff
    high31 = (encoded >> 32) & 0x7fffffff
    if low not in (2, 6):
        raise ValueError(f"unexpected terminal low word: 0x{low:x}")
    unsigned = high31 | ((1 if low == 6 else 0) << 31)
    signed = unsigned - 0x100000000 if unsigned >= 0x80000000 else unsigned
    return encoded, unsigned, signed


def run(cmd: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    out = repo / "artifacts" / "rv32_harness_selftest"
    out.mkdir(parents=True, exist_ok=True)
    start = repo / "tools" / "triple_diff" / "rv32_split_tohost_start.s"
    link = repo / "tools" / "triple_diff" / "rv32_split_tohost_link.ld"
    report = out / "selftest_results.psv"
    report.write_text(
        "case|expected|link_exit|spike_exit|encoded|decoded_uint32|decoded_signed|assembly|link_command|spike_command|stdout|stderr|status\n",
        encoding="utf-8",
    )

    ok = True
    for name, expected in CASES:
        asm = out / f"{name}.s"
        elf = out / f"{name}.elf"
        stdout = out / f"{name}.stdout"
        stderr = out / f"{name}.stderr"
        asm.write_text(f".text\n.globl main\nmain:\n  li a0, {expected}\n  ret\n", encoding="ascii")
        link_cmd = (
            "riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static "
            f"-T {wsl_path(link)} -o {wsl_path(elf)} {wsl_path(start)} {wsl_path(asm)}"
        )
        spike_cmd = f"timeout 5s /home/ayako/.local/bin/spike --isa=rv32im {wsl_path(elf)}"

        link_proc = run(["wsl", "-e", "bash", "-lc", link_cmd])
        link_exit = link_proc.returncode
        spike_exit = ""
        encoded = decoded_unsigned = decoded_signed = ""
        status = "FAIL"
        spike_out = spike_err = ""
        if link_exit == 0:
            spike_proc = run(["wsl", "-e", "bash", "-lc", spike_cmd])
            spike_exit = str(spike_proc.returncode)
            spike_out = spike_proc.stdout
            spike_err = spike_proc.stderr
            stdout.write_text(spike_out, encoding="utf-8")
            stderr.write_text(spike_err, encoding="utf-8")
            try:
                encoded_i, decoded_unsigned_i, decoded_signed_i = decode_tohost(spike_out + spike_err)
                encoded = f"0x{encoded_i:x}"
                decoded_unsigned = str(decoded_unsigned_i)
                decoded_signed = str(decoded_signed_i)
                status = "PASS" if decoded_signed_i == expected else "FAIL"
            except ValueError as exc:
                spike_err = spike_err + f"\nDECODE_ERROR: {exc}\n"
        else:
            stderr.write_text(link_proc.stderr, encoding="utf-8")

        if status != "PASS":
            ok = False
        with report.open("a", encoding="utf-8") as f:
            f.write(
                "|".join(
                    [
                        name,
                        str(expected),
                        str(link_exit),
                        spike_exit,
                        str(encoded),
                        str(decoded_unsigned),
                        str(decoded_signed),
                        str(asm),
                        link_cmd,
                        spike_cmd,
                        str(stdout),
                        str(stderr),
                        status,
                    ]
                )
                + "\n"
            )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
