#!/usr/bin/env bash
set -euo pipefail

build_dir="build"
cases_dir=""
compiler=""
runner=""
march="rv32im"
mabi="ilp32"
opt_only=0
default_only=0

usage() {
    cat <<'EOF'
Usage: bash tests/integration/run_toyc_exec_cases.sh [options]

Options:
  --build-dir DIR     Build directory containing toycc or toycc.exe. Default: build.
  --cases-dir DIR     Directory containing *.tc and matching *.expected files.
  --compiler PATH     RISC-V Linux GCC. Default: riscv64-linux-gnu-gcc.
  --runner PATH       qemu-riscv32 runner. Default: qemu-riscv32.
  --march VALUE       GCC -march value. Default: rv32im.
  --mabi VALUE        GCC -mabi value. Default: ilp32.
  --opt-only          Run only -opt mode.
  --default-only      Run only default mode.
  -h, --help          Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --cases-dir)
            cases_dir="$2"
            shift 2
            ;;
        --compiler)
            compiler="$2"
            shift 2
            ;;
        --runner)
            runner="$2"
            shift 2
            ;;
        --march)
            march="$2"
            shift 2
            ;;
        --mabi)
            mabi="$2"
            shift 2
            ;;
        --opt-only)
            opt_only=1
            shift
            ;;
        --default-only)
            default_only=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ "$opt_only" -eq 1 && "$default_only" -eq 1 ]]; then
    echo "--opt-only and --default-only cannot be used together" >&2
    exit 2
fi

resolve_tool() {
    local explicit="$1"
    local kind="$2"
    shift 2

    if [[ -n "$explicit" ]]; then
        if command -v "$explicit" >/dev/null 2>&1; then
            command -v "$explicit"
            return 0
        fi
        echo "$kind not found: $explicit" >&2
        exit 1
    fi

    local candidate
    for candidate in "$@"; do
        if command -v "$candidate" >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
    done

    echo "$kind not found. Install one of: $*" >&2
    exit 1
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
build_root="$repo_root/$build_dir"

if [[ -z "$cases_dir" ]]; then
    cases_dir="$script_dir/cases"
elif [[ "$cases_dir" != /* ]]; then
    cases_dir="$repo_root/$cases_dir"
fi

toycc="$build_root/toycc"
if [[ ! -e "$toycc" ]]; then
    toycc="$build_root/toycc.exe"
fi
if [[ ! -e "$toycc" ]]; then
    echo "Missing toycc executable in $build_root." >&2
    echo "Build first: cmake --build $build_dir --target toycc" >&2
    exit 1
fi

cc="$(resolve_tool "$compiler" "compiler" riscv64-linux-gnu-gcc)"
qemu="$(resolve_tool "$runner" "runner" qemu-riscv32)"

work_dir="$build_root/toyc-exec-cases"
mkdir -p "$work_dir"

crt0="$work_dir/crt0.S"
cat >"$crt0" <<'EOF'
    .section .text
    .global _start
_start:
    call main
    andi a0, a0, 255
    li a7, 93
    ecall
EOF

shopt -s nullglob
case_files=("$cases_dir"/*.tc)
shopt -u nullglob

if [[ "${#case_files[@]}" -eq 0 ]]; then
    echo "No .tc cases found in $cases_dir" >&2
    exit 1
fi

modes=("")
if [[ "$opt_only" -eq 1 ]]; then
    modes=("-opt")
elif [[ "$default_only" -eq 0 ]]; then
    modes+=("-opt")
fi

passed=0
total=0

for case_file in "${case_files[@]}"; do
    case_name="$(basename "$case_file" .tc)"
    expected_file="$cases_dir/$case_name.expected"
    if [[ ! -f "$expected_file" ]]; then
        echo "Missing expected file for $case_name: $expected_file" >&2
        exit 1
    fi
    expected="$(tr -d '[:space:]' <"$expected_file")"
    if [[ ! "$expected" =~ ^[0-9]+$ ]]; then
        echo "Invalid expected exit code in $expected_file: $expected" >&2
        exit 1
    fi

    for mode in "${modes[@]}"; do
        suffix="default"
        if [[ "$mode" == "-opt" ]]; then
            suffix="opt"
        fi

        asm="$work_dir/$case_name-$suffix.s"
        elf="$work_dir/$case_name-$suffix.elf"
        stderr_log="$work_dir/$case_name-$suffix.toycc.stderr"

        toycc_args=()
        if [[ -n "$mode" ]]; then
            toycc_args+=("$mode")
        fi

        "$toycc" "${toycc_args[@]}" <"$case_file" >"$asm" 2>"$stderr_log"

        "$cc" \
            "-march=$march" \
            "-mabi=$mabi" \
            -static \
            -nostdlib \
            -Wl,-e,_start \
            "$crt0" \
            "$asm" \
            -o "$elf"

        set +e
        "$qemu" "$elf"
        actual=$?
        set -e

        total=$((total + 1))
        if [[ "$actual" -ne "$expected" ]]; then
            echo "FAIL $case_name $suffix expected exit=$expected got=$actual" >&2
            echo "assembly: $asm" >&2
            exit 1
        fi

        passed=$((passed + 1))
        echo "PASS $case_name $suffix exit=$actual"
    done
done

echo "PASS $passed/$total ToyC exec checks"
