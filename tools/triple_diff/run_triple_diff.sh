#!/usr/bin/env bash
set -u

repo="${1:?repo path required}"
java_root="${2:?java root required}"
src2_root="${3:?src2 root required}"
new_cpp="${4:?new compiler path required}"
cases_root="${5:?cases root required}"
reports_dir="${6:?reports dir required}"
no_build_java="${7:-0}"
no_build_src2="${8:-0}"

gcc="riscv64-unknown-elf-gcc"
spike="$HOME/.local/bin/spike"
start_file="$repo/tools/triple_diff/rv32_split_tohost_start.s"
link_file="$repo/tools/triple_diff/rv32_split_tohost_link.ld"
runtime_dir="$reports_dir/runtime"
summary="$reports_dir/latest.md"
rows_file="$runtime_dir/rows.psv"
src2_bin="$runtime_dir/src2_toycc"

mkdir -p "$runtime_dir"
: >"$rows_file"

count_opcode() {
  local asm="$1"
  local op="$2"
  [[ -f "$asm" ]] && awk -v op="$op" '$1 == op { count++ } END { print count + 0 }' "$asm" || echo 0
}

count_lines() {
  local asm="$1"
  [[ -f "$asm" ]] && awk '$0 !~ /^[[:space:]]*($|[.#])/ { count++ } END { print count + 0 }' "$asm" || echo 0
}

emit_row() {
  local status="$1"
  local reason="$2"
  local actual_unsigned="${3:-}"
  local actual_signed="${4:-}"
  local lw="${5:-}"
  local sw="${6:-}"
  local jal="${7:-}"
  local call="${8:-}"
  local ret="${9:-}"
  local mul="${10:-}"
  local div="${11:-}"
  local rem="${12:-}"
  local lines="${13:-}"
  local asm="${14:-}"
  local spike_log="${15:-}"
  local result="${16:-}"
  printf '%s|%s|%s|%s|||%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n' \
    "$status" "$reason" "$actual_unsigned" "$actual_signed" \
    "$lw" "$sw" "$jal" "$call" "$ret" "$mul" "$div" "$rem" "$lines" \
    "$asm" "$spike_log" "$result"
}

extract_expected() {
  local tc="$1"
  local base="${tc%.tc}"
  local expected="${base}.expected.exit"
  if [[ -f "$expected" ]]; then
    tr -d '\r\n ' <"$expected"
  else
    echo ""
  fi
}

extract_expected_status() {
  local tc="$1"
  local base="${tc%.tc}"
  local expected="${base}.expected.status"
  if [[ -f "$expected" ]]; then
    tr -d '\r\n ' <"$expected"
  else
    echo ""
  fi
}

extract_expected_stderr() {
  local tc="$1"
  local base="${tc%.tc}"
  local expected="${base}.expected.stderr"
  if [[ -f "$expected" ]]; then
    tr -d '\r' <"$expected"
  else
    echo ""
  fi
}

signed32() {
  python3 - "$1" <<'PY'
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
m = re.search(r"tohost\s*=\s*(0x[0-9a-fA-F]+|-?\d+)", text)
if not m:
    sys.exit(1)
encoded = int(m.group(1), 0) & 0xffffffffffffffff
low = encoded & 0xffffffff
high31 = (encoded >> 32) & 0x7fffffff
if low not in (2, 6):
    sys.exit(1)
sign = 1 if low == 6 else 0
value = high31 | (sign << 31)
signed = value - 0x100000000 if value >= 0x80000000 else value
print(f"{value} {signed}")
PY
}

build_java() {
  if [[ "$no_build_java" == "1" ]]; then
    return 0
  fi
  (cd "$java_root" && mvn -q -DskipTests package) >"$runtime_dir/java_build.log" 2>&1
}

find_java_jar() {
  local jar
  jar="$(find "$java_root/target" -maxdepth 1 -name "*.jar" ! -name "original-*.jar" | sort | head -n 1)"
  [[ -n "$jar" ]] && echo "$jar"
}

