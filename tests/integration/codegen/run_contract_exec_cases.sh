#!/usr/bin/env bash
set -euo pipefail

build_dir="build"
compiler=""
runner=""
march="rv32im"
mabi="ilp32"
opt_only=0

usage() {
    cat <<'EOF'
Usage: bash tests/integration/codegen/run_contract_exec_cases.sh [options]

Options:
  --build-dir DIR     Build directory containing codegen_contract_exec_cases.
  --compiler PATH     RISC-V Linux GCC. Default: auto-detect.
  --runner PATH       qemu-riscv32 runner. Default: auto-detect.
  --march VALUE       GCC -march value. Default: rv32im.
  --mabi VALUE        GCC -mabi value. Default: ilp32.
  --opt-only          Run only the --opt cases.
  -h, --help          Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            build_dir="$2"
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
repo_root="$(cd "$script_dir/../../.." && pwd)"
build_root="$repo_root/$build_dir"

case_emitter="$build_root/codegen_contract_exec_cases"
if [[ ! -e "$case_emitter" ]]; then
    case_emitter="$build_root/codegen_contract_exec_cases.exe"
fi
if [[ ! -e "$case_emitter" ]]; then
    echo "Missing codegen_contract_exec_cases." >&2
    echo "Run from PowerShell first: cmake --build $build_dir --target codegen_contract_exec_cases" >&2
    exit 1
fi

cc="$(resolve_tool "$compiler" "compiler" \
    riscv32-unknown-linux-gnu-gcc \
    riscv64-unknown-linux-gnu-gcc \
    riscv64-linux-gnu-gcc)"
qemu="$(resolve_tool "$runner" "runner" qemu-riscv32)"

work_dir="$build_root/codegen-contract-exec"
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

cases=(
    "basic_return:42"
    "loop_sum:6"
    "many_params:55"
    "global_var:12"
    "recursion:120"
)

modes=("")
if [[ "$opt_only" -eq 1 ]]; then
    modes=("--opt")
else
    modes+=("--opt")
fi

for case_entry in "${cases[@]}"; do
    case_name="${case_entry%%:*}"
    expected="${case_entry##*:}"

    for mode in "${modes[@]}"; do
        suffix="default"
        if [[ "$mode" == "--opt" ]]; then
            suffix="opt"
        fi

        asm="$work_dir/$case_name-$suffix.s"
        elf="$work_dir/$case_name-$suffix.elf"
        emit_args=(--case "$case_name")
        if [[ -n "$mode" ]]; then
            emit_args+=("$mode")
        fi

        "$case_emitter" "${emit_args[@]}" >"$asm"

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

        if [[ "$actual" -ne "$expected" ]]; then
            echo "FAIL $case_name $suffix expected exit=$expected got=$actual" >&2
            exit 1
        fi

        echo "PASS $case_name $suffix exit=$actual"
    done
done
