#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BENCH_DIR="$REPO_ROOT/src/test/resources/bench"
RUNTIME_START="$REPO_ROOT/src/test/resources/runtime/start_rv32.S"
JAR="$REPO_ROOT/target/toyc.jar"
TEMP_ROOT="${TMPDIR:-/tmp}/toyc-compiler-bench"
RESULT_ROOT="$REPO_ROOT/benchmark-results"
RUNS=5
RUN_NAME="$(date +%Y%m%d-%H%M%S)"
MODE="both"

usage() {
    printf '%s\n' \
        "Usage: scripts/bench.sh [--name NAME] [--runs N] [--mode no-opt|opt|both]" \
        "" \
        "Compiles src/test/resources/bench/*.tc, links RV32 ELF files, runs them" \
        "with qemu-riscv32, checks *.expected.exit, and writes CSV results." \
        "" \
        "Outputs:" \
        "  $TEMP_ROOT/<NAME>/        generated .s and .elf files" \
        "  benchmark-results/<NAME>.raw.csv" \
        "  benchmark-results/<NAME>.summary.csv"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --name)
            RUN_NAME="${2:?missing value for --name}"
            shift 2
            ;;
        --runs)
            RUNS="${2:?missing value for --runs}"
            shift 2
            ;;
        --mode)
            MODE="${2:?missing value for --mode}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'bench.sh: unknown argument: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "$MODE" in
    no-opt|opt|both) ;;
    *)
        printf 'bench.sh: --mode must be no-opt, opt, or both\n' >&2
        exit 2
        ;;
esac

if ! [[ "$RUNS" =~ ^[1-9][0-9]*$ ]]; then
    printf 'bench.sh: --runs must be a positive integer\n' >&2
    exit 2
fi

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'bench.sh: required command not found: %s\n' "$1" >&2
        exit 1
    fi
}

require_command java
require_command mvn
require_command riscv64-unknown-elf-gcc
require_command qemu-riscv32

if [[ ! -f "$JAR" ]]; then
    printf 'bench.sh: compiler jar not found, building with mvn package...\n'
    (cd "$REPO_ROOT" && mvn -q package)
fi

RUN_DIR="$TEMP_ROOT/$RUN_NAME"
RAW_CSV="$RESULT_ROOT/$RUN_NAME.raw.csv"
SUMMARY_CSV="$RESULT_ROOT/$RUN_NAME.summary.csv"
mkdir -p "$RUN_DIR" "$RESULT_ROOT"

printf 'case,mode,run,expected_exit,actual_exit,duration_ms,asm_bytes,elf_bytes\n' > "$RAW_CSV"

compile_case() {
    local tc="$1"
    local mode="$2"
    local name asm elf expected actual start_ns end_ns duration_ms asm_bytes elf_bytes

    name="$(basename "$tc" .tc)"
    expected="$(tr -d '[:space:]' < "$BENCH_DIR/$name.expected.exit")"
    mkdir -p "$RUN_DIR/$name/$mode"
    asm="$RUN_DIR/$name/$mode/$name.s"
    elf="$RUN_DIR/$name/$mode/$name.elf"

    case "$mode" in
        no-opt)
            java -jar "$JAR" < "$tc" > "$asm"
            riscv64-unknown-elf-gcc \
                -march=rv32im \
                -mabi=ilp32 \
                -nostdlib \
                -nostartfiles \
                -Wl,--no-relax \
                "$RUNTIME_START" \
                "$asm" \
                -o "$elf"
            ;;
        opt)
            java -jar "$JAR" -opt < "$tc" > "$asm"
            riscv64-unknown-elf-gcc \
                -march=rv32im \
                -mabi=ilp32 \
                -nostdlib \
                -nostartfiles \
                -Wl,--no-relax \
                "$RUNTIME_START" \
                "$asm" \
                -o "$elf"
            ;;
        gcc-o2)
            riscv64-unknown-elf-gcc \
                -x c \
                -O2 \
                -msmall-data-limit=0 \
                -march=rv32im \
                -mabi=ilp32 \
                -S \
                "$tc" \
                -o "$asm"
            riscv64-unknown-elf-gcc \
                -O2 \
                -msmall-data-limit=0 \
                -march=rv32im \
                -mabi=ilp32 \
                -nostdlib \
                -nostartfiles \
                "$RUNTIME_START" \
                -x c \
                "$tc" \
                -o "$elf"
            ;;
        *)
            printf 'bench.sh: unknown compile mode: %s\n' "$mode" >&2
            exit 2
            ;;
    esac

    asm_bytes="$(wc -c < "$asm" | tr -d '[:space:]')"
    elf_bytes="$(wc -c < "$elf" | tr -d '[:space:]')"

    for ((run = 1; run <= RUNS; run++)); do
        start_ns="$(date +%s%N)"
        set +e
        qemu-riscv32 "$elf" >/dev/null
        actual="$?"
        set -e
        end_ns="$(date +%s%N)"
        duration_ms="$(awk -v start="$start_ns" -v end="$end_ns" 'BEGIN { printf "%.3f", (end - start) / 1000000 }')"

        printf '%s,%s,%d,%s,%s,%s,%s,%s\n' \
            "$name" "$mode" "$run" "$expected" "$actual" "$duration_ms" "$asm_bytes" "$elf_bytes" >> "$RAW_CSV"

        if [[ "$actual" != "$expected" ]]; then
            printf 'bench.sh: %s (%s) run %d expected exit %s but got %s\n' \
                "$name" "$mode" "$run" "$expected" "$actual" >&2
            exit 1
        fi
    done
}

