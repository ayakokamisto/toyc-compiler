#!/usr/bin/env bash
set -u

repo="${1:?repo path required}"
toycc="${2:?src toycc path required}"
cases_dir="${3:?cases path required}"
reports_dir="${4:?reports path required}"

gcc="riscv64-unknown-elf-gcc"
spike="$HOME/.local/bin/spike"
start_file="/tmp/rvtest_start.s"
link_file="/tmp/rvtest_link.ld"
src2_bin="/tmp/toyc_src2_runtime/src2_toycc"
runtime_dir="$reports_dir/runtime"
summary="$reports_dir/latest_runtime.md"

mkdir -p "$runtime_dir" "$(dirname "$src2_bin")"

quote_cmd() {
  printf '%q ' "$@"
}

extract_expect() {
  local file="$1"
  sed -nE 's/^[[:space:]]*\/\/[[:space:]]*EXPECT:[[:space:]]*(-?[0-9]+).*/\1/p' "$file" | head -n 1
}

count_opcode() {
  local asm="$1"
  local op="$2"
  if [[ -f "$asm" ]]; then
    grep -Ec "^[[:space:]]*$op([[:space:]]|$)" "$asm" || true
  else
    echo 0
  fi
}

count_asm_lines() {
  local asm="$1"
  if [[ -f "$asm" ]]; then
    grep -Ec "^[[:space:]]*[^.#[:space:]]" "$asm" || true
  else
    echo 0
  fi
}

dead_writebacks_removed() {
  local log="$1"
  if [[ -f "$log" ]]; then
    local removed suppressed
    removed="$(tr -d '\r' <"$log" | sed -nE 's/^dead_writebacks_removed=([0-9]+)$/\1/p' | tail -n 1)"
    suppressed="$(tr -d '\r' <"$log" | sed -nE 's/^vreg_home_writebacks_suppressed=([0-9]+)$/\1/p' | tail -n 1)"
    removed="${removed:-0}"
    suppressed="${suppressed:-0}"
    echo $((removed + suppressed))
  else
    echo 0
  fi
}

signed32() {
  local value="$1"
  python3 - "$value" <<'PY'
import sys
v = int(sys.argv[1]) & 0xffffffff
if v >= 0x80000000:
    v -= 0x100000000
print(v)
PY
}

parse_tohost() {
  local log="$1"
  python3 - "$log" <<'PY'
import re
import sys
text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
m = re.search(r"tohost\s*=\s*(0x[0-9a-fA-F]+|-?\d+)(?:\s*\(=\s*(-?\d+)\))?", text)
if not m:
    sys.exit(1)
raw = m.group(2) if m.group(2) is not None else m.group(1)
value = int(raw, 0) & 0xffffffff
if "*** FAILED ***" in text:
    value = ((value << 1) | 1) & 0xffffffff
signed = value - 0x100000000 if value >= 0x80000000 else value
print(f"{value} {signed}")
PY
}

compile_src2() {
  local log="$runtime_dir/src2_build.log"
  (
    cd "$repo" || exit 1
    mapfile -t sources < <(find src2 -name "*.cpp" | sort)
    g++ -std=c++20 -O2 -Isrc2 "${sources[@]}" -o "$src2_bin"
  ) >"$log" 2>&1
}

classify_link_failure() {
  local asm="$1"
  local err="$2"
  if grep -Eq "^[[:space:]]*(mul|div|rem)([[:space:]]|$)" "$asm" 2>/dev/null ||
     grep -Eiq "extension|unrecognized opcode|opcode" "$err" 2>/dev/null; then
    echo "RV32I instruction-set violation"
  else
    echo "RISC-V assembly/link failure"
  fi
}

