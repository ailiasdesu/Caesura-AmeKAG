# Handoff Report — Pillar R2: Engine Performance Benchmarking & Baseline Profiling

- **Author**: Worker R2 (Implementer / QA / Specialist)
- **Target**: Orchestrator / Parent Agent (`e719b389-c81d-4035-b5ae-7b9d40b96a30`)
- **Working Directory**: `d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r2`
- **Timestamp**: 2026-08-25T01:01:00Z
- **Subject**: Completion of Pillar R2 Performance Telemetry & Comprehensive Baseline Documentation for Caesura (AmeKAG)

---

## 1. Observation

Direct execution of benchmark suites and inspection of codebase implementations:

### 1.1 Benchmark Test Suite Execution Results
- **`bash scripts/run_benchmarks.sh`**:
  - `test_frame_bench.lua` (per-frame render/expr/add guards): **PASS (1.51s)**
  - `test_scale_stress.lua` (large-asset scale stress): **PASS (2.17s)**
  - `test_benchmark.lua` (tokenizer/scheduler throughput): **PASS (469ms)**
  - `test_bench_dispatch.lua` (scheduler hot-loop dispatch): **PASS (381ms)**
  - `test_label_bench.lua` (label index vs linear scan): **PASS (713ms)**
  - **Summary**: `Total: 5/5 PASS, 0 FAIL (5.81s)`.
- **`build/tests/Debug/CaesuraTests.exe -tc="Perf:*" -s`**:
  - `Perf: Lua string/table throughput`: 10k `string.format` + table append = **30.58 ms** (CHECK: `< 800.0 ms`, SUCCESS).
  - `Perf: Lua table field access`: 10k table field reads = **0.90 ms** (CHECK: `< 400.0 ms`, SUCCESS).
  - `Perf: SmaSkinner 8k-vertex soft skin`: 8k vertex CPU soft skinning = **1.08 ms/frame** (GPU compute reference: ~0.08 ms, CHECK: `< 10.0 ms`, SUCCESS).
  - **Summary**: `3 test cases passed, 0 failed, 1049 skipped`.
- **`python scripts/count_coupling.py`**:
  - All 16 modules (`archive: 2/4`, `audio: 2/4`, `debug: 0/4`, `di: 13/14`, `entry: 14/14`, `input: 0/4`, `job: 1/4`, `live2d: 3/4`, `minigame: 4/4`, `platform: 0/4`, `render: 4/4`, `resource: 3/4`, `rpc: 2/4`, `script: 11/14`, `steam: 0/4`, `storage: 4/4`) strictly adhere to architectural coupling limits with **0 violations**.

### 1.2 Subsystem Verification & Implementation Telemetry
1. **Subsystem 1 (Font Glyph Atlas & Layout)**:
   - `src/render/TextRenderer.h` & `TextRenderer.cpp`: FreeType 2 2048² dynamic atlas (`loadTTF()`), 4096² CJK pre-baked atlas (`loadCjkAtlas()`), `MessageLayerCache` dynamic vertex/index buffers with `computeDirtyRange()` and `layoutGlyphs()`.
   - `test_scale_stress.lua` Section A: 4096 tiles (16,777,216 texels) accounted in **1.000 ms** (< 1.0s ceiling, > 99.7% headroom).
2. **Subsystem 2 (Audio 3-Bus Mixer & Concurrency)**:
   - `src/audio/SoLoudAudioEngine.h` & `SoLoudAudioEngine.cpp`: 3-bus SoLoud topology (`m_bgmBus`, `m_voiceBus`, `m_seBus`), voice pool ducking with 0.3s fade recovery, 32-entry LRU wave cache (`m_waveCache`), frame-based handle culling (`cullFinishedHandles()`).
   - `test_scale_stress.lua` Section B: 80,000 handle alloc/free operations on a 128-max concurrent pool completed in **63.000 ms** (< 2.0s ceiling, 79,872 reuses, live handles = 120, next ID bounded to 129).
3. **Subsystem 3 (Large Script Tokenization & Execution Throughput)**:
   - `scripts/tokenizer.lua`, `scripts/kag/compiler.lua`, `scripts/scheduler.lua`: LPeg non-backtracking tokenizer, AOT compiler emitting `_compiled` structures (`exprs`, `params`, `handlers`, `flow`, `labels`), coroutine scheduler hot loop with `build_label_index()` O(1) hash lookup.
   - `test_scale_stress.lua` Section C: 4800 lines of `[ch][p]` (9600 tokens) parsed in **946.0 ms** (< 10.0s ceiling) and executed across 9601 frames in **42.0 ms** (229 tok/ms, < 10.0s ceiling).
   - `test_benchmark.lua`: 2000-line script parsed in **269.0 ms** (67.25 ms/1000tok), 4001 resumes in **24.0 ms** (total 293.0 ms < 3.0s ceiling).
   - `test_bench_dispatch.lua`: 2000 compiled `[ch]` commands dispatched in **6.0 ms** (333,333 tokens/sec).
   - `test_label_bench.lua`: 1500-label index build and 3000 lookups confirmed O(1) hash lookup is >300x faster than linear scan.
   - `test_scale_stress.lua` Section E: 3000-line narrative flow (408.9 KB) translated in **1226.0 ms** (408.7 µs/line < 3000 µs/line ceiling).
