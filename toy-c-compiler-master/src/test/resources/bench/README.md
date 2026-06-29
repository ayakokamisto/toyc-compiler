# ToyC Benchmark Cases

This directory contains performance-oriented ToyC programs. They mirror the
remote performance-test themes reported by the online judge: constants, dead
code, copy propagation, common subexpressions, algebraic simplification, tail
recursion, loop-heavy code, combined basics, graph-like global updates,
matrix-like arithmetic, global constant propagation, and constant-expression
chains.

Suggested workflow:

```bash
mvn package
scripts/bench.sh --name baseline --runs 5
scripts/bench.sh --name opt-check --runs 5 --mode opt
```

The programs intentionally keep return values in the `0..255` process exit-code
range while doing enough work to expose optimization effects. The exact judge
inputs are hidden, so these cases are local proxies for trend tracking rather
than copies of the remote tests.

The runner always builds and times a `gcc -O2` RV32 baseline for each benchmark
case. Summary CSV files include `gcc_o2_median_ms`, `gcc_o2_mean_ms`,
`vs_gcc_o2_median_ratio`, and `vs_gcc_o2_mean_ratio`; ratio values are ToyC time
divided by the matching gcc `-O2` time, so larger values mean slower than gcc.

The runner writes generated assembly and ELF files under
`/tmp/toyc-compiler-bench/<name>/`, and writes result CSV files under
`benchmark-results/`.