run_one() {
  local compiler_name="$1"
  local compiler_path="$2"
  local case_file="$3"
  local name
  name="$(basename "$case_file" .tc)"
  local prefix="$runtime_dir/${compiler_name}_${name}"
  local src_copy="$prefix.tc"
  local asm="$prefix.s"
  local elf="$prefix.elf"
  local compile_log="$prefix.compile.log"
  local link_log="$prefix.link.log"
  local spike_log="$prefix.spike.log"
  local result="$prefix.result.txt"
  local expected
  expected="$(extract_expect "$case_file")"
  if [[ -z "$expected" ]]; then
    expected="MISSING"
  fi
  cp "$case_file" "$src_copy"

  local compile_cmd=("$compiler_path" "-opt")
  local compile_env=()
  if [[ "$compiler_name" == "src2" ]]; then
    compile_cmd=("$compiler_path" "-opt=o3")
  elif [[ "$compiler_name" == "src" ]]; then
    compile_env=("TOYC_MIR_DEAD_WRITEBACK_STATS=1")
  fi

  env "${compile_env[@]}" "${compile_cmd[@]}" <"$case_file" >"$asm" 2>"$compile_log"
  local compile_status=$?
  local removed
  removed="$(dead_writebacks_removed "$compile_log")"
  if [[ -z "$removed" ]]; then removed=0; fi
  if [[ $compile_status -ne 0 ]]; then
    cat >"$result" <<EOF
status=FAIL
classification=Toy-C compile failure
expected=$expected
actual_unsigned=
actual_signed=
compile_command=$(quote_cmd "${compile_cmd[@]}")
link_command=
spike_command=
EOF
    echo "$compiler_name|$name|$expected|FAIL|Toy-C compile failure|||$(count_opcode "$asm" lw)|$(count_opcode "$asm" sw)|$(count_opcode "$asm" jal)|$(count_opcode "$asm" call)|$(count_opcode "$asm" ret)|$(count_opcode "$asm" mul)|$(count_opcode "$asm" div)|$(count_opcode "$asm" rem)|$(count_asm_lines "$asm")|$removed|$asm|$spike_log|$result"
    return 0
  fi

  local link_cmd=("$gcc" "-march=rv32i" "-mabi=ilp32" "-nostdlib" "-static" "-T" "$link_file" "-o" "$elf" "$start_file" "$asm")
  "${link_cmd[@]}" >"$link_log" 2>&1
  local link_status=$?
  if [[ $link_status -ne 0 ]]; then
    local cls
    cls="$(classify_link_failure "$asm" "$link_log")"
    cat >"$result" <<EOF
status=FAIL
classification=$cls
expected=$expected
actual_unsigned=
actual_signed=
compile_command=$(quote_cmd "${compile_cmd[@]}")
link_command=$(quote_cmd "${link_cmd[@]}")
spike_command=
EOF
    echo "$compiler_name|$name|$expected|FAIL|$cls|||$(count_opcode "$asm" lw)|$(count_opcode "$asm" sw)|$(count_opcode "$asm" jal)|$(count_opcode "$asm" call)|$(count_opcode "$asm" ret)|$(count_opcode "$asm" mul)|$(count_opcode "$asm" div)|$(count_opcode "$asm" rem)|$(count_asm_lines "$asm")|$removed|$asm|$spike_log|$result"
    return 0
  fi

  local spike_cmd=("$spike" "--isa=rv32i" "$elf")
  timeout 20s "${spike_cmd[@]}" >"$spike_log" 2>&1
  local spike_status=$?
  if [[ $spike_status -eq 124 ]]; then
    cat >"$result" <<EOF
status=FAIL
classification=timeout or non-termination
expected=$expected
actual_unsigned=
actual_signed=
compile_command=$(quote_cmd "${compile_cmd[@]}")
link_command=$(quote_cmd "${link_cmd[@]}")
spike_command=$(quote_cmd "${spike_cmd[@]}")
EOF
    echo "$compiler_name|$name|$expected|FAIL|timeout or non-termination|||$(count_opcode "$asm" lw)|$(count_opcode "$asm" sw)|$(count_opcode "$asm" jal)|$(count_opcode "$asm" call)|$(count_opcode "$asm" ret)|$(count_opcode "$asm" mul)|$(count_opcode "$asm" div)|$(count_opcode "$asm" rem)|$(count_asm_lines "$asm")|$removed|$asm|$spike_log|$result"
    return 0
  fi
  local parsed
  if ! parsed="$(parse_tohost "$spike_log")"; then
    if [[ $spike_status -ne 0 ]]; then
      cat >"$result" <<EOF
status=FAIL
classification=Spike execution failure
expected=$expected
actual_unsigned=
actual_signed=
compile_command=$(quote_cmd "${compile_cmd[@]}")
link_command=$(quote_cmd "${link_cmd[@]}")
spike_command=$(quote_cmd "${spike_cmd[@]}")
EOF
      echo "$compiler_name|$name|$expected|FAIL|Spike execution failure|||$(count_opcode "$asm" lw)|$(count_opcode "$asm" sw)|$(count_opcode "$asm" jal)|$(count_opcode "$asm" call)|$(count_opcode "$asm" ret)|$(count_opcode "$asm" mul)|$(count_opcode "$asm" div)|$(count_opcode "$asm" rem)|$(count_asm_lines "$asm")|$removed|$asm|$spike_log|$result"
      return 0
    fi
    cat >"$result" <<EOF
status=FAIL
classification=tohost parse failure
expected=$expected
actual_unsigned=
actual_signed=
compile_command=$(quote_cmd "${compile_cmd[@]}")
link_command=$(quote_cmd "${link_cmd[@]}")
spike_command=$(quote_cmd "${spike_cmd[@]}")
EOF
    echo "$compiler_name|$name|$expected|FAIL|tohost parse failure|||$(count_opcode "$asm" lw)|$(count_opcode "$asm" sw)|$(count_opcode "$asm" jal)|$(count_opcode "$asm" call)|$(count_opcode "$asm" ret)|$(count_opcode "$asm" mul)|$(count_opcode "$asm" div)|$(count_opcode "$asm" rem)|$(count_asm_lines "$asm")|$removed|$asm|$spike_log|$result"
    return 0
  fi

  local actual_unsigned actual_signed expected_signed status cls
  actual_unsigned="${parsed%% *}"
  actual_signed="${parsed##* }"
  expected_signed="$(signed32 "$expected")"
  if [[ "$actual_signed" == "$expected_signed" ]]; then
    status="PASS"
    cls=""
  else
    status="FAIL"
    cls="wrong runtime result"
  fi

  cat >"$result" <<EOF
status=$status
classification=$cls
expected=$expected
expected_signed=$expected_signed
actual_unsigned=$actual_unsigned
actual_signed=$actual_signed
compile_command=$(quote_cmd "${compile_cmd[@]}")
link_command=$(quote_cmd "${link_cmd[@]}")
spike_command=$(quote_cmd "${spike_cmd[@]}")
EOF
  echo "$compiler_name|$name|$expected|$status|$cls|$actual_unsigned|$actual_signed|$(count_opcode "$asm" lw)|$(count_opcode "$asm" sw)|$(count_opcode "$asm" jal)|$(count_opcode "$asm" call)|$(count_opcode "$asm" ret)|$(count_opcode "$asm" mul)|$(count_opcode "$asm" div)|$(count_opcode "$asm" rem)|$(count_asm_lines "$asm")|$removed|$asm|$spike_log|$result"
}