build_src2() {
  if [[ "$no_build_src2" == "1" && -x "$src2_bin" ]]; then
    return 0
  fi
  (
    cd "$repo" || exit 1
    mapfile -t sources < <(find src2 -name "*.cpp" | sort)
    g++ -std=c++20 -O2 -Isrc2 "${sources[@]}" -o "$src2_bin"
  ) >"$runtime_dir/src2_build.log" 2>&1
}

run_compiler() {
  local compiler="$1"
  local case_file="$2"
  local name
  name="$(basename "$case_file" .tc)"
  local safe_case
  safe_case="$(basename "$(dirname "$case_file")")_${name}"
  local prefix="$runtime_dir/${compiler}_${safe_case}"
  local asm="$prefix.s"
  local compile_log="$prefix.compile.log"

  case "$compiler" in
    java)
      local jar
      jar="$(find_java_jar || true)"
      if [[ -z "$jar" ]]; then
        emit_row "SKIP" "missing Java jar" "" "" "" "" "" "" "" "" "" "" "" "$asm" "" "$prefix.result.txt"
        return 0
      fi
      timeout 20s java -jar "$jar" -opt <"$case_file" >"$asm" 2>"$compile_log"
      ;;
    src2)
      if [[ ! -x "$src2_bin" ]]; then
        emit_row "SKIP" "missing src2 compiler" "" "" "" "" "" "" "" "" "" "" "" "$asm" "" "$prefix.result.txt"
        return 0
      fi
      timeout 20s "$src2_bin" -opt=o3 <"$case_file" >"$asm" 2>"$compile_log"
      ;;
    newcpp)
      if [[ ! -x "$new_cpp" ]]; then
        emit_row "SKIP" "missing new C++ compiler" "" "" "" "" "" "" "" "" "" "" "" "$asm" "" "$prefix.result.txt"
        return 0
      fi
      timeout 20s "$new_cpp" -opt <"$case_file" >"$asm" 2>"$compile_log"
      ;;
    newcpp_native)
      if [[ ! -x "$new_cpp" ]]; then
        emit_row "SKIP" "missing new C++ compiler" "" "" "" "" "" "" "" "" "" "" "" "$asm" "" "$prefix.result.txt"
        return 0
      fi
      timeout 20s "$new_cpp" <"$case_file" >"$asm" 2>"$compile_log"
      ;;
  esac
  local status=$?
  if [[ $status -ne 0 ]]; then
    emit_row "FAIL" "compile failure" "" "" \
      "$(count_opcode "$asm" lw)" "$(count_opcode "$asm" sw)" \
      "$(count_opcode "$asm" jal)" "$(count_opcode "$asm" call)" "$(count_opcode "$asm" ret)" \
      "$(count_opcode "$asm" mul)" "$(count_opcode "$asm" div)" "$(count_opcode "$asm" rem)" \
      "$(count_lines "$asm")" "$asm" "" "$prefix.result.txt"
    return 0
  fi

  local elf="$prefix.elf"
  local link_log="$prefix.link.log"
  "$gcc" -march=rv32im -mabi=ilp32 -nostdlib -static -T "$link_file" -o "$elf" "$start_file" "$asm" >"$link_log" 2>&1
  if [[ $? -ne 0 ]]; then
    emit_row "FAIL" "link failure" "" "" \
      "$(count_opcode "$asm" lw)" "$(count_opcode "$asm" sw)" \
      "$(count_opcode "$asm" jal)" "$(count_opcode "$asm" call)" "$(count_opcode "$asm" ret)" \
      "$(count_opcode "$asm" mul)" "$(count_opcode "$asm" div)" "$(count_opcode "$asm" rem)" \
      "$(count_lines "$asm")" "$asm" "" "$prefix.result.txt"
    return 0
  fi

  local spike_log="$prefix.spike.log"
  timeout 20s "$spike" --isa=rv32im "$elf" >"$spike_log" 2>&1
  if [[ $? -eq 124 ]]; then
    emit_row "FAIL" "timeout" "" "" \
      "$(count_opcode "$asm" lw)" "$(count_opcode "$asm" sw)" \
      "$(count_opcode "$asm" jal)" "$(count_opcode "$asm" call)" "$(count_opcode "$asm" ret)" \
      "$(count_opcode "$asm" mul)" "$(count_opcode "$asm" div)" "$(count_opcode "$asm" rem)" \
      "$(count_lines "$asm")" "$asm" "$spike_log" "$prefix.result.txt"
    return 0
  fi

  local parsed actual_unsigned actual_signed
  if ! parsed="$(parse_tohost "$spike_log")"; then
    emit_row "FAIL" "tohost parse failure" "" "" \
      "$(count_opcode "$asm" lw)" "$(count_opcode "$asm" sw)" \
      "$(count_opcode "$asm" jal)" "$(count_opcode "$asm" call)" "$(count_opcode "$asm" ret)" \
      "$(count_opcode "$asm" mul)" "$(count_opcode "$asm" div)" "$(count_opcode "$asm" rem)" \
      "$(count_lines "$asm")" "$asm" "$spike_log" "$prefix.result.txt"
    return 0
  fi
  actual_unsigned="${parsed%% *}"
  actual_signed="${parsed##* }"
  emit_row "PASS" "" "$actual_unsigned" "$actual_signed" \
    "$(count_opcode "$asm" lw)" "$(count_opcode "$asm" sw)" \
    "$(count_opcode "$asm" jal)" "$(count_opcode "$asm" call)" "$(count_opcode "$asm" ret)" \
    "$(count_opcode "$asm" mul)" "$(count_opcode "$asm" div)" "$(count_opcode "$asm" rem)" \
    "$(count_lines "$asm")" "$asm" "$spike_log" "$prefix.result.txt"
}

