#!/usr/bin/env python3
"""Measure default vs -opt codegen on ToyC integration cases.

The script is intentionally not wired into CTest because it requires a RISC-V
Linux GCC and qemu-riscv32 when execution checking is enabled.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class AsmStats:
    lines: int = 0
    lw: int = 0
    sw: int = 0
    mv: int = 0
    li: int = 0
    addi_sp: int = 0


@dataclass
class CaseModeResult:
    asm_path: Path
    stats: AsmStats
    actual_exit: int | None
    expected_exit: int

    @property
    def passed(self) -> bool:
        return self.actual_exit == self.expected_exit


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Measure default vs -opt assembly stats for ToyC integration cases."
    )
    parser.add_argument(
        "--build-dir",
        default="build",
        help="Build directory containing toycc or toycc.exe. Default: build.",
    )
    parser.add_argument(
        "--cases-dir",
        default=None,
        help="Directory containing *.tc and matching *.expected files.",
    )
    parser.add_argument(
        "--compiler",
        default=None,
        help="RISC-V Linux GCC. Default: auto-detect riscv64-linux-gnu-gcc.",
    )
    parser.add_argument(
        "--runner",
        default=None,
        help="qemu-riscv32 runner. Default: auto-detect qemu-riscv32.",
    )
    parser.add_argument("--march", default="rv32im", help="GCC -march value.")
    parser.add_argument("--mabi", default="ilp32", help="GCC -mabi value.")
    parser.add_argument(
        "--no-run",
        action="store_true",
        help="Only generate and measure assembly; skip link/run exit-code checks.",
    )
    return parser.parse_args()


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def resolve_path(root: Path, value: str | None, default: Path) -> Path:
    if value is None:
        return default
    path = Path(value)
    if path.is_absolute():
        return path
    return root / path


def resolve_tool(explicit: str | None, candidates: list[str], kind: str) -> str:
    if explicit:
        resolved = shutil.which(explicit)
        if resolved:
            return resolved
        raise SystemExit(f"{kind} not found: {explicit}")
    for candidate in candidates:
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    raise SystemExit(f"{kind} not found. Install one of: {', '.join(candidates)}")


def find_toycc(build_root: Path) -> Path:
    for name in ("toycc", "toycc.exe"):
        candidate = build_root / name
        if candidate.exists():
            return candidate
    raise SystemExit(f"Missing toycc executable in {build_root}. Build target toycc first.")


def run_checked(command: list[str | Path], *, stdin_path: Path | None = None, stdout_path: Path | None = None) -> None:
    stdin_file = stdin_path.open("rb") if stdin_path is not None else None
    stdout_file = stdout_path.open("wb") if stdout_path is not None else None
    try:
        result = subprocess.run(
            [str(part) for part in command],
            stdin=stdin_file,
            stdout=stdout_file,
            stderr=subprocess.PIPE,
            check=False,
        )
    finally:
        if stdin_file is not None:
            stdin_file.close()
        if stdout_file is not None:
            stdout_file.close()
    if result.returncode != 0:
        stderr = result.stderr.decode(errors="replace")
        raise SystemExit(
            f"command failed ({result.returncode}): {' '.join(str(part) for part in command)}\n{stderr}"
        )


def strip_comment(line: str) -> str:
    return line.split("#", 1)[0].strip()


def mnemonic(line: str) -> str | None:
    text = strip_comment(line)
    if not text or text.endswith(":") or text.startswith("."):
        return None
    return text.split(None, 1)[0]


def is_addi_sp(line: str) -> bool:
    text = strip_comment(line)
    if not text.startswith("addi "):
        return False
    operands = text[len("addi ") :].replace(" ", "")
    return operands.startswith("sp,sp,")


def measure_assembly(path: Path) -> AsmStats:
    stats = AsmStats()
    for line in path.read_text(encoding="utf-8").splitlines():
        text = strip_comment(line)
        if text:
            stats.lines += 1
        op = mnemonic(line)
        if op == "lw":
            stats.lw += 1
        elif op == "sw":
            stats.sw += 1
        elif op == "mv":
            stats.mv += 1
        elif op == "li":
            stats.li += 1
        if is_addi_sp(line):
            stats.addi_sp += 1
    return stats


def write_crt0(work_dir: Path) -> Path:
    crt0 = work_dir / "crt0.S"
    crt0.write_text(
        """    .section .text
    .global _start
_start:
    call main
    andi a0, a0, 255
    li a7, 93
    ecall