{
  echo "# src backend Spike runtime report"
  echo
  echo "- generated_at: $(date -Is)"
  echo "- repo: $repo"
  echo "- src_toycc: $toycc"
  echo "- src2_toycc: $src2_bin"
  echo "- gcc_path: $(command -v "$gcc" || true)"
  echo "- gcc_version: $("$gcc" --version | head -n 1)"
  echo "- spike_path: $spike"
  echo "- spike_help: $("$spike" --help 2>&1 | head -n 1)"
  echo "- start_file: $start_file"
  echo "- link_file: $link_file"
  echo "- runtime_dir: $runtime_dir"
  echo
} >"$summary"

missing=0
for required in "$toycc" "$spike" "$start_file" "$link_file"; do
  if [[ ! -e "$required" ]]; then
    echo "missing required path: $required" | tee -a "$summary"
    missing=1
  fi
done
if ! command -v "$gcc" >/dev/null 2>&1; then
  echo "missing required command: $gcc" | tee -a "$summary"
  missing=1
fi
if [[ $missing -ne 0 ]]; then
  exit 2
fi

echo "## src2 build" >>"$summary"
if compile_src2; then
  echo "- status: pass" >>"$summary"
else
  echo "- status: fail" >>"$summary"
  echo "- log: $runtime_dir/src2_build.log" >>"$summary"
