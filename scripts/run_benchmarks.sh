#!/usr/bin/env bash
# =============================================================================
# scripts/run_benchmarks.sh -- Caesura (AmeKAG) performance benchmark entry
#
# Purpose (task book §16 "Performance Benchmark", docs/plans/audit/):
#   One-shot runner for the long-lived performance suites. Release-gate
#   reference: any PR that touches an engine hot path (tokenizer / kag.compiler
#   / scheduler / kag.expr / layers render / web bridge.js player loop) MUST
#   attach a benchmark run (this script's summary, or tmp/bench-latest.txt)
#   OR a written performance justification. Number baselines live in
#   docs/plans/2026-08-04-006-perf-baseline-update.md (rounds 66-114).
#   Usage guide: docs/guides/performance-benchmarks.md.
#
# Suites and the dimension each one guards (all headless pure Lua unless noted):
#   test_frame_bench.lua      Per-frame cost guard: layers.render() 5000x mean
#                             <500us/frame; mixed expr translate 1000x <2s;
#                             schema-migrated [add] chain dispatch 1000x <2s.
#   test_scale_stress.lua     Large-asset scale stress (round 101, 20 asserts):
#                             4096x4096 atlas texel accounting <1s; 80k audio
#                             handle alloc/free (cap 128) <2s; 9600-token scene
#                             parse/run <10s each; 500-page backlog heap growth
#                             <4096KB; 3000-line (~400KB) translate <10s.
#   test_benchmark.lua        Throughput: 2000-line .ks tokenizer.parse <3s +
#                             mock scheduler dispatch progress; total <3s.
#   test_bench_dispatch.lua   Scheduler hot loop: 2000 compiled [ch] dispatch
#                             count exact + parse/compile/run pipeline <10s;
#                             branch/jump flow order; prints tokens/sec.
#   test_label_bench.lua      Label index: build over a 1500-label scene;
#                             indexed lookup <= linear scan at 3000 lookups
#                             (real ratio ~300x); finds last label.
#
# With --web (vitest + jsdom + wasmoon; local-only, not part of CI):
#   web/perf-baseline.test.js Web player baseline (round 109): story.ks frame
#                             throughput >1.3 frames/ms, token >0.15 tok/ms,
#                             Lua heap growth <1024KB; synthetic 1000-line
#                             >2.5 frames/ms, heap <2048KB; 2000-vs-1000-line
#                             wall clock <2.5x (scale linearity).
#   web/perf-bundle.test.js   ks_bake bundle path vs raw .ks source path:
#                             bundle >= 0.8x token throughput (<=20% slower)
#                             on tiny / real story.ks / 1000+ cmd scenes.
#
# Usage:
#   bash scripts/run_benchmarks.sh          # 5 Lua suites (default gate)
#   bash scripts/run_benchmarks.sh --web    # + web vitest perf suites
#
# Output: full log in tmp/bench-latest.txt; PASS/FAIL summary table on stdout.
# Exit code: 0 = all green, 1 = any suite failed.
# =============================================================================
set -u

cd "$(dirname "$0")/.." || exit 1   # repo root

LUA="external/lua/lua.exe"
LOG="tmp/bench-latest.txt"
RUN_WEB=0
[ "${1:-}" = "--web" ] && RUN_WEB=1

if [ ! -f "$LUA" ]; then
  echo "ERROR: $LUA not found (run from repo root)" >&2
  exit 1
fi

mkdir -p tmp
: > "$LOG"

# Progress goes to stderr so stdout stays a clean summary table.
log() { printf '%s\n' "$*" >&2; printf '%s\n' "$*" >> "$LOG"; }
now_ms() { date +%s%N | cut -c1-13; }

NAMES=(); DIMS=(); RESULTS=(); TIMES=()

run_suite() {
  local name="$1" dim="$2"; shift 2
  log ">>> RUN   $name -- $dim"
  local t0 t1 ms rc verdict
  t0=$(now_ms)
  "$@" > "$LOG.tmp" 2>&1
  rc=$?
  t1=$(now_ms)
  ms=$((t1 - t0))
  cat "$LOG.tmp" >> "$LOG"
  if [ "$rc" -eq 0 ] && ! grep -q "FAIL" "$LOG.tmp"; then
    verdict="PASS"
  else
    verdict="FAIL"
    log "!!! FAIL  $name (exit $rc) -- see $LOG"
  fi
  NAMES+=("$name"); DIMS+=("$dim"); RESULTS+=("$verdict"); TIMES+=("$ms")
  rm -f "$LOG.tmp"
}

T0=$(now_ms)

run_suite "test_frame_bench.lua" "per-frame render/expr/add guards" \
  "$LUA" "tests/scripts/test_frame_bench.lua"
run_suite "test_scale_stress.lua" "large-asset scale stress" \
  "$LUA" "tests/scripts/test_scale_stress.lua"
run_suite "test_benchmark.lua" "tokenizer/scheduler throughput" \
  "$LUA" "tests/scripts/test_benchmark.lua"
run_suite "test_bench_dispatch.lua" "scheduler hot-loop dispatch" \
  "$LUA" "tests/scripts/test_bench_dispatch.lua"
run_suite "test_label_bench.lua" "label index vs linear scan" \
  "$LUA" "tests/scripts/test_label_bench.lua"

if [ "$RUN_WEB" -eq 1 ]; then
  run_suite "perf-baseline.test.js" "web player frames/token/heap" \
    bash -c "cd web && npx vitest run perf-baseline.test.js"
  run_suite "perf-bundle.test.js" "bundle vs .ks source throughput" \
    bash -c "cd web && npx vitest run perf-bundle.test.js"
fi

TOTAL_MS=$(($(now_ms) - T0))

fmt_ms() {
  local ms=$1
  if [ "$ms" -ge 1000 ]; then
    printf '%d.%02ds' $((ms / 1000)) $(((ms % 1000) / 10))
  else
    printf '%dms' "$ms"
  fi
}

# ---- summary table on stdout ------------------------------------------------
printf '\n'
printf '================ Caesura Benchmark Summary ================\n'
printf '%-26s %-34s %-6s %8s\n' "Suite" "Dimension" "Result" "Time"
printf '%.0s-' {1..78}; printf '\n'
i=0; pass_n=0; fail_n=0
while [ $i -lt ${#NAMES[@]} ]; do
  printf '%-26s %-34s %-6s %8s\n'     "${NAMES[$i]}" "${DIMS[$i]}" "${RESULTS[$i]}" "$(fmt_ms ${TIMES[$i]})"
  if [ "${RESULTS[$i]}" = "PASS" ]; then pass_n=$((pass_n + 1)); else fail_n=$((fail_n + 1)); fi
  i=$((i + 1))
done
printf '%.0s-' {1..78}; printf '\n'
printf 'Total: %d/%d PASS, %d FAIL  (%s)\n' "$pass_n" $((pass_n + fail_n)) "$fail_n" "$(fmt_ms $TOTAL_MS)"
printf 'Log:   %s\n' "$LOG"

[ "$fail_n" -eq 0 ]