""",
        encoding="ascii",
    )
    return crt0


def run_elf(runner: str, elf_path: Path) -> int:
    result = subprocess.run([runner, str(elf_path)], check=False)
    return result.returncode


def build_and_run(
    compiler: str,
    runner: str,
    crt0: Path,
    asm_path: Path,
    elf_path: Path,
    march: str,
    mabi: str,
) -> int:
    run_checked(
        [
            compiler,
            f"-march={march}",
            f"-mabi={mabi}",
            "-static",
            "-nostdlib",
            "-Wl,-e,_start",
            crt0,
            asm_path,
            "-o",
            elf_path,
        ]
    )
    return run_elf(runner, elf_path)


def format_delta(opt_value: int, default_value: int) -> str:
    delta = opt_value - default_value
    if delta > 0:
        return f"+{delta}"
    return str(delta)


def print_table(results: list[tuple[str, CaseModeResult, CaseModeResult]], ran: bool) -> None:
    header = (
        "case",
        "ok",
        "lines",
        "lw",
        "sw",
        "mv",
        "li",
        "addi_sp",
    )
    rows: list[tuple[str, str, str, str, str, str, str, str]] = []
    for case_name, default, opt in results:
        ok_text = "skip"
        if ran:
            ok_text = "yes" if default.passed and opt.passed else "NO"
        rows.append(
            (
                case_name,
                ok_text,
                f"{default.stats.lines}->{opt.stats.lines} ({format_delta(opt.stats.lines, default.stats.lines)})",
                f"{default.stats.lw}->{opt.stats.lw} ({format_delta(opt.stats.lw, default.stats.lw)})",
                f"{default.stats.sw}->{opt.stats.sw} ({format_delta(opt.stats.sw, default.stats.sw)})",
                f"{default.stats.mv}->{opt.stats.mv} ({format_delta(opt.stats.mv, default.stats.mv)})",
                f"{default.stats.li}->{opt.stats.li} ({format_delta(opt.stats.li, default.stats.li)})",
                f"{default.stats.addi_sp}->{opt.stats.addi_sp} ({format_delta(opt.stats.addi_sp, default.stats.addi_sp)})",
            )
        )

    widths = [len(value) for value in header]
    for row in rows:
        for index, value in enumerate(row):
            widths[index] = max(widths[index], len(value))

    def render(row: tuple[str, ...]) -> str:
        return "  ".join(value.ljust(widths[index]) for index, value in enumerate(row))

    print(render(header))
    print(render(tuple("-" * width for width in widths)))
    for row in rows:
        print(render(row))


def print_summary(results: list[tuple[str, CaseModeResult, CaseModeResult]], ran: bool) -> None:
    if ran:
        total_checks = len(results) * 2
        passed_checks = sum(1 for _, d, o in results for item in (d, o) if item.passed)
        print(f"\nexecution: {passed_checks}/{total_checks} mode checks passed")
    else:
        print("\nexecution: skipped")

    improved_lw = [name for name, d, o in results if o.stats.lw < d.stats.lw]
    worsened_lines = [name for name, d, o in results if o.stats.lines > d.stats.lines]
    worsened_lw_sw = [
        name
        for name, d, o in results
        if (o.stats.lw + o.stats.sw) > (d.stats.lw + d.stats.sw)
    ]

    print("lw reductions:", ", ".join(improved_lw) if improved_lw else "none")
    print("line-count increases:", ", ".join(worsened_lines) if worsened_lines else "none")
    print("lw+sw increases:", ", ".join(worsened_lw_sw) if worsened_lw_sw else "none")


def measure_case(
    toycc: Path,
    case_path: Path,
    expected: int,
    work_dir: Path,
    mode_name: str,
    toycc_args: list[str],
    *,
    compiler: str | None,
    runner: str | None,
    crt0: Path | None,
    march: str,
    mabi: str,
    run_execution: bool,
) -> CaseModeResult:
    asm_path = work_dir / f"{case_path.stem}-{mode_name}.s"
    elf_path = work_dir / f"{case_path.stem}-{mode_name}.elf"
    run_checked([toycc, *toycc_args], stdin_path=case_path, stdout_path=asm_path)
    stats = measure_assembly(asm_path)

    actual_exit: int | None = None
    if run_execution:
        assert compiler is not None
        assert runner is not None
        assert crt0 is not None
        actual_exit = build_and_run(compiler, runner, crt0, asm_path, elf_path, march, mabi)

    return CaseModeResult(asm_path=asm_path, stats=stats, actual_exit=actual_exit, expected_exit=expected)


def main() -> int:
    args = parse_args()
    root = repo_root()
    build_root = resolve_path(root, args.build_dir, root / "build")
    cases_dir = resolve_path(root, args.cases_dir, root / "tests" / "integration" / "cases")
    toycc = find_toycc(build_root)

    run_execution = not args.no_run
    compiler = None
    runner = None
    if run_execution:
        compiler = resolve_tool(args.compiler, ["riscv64-linux-gnu-gcc"], "compiler")
        runner = resolve_tool(args.runner, ["qemu-riscv32"], "runner")

    work_dir = build_root / "codegen-opt-measure"
    work_dir.mkdir(parents=True, exist_ok=True)
    crt0 = write_crt0(work_dir) if run_execution else None

    case_paths = sorted(cases_dir.glob("*.tc"))
    if not case_paths:
        raise SystemExit(f"No .tc cases found in {cases_dir}")

    results: list[tuple[str, CaseModeResult, CaseModeResult]] = []
    for case_path in case_paths:
        expected_path = case_path.with_suffix(".expected")
        if not expected_path.exists():
            raise SystemExit(f"Missing expected file: {expected_path}")
        expected_text = expected_path.read_text(encoding="utf-8").strip()
        if not expected_text.isdigit():
            raise SystemExit(f"Invalid expected exit code in {expected_path}: {expected_text}")
        expected = int(expected_text)
        if expected < 0 or expected > 255:
            raise SystemExit(f"Expected exit code out of range in {expected_path}: {expected}")

        default = measure_case(
            toycc,
            case_path,
            expected,
            work_dir,
            "default",
            [],
            compiler=compiler,
            runner=runner,
            crt0=crt0,
            march=args.march,
            mabi=args.mabi,
            run_execution=run_execution,
        )
        opt = measure_case(
            toycc,
            case_path,
            expected,
            work_dir,
            "opt",
            ["-opt"],
            compiler=compiler,
            runner=runner,
            crt0=crt0,
            march=args.march,
            mabi=args.mabi,
            run_execution=run_execution,
        )
        results.append((case_path.stem, default, opt))

    print_table(results, run_execution)
    print_summary(results, run_execution)

    if run_execution and any(not d.passed or not o.passed for _, d, o in results):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