fi
echo >>"$summary"

echo "| compiler | case | expected | status | classification | actual unsigned 32-bit | actual signed 32-bit | lw | sw | jal | call | ret | mul | div | rem | asm lines | dead writebacks removed |" >>"$summary"
echo "| --- | --- | ---: | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |" >>"$summary"

rows_file="$runtime_dir/rows.psv"
: >"$rows_file"
while IFS= read -r -d '' case_file; do
  run_one "src" "$toycc" "$case_file" | tee -a "$rows_file" >/dev/null
  if [[ -x "$src2_bin" ]]; then
    run_one "src2" "$src2_bin" "$case_file" | tee -a "$rows_file" >/dev/null
  fi
done < <(find "$cases_dir" -maxdepth 1 -name "*.tc" -print0 | sort -z)

while IFS='|' read -r compiler name expected status cls actual_unsigned actual_signed lw sw jal call ret mul div rem asm_lines removed asm spike_log result; do
  echo "| $compiler | $name | $expected | $status | $cls | $actual_unsigned | $actual_signed | $lw | $sw | $jal | $call | $ret | $mul | $div | $rem | $asm_lines | $removed |" >>"$summary"
done <"$rows_file"

echo >>"$summary"
echo "## src/src2 behavior comparison" >>"$summary"
echo >>"$summary"
echo "| case | src status | src actual signed | src2 status | src2 actual signed | comparison |" >>"$summary"
echo "| --- | --- | ---: | --- | ---: | --- |" >>"$summary"

cut -d'|' -f2 "$rows_file" | sort -u | while read -r name; do
  src_row="$(grep -m1 "^src|$name|" "$rows_file" || true)"
  src2_row="$(grep -m1 "^src2|$name|" "$rows_file" || true)"
  IFS='|' read -r _ _ _ src_status src_cls _ src_actual _ <<<"$src_row"
  if [[ -n "$src2_row" ]]; then
    IFS='|' read -r _ _ _ src2_status src2_cls _ src2_actual _ <<<"$src2_row"
  else
    src2_status="MISSING"
    src2_actual=""
  fi
  comparison="consistent"
  if [[ "$src_status" == "PASS" && "$src2_status" == "PASS" && "$src_actual" != "$src2_actual" ]]; then
    comparison="different return value"
  elif [[ "$src_status" != "PASS" && "$src2_status" == "PASS" ]]; then
    comparison="src ABI/codegen regression"
  elif [[ "$src_status" != "PASS" && "$src2_status" != "PASS" ]]; then
    comparison="both failed; inspect case validity"
  fi
  echo "| $name | $src_status ${src_cls:+($src_cls)} | $src_actual | $src2_status ${src2_cls:+($src2_cls)} | $src2_actual | $comparison |" >>"$summary"
done

echo >>"$summary"
echo "## Failure evidence" >>"$summary"
echo >>"$summary"
while IFS='|' read -r compiler name expected status cls actual_unsigned actual_signed lw sw jal call ret mul div rem asm_lines removed asm spike_log result; do
  if [[ "$status" == "PASS" ]]; then
    continue
  fi
  echo "### $compiler/$name" >>"$summary"
  echo "- expected: $expected" >>"$summary"
  echo "- actual_unsigned: $actual_unsigned" >>"$summary"
  echo "- actual_signed: $actual_signed" >>"$summary"
  echo "- classification: $cls" >>"$summary"
  echo "- result: $result" >>"$summary"
  echo "- assembly: $asm" >>"$summary"
  echo "- spike_log: $spike_log" >>"$summary"
  echo >>"$summary"
  echo '```asm' >>"$summary"
  grep -E "^[[:space:]]*(call|jal|ret|lw|sw|add|sub|b|j)" "$asm" 2>/dev/null | head -n 40 >>"$summary" || true
  echo '```' >>"$summary"
  echo >>"$summary"
done <"$rows_file"