4. **Subsystem 4 (Backlog Memory Overhead & Serialization)**:
   - `scripts/kag/commands/text.lua` & `scripts/kag/commands/save.lua`: `TextCommands.push_backlog()` stores structured records with `ctx.backlog_max or 500` ring-buffer cap; `save.lua` serializes the most recent 100 entries (< 50 KB JSON payload) and enforces 500-entry cap on restore.
   - `test_scale_stress.lua` Section D: 500-page backlog accumulation (2500 dialogue entries) results in **934.7 KB** heap growth (< 4096.0 KB ceiling, > 77.2% headroom).
5. **Subsystem 5 (Frame Rendering Time & Dispatch Budget)**:
   - `scripts/layers.lua`, `src/render/BgfxQuadBatch.cpp`, `src/render/SmaSkinner.h`: 7 layer roles DFS traversal with `pool.lua` zero-GC allocation; 2048-quad dynamic batching; dual-mode skinning (CPU soft skinning vs GPU compute skinning).
   - `test_frame_bench.lua`: 5000-frame traversal across 5 active layers averaged **274.6 ~ 350.0 µs/frame** (< 500 µs/frame ceiling, occupying ~1.6%–2.1% of a 16.67ms 60 FPS frame).
   - `test_perf_bench.cpp`: 8k-vertex dual-bone soft skinning takes **0.75 ~ 1.08 ms/frame** (< 10.0 ms ceiling) on CPU vs ~0.08 ms on GPU compute.

---

## 2. Logic Chain

1. **Verification of Test Suites**: Running `bash scripts/run_benchmarks.sh`, `CaesuraTests.exe -tc="Perf:*" -s`, and `python scripts/count_coupling.py` directly confirmed that all hot paths have established guards that execute cleanly with zero failures.
2. **Subsystem Architecture Mapping**: Deep inspection of `TextRenderer`, `SoLoudAudioEngine`, `tokenizer/compiler/scheduler`, `text/save`, and `layers/SmaSkinner` confirmed that the 5 core subsystem implementations align precisely with the requirements and scale-stress tests.
3. **Comprehensive Documentation Delivery**: `docs/design/engine-performance-baseline.md` was rewritten from an outdated 33-line microbenchmark record into a complete 229-line authoritative performance specification. It contains in-depth architectural descriptions, measurement methodologies, target ceilings, empirical numbers, safety margins, and exact reproducer runbooks for all 5 subsystems and Web Wasmoon.
4. **Architectural Compliance**: No modifications were made outside the exclusive write boundary (`docs/design/engine-performance-baseline.md`). Cross-module coupling was re-verified, confirming 0 violations across all 16 modules.

---

## 3. Caveats

1. **Platform Clocks & Jitter**: Microbenchmarks rely on `os.clock()` and `std::chrono::steady_clock`. Timings can fluctuate depending on CPU power management, turbo states, and background OS load. The documented ceilings deliberately maintain >40%–99% headroom to ensure reliability across varied CI virtual machines.
2. **Headless Execution**: Lua benchmark suites simulate audio and GPU backend submission via local mocks to allow deterministic CI execution without physical display or audio devices. Real GPU dispatch and rendering are verified via C++ integration tests.

---

## 4. Conclusion

- Pillar R2 performance benchmarking milestone is **100% complete and fully verified**.
- `docs/design/engine-performance-baseline.md` has been successfully updated with authoritative, comprehensive performance telemetry across all 5 core subsystems.
- All benchmark test suites pass with **0 failures**, and architectural coupling limits remain at **0 violations**.

---

## 5. Verification Method

To independently reproduce and verify all metrics, run:

1. **Lua Benchmark Suites**:
   ```bash
   bash scripts/run_benchmarks.sh
   ```
   *Expected Output*: `Total: 5/5 PASS, 0 FAIL (5.8s)`
2. **C++ Doctest Hot-Path Benchmarks**:
   ```powershell
   build\tests\Debug\CaesuraTests.exe -tc="Perf:*" -s
   ```
   *Expected Output*: `3 passed, 0 failed, 1049 skipped`
3. **Architecture Coupling Limits**:
   ```powershell
   python scripts/count_coupling.py
   ```
   *Expected Output*: All 16 modules pass with 0 violations.
