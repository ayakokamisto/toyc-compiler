#!/usr/bin/env bash
set -euo pipefail

build_dir="build"
cases_dir=""
compiler=""
runner=""
timeout_tool=""
march="rv32im"
mabi="ilp32"
timeout_seconds=20
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
  --timeout-tool PATH timeout executable. Default: timeout.
  --march VALUE       GCC -march value. Default: rv32im.
  --mabi VALUE        GCC -mabi value. Default: ilp32.
  --timeout SECONDS   Per-command timeout for toycc, GCC, and QEMU. Default: 20.
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
        --timeout-tool)
            timeout_tool="$2"
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
        --timeout)
            timeout_seconds="$2"
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

if [[ ! "$timeout_seconds" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid --timeout value: $timeout_seconds" >&2
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

quote_command() {
    printf '%q ' "$@"
}

write_command_log() {
    local path="$1"
    shift
    quote_command "$@" >"$path"
    printf '\n' >>"$path"
}

run_with_logs() {
    local step="$1"
    local stdin_path="$2"
    local stdout_path="$3"
    local stderr_path="$4"
    local exit_path="$5"
    local cmd_path="$6"
    shift 6

    write_command_log "$cmd_path" "$@"

    set +e
    if [[ -n "$stdin_path" ]]; then
        "$timeout_tool" "$timeout_seconds" "$@" <"$stdin_path" >"$stdout_path" 2>"$stderr_path"
    else
        "$timeout_tool" "$timeout_seconds" "$@" >"$stdout_path" 2>"$stderr_path"
    fi
    local status=$?
    set -e

    printf '%s\n' "$status" >"$exit_path"
    last_status="$status"
    if [[ "$status" -eq 124 || "$status" -eq 137 ]]; then
        echo "TIMEOUT $step after ${timeout_seconds}s" >&2
        echo "command log: $cmd_path" >&2
        echo "stdout log: $stdout_path" >&2
        echo "stderr log: $stderr_path" >&2
        exit 1
    fi
    return 0
}

require_step_success() {
    local step="$1"
    local status="$2"
    local cmd_path="$3"
    local stdout_path="$4"
    local stderr_path="$5"

    if [[ "$status" -eq 0 ]]; then
        return 0
    fi

    echo "FAIL $step exit=$status" >&2
    echo "command log: $cmd_path" >&2
    echo "stdout log: $stdout_path" >&2
    echo "stderr log: $stderr_path" >&2
    exit 1
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
if [[ "$build_dir" == /* || "$build_dir" =~ ^[A-Za-z]:[\\/] ]]; then
    build_root="$build_dir"
else
    build_root="$repo_root/$build_dir"
fi

if [[ -z "$cases_dir" ]]; then
    cases_dir="$script_dir/cases"
elif [[ "$cases_dir" != /* && ! "$cases_dir" =~ ^[A-Za-z]:[\\/] ]]; then
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
timeout_tool="$(resolve_tool "$timeout_tool" "timeout" timeout)"

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
        toycc_stdout_log="$asm"
        toycc_stderr_log="$work_dir/$case_name-$suffix.toycc.stderr"
        toycc_exit_log="$work_dir/$case_name-$suffix.toycc.exit"
        toycc_cmd_log="$work_dir/$case_name-$suffix.toycc.cmd"
        gcc_stdout_log="$work_dir/$case_name-$suffix.gcc.stdout"
        gcc_stderr_log="$work_dir/$case_name-$suffix.gcc.stderr"
        gcc_exit_log="$work_dir/$case_name-$suffix.gcc.exit"
        gcc_cmd_log="$work_dir/$case_name-$suffix.gcc.cmd"
        qemu_stdout_log="$work_dir/$case_name-$suffix.qemu.stdout"
        qemu_stderr_log="$work_dir/$case_name-$suffix.qemu.stderr"
        qemu_exit_log="$work_dir/$case_name-$suffix.qemu.exit"
        qemu_cmd_log="$work_dir/$case_name-$suffix.qemu.cmd"

        toycc_args=()
        if [[ -n "$mode" ]]; then
            toycc_args+=("$mode")
        fi

        run_with_logs \
            "toycc $case_name $suffix" \
            "$case_file" \
            "$toycc_stdout_log" \
            "$toycc_stderr_log" \
            "$toycc_exit_log" \
            "$toycc_cmd_log" \
            "$toycc" "${toycc_args[@]}"
        toycc_status="$last_status"
        require_step_success \
            "toycc $case_name $suffix" \
            "$toycc_status" \
            "$toycc_cmd_log" \
            "$toycc_stdout_log" \
            "$toycc_stderr_log"

        if [[ ! -s "$asm" ]]; then
            echo "FAIL toycc $case_name $suffix generated empty assembly" >&2
            echo "command log: $toycc_cmd_log" >&2
            echo "stdout log: $toycc_stdout_log" >&2
            echo "stderr log: $toycc_stderr_log" >&2
            exit 1
        fi

        gcc_command=(
            "$cc"
            "-march=$march" \
            "-mabi=$mabi" \
            -static \
            -nostdlib \
            -Wl,-e,_start \
            "$crt0" \
            "$asm" \
            -o "$elf"
        )
        run_with_logs \
            "gcc $case_name $suffix" \
            "" \
            "$gcc_stdout_log" \
            "$gcc_stderr_log" \
            "$gcc_exit_log" \
            "$gcc_cmd_log" \
            "${gcc_command[@]}"
        gcc_status="$last_status"
        require_step_success \
            "gcc $case_name $suffix" \
            "$gcc_status" \
            "$gcc_cmd_log" \
            "$gcc_stdout_log" \
            "$gcc_stderr_log"

        if [[ ! -s "$elf" ]]; then
            echo "FAIL gcc $case_name $suffix did not produce a non-empty ELF" >&2
            echo "command log: $gcc_cmd_log" >&2
            echo "stdout log: $gcc_stdout_log" >&2
            echo "stderr log: $gcc_stderr_log" >&2
            exit 1
        fi

        run_with_logs \
            "qemu $case_name $suffix" \
            "" \
            "$qemu_stdout_log" \
            "$qemu_stderr_log" \
            "$qemu_exit_log" \
            "$qemu_cmd_log" \
            "$qemu" "$elf"
        actual="$last_status"

        total=$((total + 1))
        if [[ "$actual" -ne "$expected" ]]; then
            if [[ -s "$qemu_stderr_log" || "$actual" -eq 126 || "$actual" -eq 127 ]]; then
                echo "FAIL $case_name $suffix qemu startup failed exit=$actual expected_target_exit=$expected" >&2
            else
                echo "FAIL $case_name $suffix target exit mismatch expected=$expected actual=$actual" >&2
            fi
            echo "command log: $qemu_cmd_log" >&2
            echo "stdout log: $qemu_stdout_log" >&2
            echo "stderr log: $qemu_stderr_log" >&2
            echo "exit log: $qemu_exit_log" >&2
            echo "assembly: $asm" >&2
            exit 1
        fi

        passed=$((passed + 1))
        echo "PASS $case_name $suffix expected=$expected actual=$actual"
        echo "logs $case_name $suffix toycc=[$toycc_cmd_log,$toycc_stdout_log,$toycc_stderr_log,$toycc_exit_log] gcc=[$gcc_cmd_log,$gcc_stdout_log,$gcc_stderr_log,$gcc_exit_log] qemu=[$qemu_cmd_log,$qemu_stdout_log,$qemu_stderr_log,$qemu_exit_log]"
    done
done

echo "PASS $passed/$total ToyC exec checks"