run_diagnostic_compiler() {
  local compiler="$1"
  local case_file="$2"
  local expected_status="$3"
  local expected_stderr="$4"
  local name
  name="$(basename "$case_file" .tc)"
  local safe_case
  safe_case="$(basename "$(dirname "$case_file")")_${name}"
  local prefix="$runtime_dir/${compiler}_${safe_case}"
  local asm="$prefix.s"
  local compile_log="$prefix.compile.log"

  case "$compiler" in
    java)
      local jar
      jar="$(find_java_jar || true)"
      if [[ -z "$jar" ]]; then
        emit_row "SKIP" "missing Java jar" "" "" "" "" "" "" "" "" "" "" "" "$asm" "" "$prefix.result.txt"
        return 0
      fi
      timeout 20s java -jar "$jar" -opt <"$case_file" >"$asm" 2>"$compile_log"
      ;;
    newcpp)
      if [[ ! -x "$new_cpp" ]]; then
        emit_row "SKIP" "missing new C++ compiler" "" "" "" "" "" "" "" "" "" "" "" "$asm" "" "$prefix.result.txt"
        return 0
      fi
      timeout 20s "$new_cpp" -opt <"$case_file" >"$asm" 2>"$compile_log"
      ;;
    newcpp_native)
      if [[ ! -x "$new_cpp" ]]; then
        emit_row "SKIP" "missing new C++ compiler" "" "" "" "" "" "" "" "" "" "" "" "$asm" "" "$prefix.result.txt"
        return 0
      fi
      timeout 20s "$new_cpp" <"$case_file" >"$asm" 2>"$compile_log"
      ;;
    src2)
      emit_row "SKIP" "diagnostic case not run for src2" "" "" "" "" "" "" "" "" "" "" "" "$asm" "" "$prefix.result.txt"
      return 0
      ;;
  esac

  local status=$?
  if [[ "$expected_status" == "compile_error" && $status -ne 0 ]]; then
    if [[ "$compiler" == "java" ]] || [[ -z "$expected_stderr" ]] || grep -Fq "$expected_stderr" "$compile_log"; then
      emit_row "PASS" "diagnostic ok" "" "" "" "" "" "" "" "" "" "" "" "$asm" "$compile_log" "$prefix.result.txt"
    else
      emit_row "FAIL" "diagnostic stderr mismatch" "" "" "" "" "" "" "" "" "" "" "" "$asm" "$compile_log" "$prefix.result.txt"
    fi
  else
    emit_row "FAIL" "diagnostic status mismatch" "" "" "" "" "" "" "" "" "" "" "" "$asm" "$compile_log" "$prefix.result.txt"
  fi
}