modes_for_run() {
    case "$MODE" in
        no-opt) printf '%s\n' "no-opt" ;;
        opt) printf '%s\n' "opt" ;;
        both) printf '%s\n%s\n' "no-opt" "opt" ;;
    esac
}

shopt -s nullglob
cases=("$BENCH_DIR"/*.tc)
if [[ "${#cases[@]}" -eq 0 ]]; then
    printf 'bench.sh: no benchmark cases found in %s\n' "$BENCH_DIR" >&2
    exit 1
fi

for tc in "${cases[@]}"; do
    name="$(basename "$tc" .tc)"
    if [[ ! -f "$BENCH_DIR/$name.expected.exit" ]]; then
        printf 'bench.sh: missing expected exit file for %s\n' "$tc" >&2
        exit 1
    fi

    printf 'bench.sh: running %s (gcc-o2 baseline, %s runs)\n' "$name" "$RUNS"
    compile_case "$tc" "gcc-o2"

    while IFS= read -r mode; do
        printf 'bench.sh: running %s (%s, %s runs)\n' "$name" "$mode" "$RUNS"
        compile_case "$tc" "$mode"
    done < <(modes_for_run)
done

if command -v python3 >/dev/null 2>&1; then
    python3 - "$RAW_CSV" "$SUMMARY_CSV" <<'PY'
import csv
import statistics
import sys
from collections import defaultdict

raw_path, summary_path = sys.argv[1], sys.argv[2]
groups = defaultdict(list)
metadata = {}

with open(raw_path, newline="", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        key = (row["case"], row["mode"])
        groups[key].append(float(row["duration_ms"]))
        metadata[key] = row

stats = {}
for key, values in groups.items():
    stats[key] = {
        "runs": len(values),
        "median_ms": statistics.median(values),
        "min_ms": min(values),
        "max_ms": max(values),
        "mean_ms": statistics.mean(values),
    }

with open(summary_path, "w", newline="", encoding="utf-8") as f:
    fields = [
        "case",
        "mode",
        "runs",
        "expected_exit",
        "median_ms",
        "min_ms",
        "max_ms",
        "mean_ms",
        "gcc_o2_median_ms",
        "gcc_o2_mean_ms",
        "vs_gcc_o2_median_ratio",
        "vs_gcc_o2_mean_ratio",
        "asm_bytes",
        "elf_bytes",
    ]
    writer = csv.DictWriter(f, fieldnames=fields)
    writer.writeheader()
    for key in sorted(groups):
        case, mode = key
        meta = metadata[key]
        current = stats[key]
        gcc = stats.get((case, "gcc-o2"))
        gcc_median = gcc["median_ms"] if gcc else None
        gcc_mean = gcc["mean_ms"] if gcc else None
        writer.writerow({
            "case": case,
            "mode": mode,
            "runs": current["runs"],
            "expected_exit": meta["expected_exit"],
            "median_ms": f"{current['median_ms']:.3f}",
            "min_ms": f"{current['min_ms']:.3f}",
            "max_ms": f"{current['max_ms']:.3f}",
            "mean_ms": f"{current['mean_ms']:.3f}",
            "gcc_o2_median_ms": f"{gcc_median:.3f}" if gcc_median is not None else "",
            "gcc_o2_mean_ms": f"{gcc_mean:.3f}" if gcc_mean is not None else "",
            "vs_gcc_o2_median_ratio": f"{current['median_ms'] / gcc_median:.3f}" if gcc_median else "",
            "vs_gcc_o2_mean_ratio": f"{current['mean_ms'] / gcc_mean:.3f}" if gcc_mean else "",
            "asm_bytes": meta["asm_bytes"],
            "elf_bytes": meta["elf_bytes"],
        })
PY
else
    printf 'bench.sh: python3 not found; summary CSV was not generated\n' >&2
fi

printf '\nbench.sh: raw results: %s\n' "$RAW_CSV"
if [[ -f "$SUMMARY_CSV" ]]; then
    printf 'bench.sh: summary:     %s\n' "$SUMMARY_CSV"
fi
printf 'bench.sh: temp files:  %s\n' "$RUN_DIR"
