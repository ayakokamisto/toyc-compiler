#!/bin/sh
# Java/src2 dual regression runner
JAVA_JAR="/mnt/e/TOYC/toy-c-compiler-master/target/toyc.jar"
SRC2_BIN="/tmp/src2_fixed"
CASES_NEW="/mnt/e/TOYC/tools/triple_diff/cases"
CASES_SMOKE="/mnt/e/TOYC/toy-c-compiler-master/src/test/resources/smoke"
STARTUP="/tmp/rvtest_start.s"
LINKER_SCRIPT="/tmp/rvtest_link.ld"
SPIKE="/home/ayako/.local/bin/spike"
OUTDIR="/tmp/dual_reg"

mkdir -p "$OUTDIR"

echo "case|compiler|result|asm_lines|lw|sw|call|ret|mul|div|rem|spike_display|dec_unsigned|dec_signed|expect|match" > "$OUTDIR/results.psv"

# Collect all unique .tc files
find "$CASES_NEW" "$CASES_SMOKE" -name "*.tc" -type f | sort -u > /tmp/all_cases.txt

while IFS= read -r tc_file; do
    tc_name=$(basename "$tc_file" .tc)
    dir_name=$(basename "$(dirname "$tc_file")")

    # Find expected exit
    for exp_candidate in "$(dirname "$tc_file")/${tc_name}.expected.exit" "$CASES_NEW/${tc_name}.expected.exit"; do
        if [ -f "$exp_candidate" ]; then
            expect=$(tr -d '\r\n ' < "$exp_candidate")
            break
        fi
    done

    for compiler in java src2; do
        prefix="$OUTDIR/${compiler}_${dir_name}_${tc_name}"
        asm_file="${prefix}.s"
        compile_log="${prefix}.compile.log"
        spike_log="${prefix}.spike.log"

        case "$compiler" in
            java)
                if [ ! -f "$JAVA_JAR" ]; then
                    echo "$tc_name|$compiler|SKIP(missing jar)||||||||||||$expect|" >> "$OUTDIR/results.psv"
                    continue
                fi
                java -jar "$JAVA_JAR" -opt < "$tc_file" > "$asm_file" 2>"$compile_log"
                ;;
            src2)
                if [ ! -x "$SRC2_BIN" ]; then
                    echo "$tc_name|$compiler|SKIP(missing bin)||||||||||||$expect|" >> "$OUTDIR/results.psv"
                    continue
                fi
                "$SRC2_BIN" -opt=o3 < "$tc_file" > "$asm_file" 2>"$compile_log"
                ;;
        esac

        if [ $? -ne 0 ]; then
            al=$(awk '$0 !~ /^[[:space:]]*($|[.#])/ { count++ } END { print count + 0 }' "$asm_file" 2>/dev/null || echo 0)
            echo "$tc_name|$compiler|FAIL(compile)|$al|||||||||||$expect|" >> "$OUTDIR/results.psv"
            continue
        fi

        al=$(awk '$0 !~ /^[[:space:]]*($|[.#])/ { count++ } END { print count + 0 }' "$asm_file")
        lw=$(awk '$1 == "lw" { count++ } END { print count + 0 }' "$asm_file")
        sw=$(awk '$1 == "sw" { count++ } END { print count + 0 }' "$asm_file")
        call=$(awk '$1 == "call" { count++ } END { print count + 0 }' "$asm_file")
        ret=$(awk '$1 == "ret" { count++ } END { print count + 0 }' "$asm_file")
        mul=$(awk '$1 == "mul" { count++ } END { print count + 0 }' "$asm_file")
        div=$(awk '$1 == "div" { count++ } END { print count + 0 }' "$asm_file")
        rem=$(awk '$1 == "rem" { count++ } END { print count + 0 }' "$asm_file")

        # Link
        elf="${prefix}.elf"
        riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static -T "$LINKER_SCRIPT" -o "$elf" "$STARTUP" "$asm_file" 2>"${prefix}.link.log"
        if [ $? -ne 0 ]; then
            echo "$tc_name|$compiler|FAIL(link)|$al|$lw|$sw|$call|$ret|$mul|$div|$rem|||$expect|" >> "$OUTDIR/results.psv"
            continue
        fi

        # Spike
        timeout 10s "$SPIKE" --isa=rv32im "$elf" > "$spike_log" 2>&1
        if [ $? -eq 124 ]; then
            echo "$tc_name|$compiler|FAIL(timeout)|$al|$lw|$sw|$call|$ret|$mul|$div|$rem|timeout||$expect|" >> "$OUTDIR/results.psv"
            continue
        fi

        # Parse tohost
        display=$(cat "$spike_log" | tr '\n' ' ')
        text=$(cat "$spike_log")
        if [ -z "$text" ]; then
            dec_unsigned=0
            dec_signed=0
        else
            tohost_part=$(echo "$text" | grep -o 'tohost = [0-9a-fxA-F-]*' | head -1 | sed 's/tohost = //' | tr -d '\r')
            if [ -z "$tohost_part" ]; then
                echo "$tc_name|$compiler|FAIL(parse)|$al|$lw|$sw|$call|$ret|$mul|$div|$rem|$display||$expect|" >> "$OUTDIR/results.psv"
                continue
            fi
            case "$tohost_part" in
                0x*|0X*) raw_val=$((tohost_part)) ;;
                *) raw_val=$((tohost_part)) ;;
            esac
            raw_val=$((raw_val & 0xffffffff))
            if echo "$text" | grep -q 'FAILED'; then
                raw_val=$(( (raw_val << 1) | 1 ))
                raw_val=$((raw_val & 0xffffffff))
            fi
            dec_unsigned=$raw_val
            dec_signed=$dec_unsigned
            if [ $dec_signed -ge $((0x80000000)) ]; then
                dec_signed=$((dec_signed - 0x100000000))
            fi
        fi

        match=""
        if [ -n "$expect" ]; then
            if [ "$dec_signed" = "$expect" ] || [ "$dec_unsigned" = "$expect" ]; then
                match="MATCH"
            else
                match="MISMATCH"
            fi
        fi
        echo "$tc_name|$compiler|PASS|$al|$lw|$sw|$call|$ret|$mul|$div|$rem|$display|$dec_unsigned|$dec_signed|$expect|$match" >> "$OUTDIR/results.psv"
    done