{
  echo "# Triple compiler diff report"
  echo
  echo "- generated_at: $(date -Is)"
  echo "- repo: $repo"
  echo "- java_root: $java_root"
  echo "- src2_root: $src2_root"
  echo "- new_cpp: $new_cpp"
  echo "- cases_root: $cases_root"
  echo "- gcc: $(command -v "$gcc" || true)"
  echo "- spike: $spike"
  echo
} >"$summary"

missing=0
for required in "$spike" "$start_file" "$link_file"; do
  if [[ ! -e "$required" ]]; then
    echo "- missing required path: $required" >>"$summary"
    missing=1
  fi
done
if ! command -v "$gcc" >/dev/null 2>&1; then
  echo "- missing required command: $gcc" >>"$summary"
  missing=1
fi
if [[ $missing -ne 0 ]]; then
  exit 2
fi

build_java || true
build_src2 || true

echo "| case | expected | compiler | status | reason | actual signed | lw | sw | call | mul | div | rem | asm lines |" >>"$summary"
echo "| --- | ---: | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |" >>"$summary"

case_root="${cases_root%/}"
root_pattern="$case_root/*.tc"
smoke_pattern="$case_root/smoke/*.tc"
bench_pattern="$case_root/bench/*.tc"

while IFS= read -r -d '' case_file; do
  rel="${case_file#$cases_root/}"
  expected="$(extract_expected "$case_file")"
  expected_status="$(extract_expected_status "$case_file")"
  expected_stderr="$(extract_expected_stderr "$case_file")"
  for compiler in java src2 newcpp newcpp_native; do
    if [[ -n "$expected_status" ]]; then
      row="$(run_diagnostic_compiler "$compiler" "$case_file" "$expected_status" "$expected_stderr")"
    else
      row="$(run_compiler "$compiler" "$case_file")"
    fi
    IFS='|' read -r status reason actual_unsigned actual_signed _ _ lw sw jal call ret mul div rem lines asm spike_log result <<<"$row"
    echo "$rel|$expected|$compiler|$status|$reason|$actual_unsigned|$actual_signed|$lw|$sw|$jal|$call|$ret|$mul|$div|$rem|$lines|$asm|$spike_log|$result" >>"$rows_file"
    echo "| $rel | ${expected:-} | $compiler | $status | $reason | ${actual_signed:-} | ${lw:-} | ${sw:-} | ${call:-} | ${mul:-} | ${div:-} | ${rem:-} | ${lines:-} |" >>"$summary"
  done
done < <(find "$case_root" -type f \( -path "$root_pattern" -o -path "$smoke_pattern" -o -path "$bench_pattern" \) -print0 | sort -z)

echo >>"$summary"
echo "## Behavior comparison" >>"$summary"
echo >>"$summary"
echo "| case | java | src2 | newcpp | judgment |" >>"$summary"
echo "| --- | ---: | ---: | ---: | --- |" >>"$summary"

cut -d'|' -f1 "$rows_file" | sort -u | while read -r case_name; do
  java_actual="$(awk -F'|' -v c="$case_name" '$1==c && $3=="java"{print $7; exit}' "$rows_file")"
  src2_actual="$(awk -F'|' -v c="$case_name" '$1==c && $3=="src2"{print $7; exit}' "$rows_file")"
  new_actual="$(awk -F'|' -v c="$case_name" '$1==c && $3=="newcpp"{print $7; exit}' "$rows_file")"
  judgment="consistent"
  if [[ -z "$java_actual" || -z "$src2_actual" ]]; then
    judgment="insufficient Java/src2 evidence"
  elif [[ "$java_actual" != "$src2_actual" ]]; then
    judgment="java-src2 differs"
  elif [[ -n "$new_actual" && "$new_actual" != "$java_actual" ]]; then
    judgment="newcpp differs"
  elif [[ -z "$new_actual" ]]; then
    judgment="newcpp skipped"
  fi
  echo "| $case_name | $java_actual | $src2_actual | $new_actual | $judgment |" >>"$summary"
done