done < /tmp/all_cases.txt

# Print summary table
echo ""
echo "============================================================"
echo "  DUAL REGRESSION SUMMARY"
echo "============================================================"
printf "%-30s %-10s %-10s %-10s %-10s\n" "case" "java" "src2" "expect" "match"
echo "------------------------------------------------------------"
for tc_name in $(cut -d'|' -f1 "$OUTDIR/results.psv" | sort -u | grep -v '^case$'); do
    jstat=$(grep "^${tc_name}|java|" "$OUTDIR/results.psv" | cut -d'|' -f3 | head -1)
    jval=$(grep "^${tc_name}|java|" "$OUTDIR/results.psv" | cut -d'|' -f13 | head -1)
    sstat=$(grep "^${tc_name}|src2|" "$OUTDIR/results.psv" | cut -d'|' -f3 | head -1)
    sval=$(grep "^${tc_name}|src2|" "$OUTDIR/results.psv" | cut -d'|' -f13 | head -1)
    expect=$(grep "^${tc_name}|src2|" "$OUTDIR/results.psv" | cut -d'|' -f15 | head -1)
    match=$(grep "^${tc_name}|src2|" "$OUTDIR/results.psv" | cut -d'|' -f16 | head -1)

    js="${jstat}"
    if [ "$jstat" = "PASS" ]; then js="PASS($jval)"; fi
    ss="${sstat}"
    if [ "$sstat" = "PASS" ]; then ss="PASS($sval)"; fi
    printf "%-30s %-10s %-10s %-10s %-10s\n" "$tc_name" "$js" "$ss" "${expect:-}" "${match:-}"
done
echo "------------------------------------------------------------"
