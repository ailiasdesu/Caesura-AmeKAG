# Handoff Report — Pillar R2: Engine Performance Benchmarking & Baseline Profiling

- **Author**: Explorer 2 (Survey Agent)
- **Target**: Orchestrator / Implementer for Pillar R2
- **Working Directory**: `d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_2`
- **Timestamp**: 2026-08-25T00:55:00Z
- **Subject**: Investigation and Survey of Pillar R2 (Performance Benchmarking, 5 Subsystem Targets, and Telemetry Documentation)

---

## 1. Observation

Direct code and file observations across the Caesura (AmeKAG) codebase:

### 1.1 Benchmark Test Suites & Infrastructure
- **`scripts/run_benchmarks.sh`** (lines 1–139):
  - Primary execution harness for performance testing. Runs 5 headless pure-Lua suites: `test_frame_bench.lua`, `test_scale_stress.lua`, `test_benchmark.lua`, `test_bench_dispatch.lua`, `test_label_bench.lua`.
  - Supports `--web` flag to execute Web player vitest suites (`perf-baseline.test.js`, `perf-bundle.test.js`).
  - Outputs a structured summary table and logs full traces to `tmp/bench-latest.txt`.
  - Verification run via `bash scripts/run_benchmarks.sh` exited with code 0: **5/5 PASS, 0 FAIL** in 7.42s.
- **`tests/cpp/test_perf_bench.cpp`** (lines 1–142):
  - C++ doctest CPU hot-path benchmarks:
    - `Perf: Lua string/table throughput`: 10k `string.format` + table append = **35.27 ms** (Ceiling < 800.0 ms).
    - `Perf: Lua table field access`: 10k table field lookups = **0.89 ms** (Ceiling < 400.0 ms).
    - `Perf: SmaSkinner 8k-vertex soft skin`: 8k vertex CPU soft skinning = **0.92 ms/frame** (Ceiling < 10.0 ms; GPU compute reference is ~0.08 ms).
  - Executed via `build/tests/Debug/CaesuraTests.exe -tc="Perf:*" -s`: **3 passed, 0 failed, 1049 skipped**.
- **`tests/cpp/test_render_integration.cpp`** (lines 650–683):
  - Benchmarks 8k-vertex dual-bone SMA mesh skinning host-side dispatch: CPU host time ~1.27 ms vs GPU host compute dispatch ~0.08 ms (~15.8x speedup).
- **`tests/cpp/test_mini_collision.cpp`** (lines 36–50):
  - Sweep-and-prune 200-box sparse set collision detection perf guard (asserts 0 overlaps in O(N log N) without O(N²) degradation).

### 1.2 Subsystem Area 1: Font Glyph Atlas Rasterization & Cache Lookup Speed
- **Implementation Paths**: `src/render/TextRenderer.h` (lines 8–249), `src/render/TextRenderer.cpp` (lines 840–1250).
- **Architecture & Hot-Path Behavior**:
  - **Dynamic TTF Rasterization (`loadTTF()`)**: FreeType 2 face initialized at requested font size (e.g. 24px). Iterates Unicode codepoint ranges: ASCII (32–126), General Punctuation (0x2000–0x206F), CJK Symbols (0x3000–0x303F), Hiragana (0x3040–0x309F), Katakana (0x30A0–0x30FF), Fullwidth Forms (0xFF00–0xFFEF), and CJK Unified Ideographs (0x4E00–0x9FFF). Rasterizes grayscale glyph bitmaps into a 2048×2048 RGBA8 texture atlas (16 MB) using row-packing (`m_ttf->penX`, `m_ttf->penY`, `m_ttf->maxRowH`).
  - **Pre-baked CJK Static Atlas (`loadCjkAtlas()`)**: Loads 4096×4096 RGBA8 pre-rendered glyph texture (64 MB) and binary metadata table mapping codepoints to `CjkGlyph` structs (`{x, y, w, h, advance, offsetX, offsetY}`).
  - **Hierarchical Lookup (`getTTFGlyph()`)**: (1) `m_ttf->glyphs` hash map → (2) `m_cjkGlyphs` hash map → (3) Built-in ASCII 32–126 bitmap font → (4) Unicode replacement character `0xFFFD`.
  - **Batch Cache & Dirty Ranges (`MessageLayerCache`, `layoutGlyphs()`, `buildQuadVertices()`, `computeDirtyRange()`)**: Supports up to 2048 glyphs per message layer in a dynamic vertex/index buffer. Identifies prefix text matches during dialogue streaming and only updates dirty vertex ranges, eliminating full-text re-rasterization and layout calculation on each typed character.
- **Empirical Scale Guard**: `test_scale_stress.lua` Section A: 4096×4096 atlas split into 4096 tiles (16,777,216 texels) full accounting prepass completed in **3.000 ms** (< 1.0s budget, > 99.7% headroom).

### 1.3 Subsystem Area 2: Audio Handle Allocation & 3-Bus Mixer Concurrency
- **Implementation Paths**: `src/audio/api/IAudioBackend.h` (lines 1–92), `src/audio/SoLoudAudioEngine.h` (lines 23–129), `src/audio/SoLoudAudioEngine.cpp` (lines 89–360).
- **Architecture & Hot-Path Behavior**:
  - **3-Bus Architecture**: Independent `m_bgmBus`, `m_voiceBus`, and `m_seBus` instances of `SoLoud::Bus` played into `m_soloud`. Allows decoupled volume control, fading, and mute state.
  - **Voice Management & Ducking**: Voice pool (`kVoicePoolSize = 4`) with automatic ducking: when voice playback begins, BGM bus volume is lowered; upon voice termination, BGM fades back to configured volume over 0.3s. Natural completions are tracked in `m_voiceCompletionsPending`.
  - **Waveform LRU Cache**: `m_waveCache` holds up to 32 loaded `SoLoud::AudioSource` instances. Uses `std::list<std::string>` (`m_waveLRU`) + hash map iterator index (`m_waveLRUMap`) for O(1) eviction. Never evicts actively playing sources (`m_soloud.countAudioSource() > 0`).
  - **Handle Lifecycle & Recycling**: Active SE handles tracked in `m_activeSE`; fading BGM/Voice handles tracked in `m_retiringBGM`/`m_retiringVoice`. Frame update `cullFinishedHandles()` sweeps dead handles and evicts transient raw-PCM wave allocations in `m_rawWaveCache`.
- **Empirical Scale Guard**: `test_scale_stress.lua` Section B: 80,000 handle alloc/free operations across a 128-max concurrent pool with ID recycling completed in **123.000 ms** (< 2.0s budget, 79,872 handle reuses, live handles = 120, next monotonic ID bounded to 129).

### 1.4 Subsystem Area 3: Large Script Tokenization & Execution Throughput
- **Implementation Paths**: `scripts/tokenizer.lua` (lines 1–284), `scripts/kag/compiler.lua` (lines 1–971), `scripts/scheduler.lua` (lines 1–650), `src/script/LuaBackend.cpp`.
- **Architecture & Hot-Path Behavior**:
  - **LPeg Tokenizer**: `tokenizer.parse()` uses compact LPeg patterns. Handles 167 KAG3 tags + Neo-Genesis extensions without exponential backtracking.
  - **AOT Compiler**: `compiler.compile()` takes raw token streams and emits `_compiled` side tables containing:
    - Pre-translated Lua expressions (`_compiled.exprs`).
    - Normalized parameter tables (`_compiled.params`).
    - Direct command function references (`_compiled.handlers`).
    - Flow jump targets and label index (`_compiled.flow`, `_compiled.labels`).
  - **Scheduler Execution Engine**: `scheduler.run()` executes compiled token arrays in a fast loop using coroutine yields for frame boundaries (`[p]`, `[wait]`).
- **Empirical Scale Guards**:
  - `test_scale_stress.lua` Section C: 4800 lines of `[ch][p]` (~388 KB source, 9600 tokens) parsed in **1474.000 ms** (< 10.0s budget) and executed across 9601 frames in **62.000 ms** (155 tokens/ms, < 10.0s budget, > 99% headroom).
  - `test_benchmark.lua`: 2000-line .ks script (4000 tokens) parsed in **249.0 ms** (62.25 ms/1000tok) and 4001 scheduler resumes in **22.0 ms** (total **271.0 ms** < 3.0s budget).
  - `test_bench_dispatch.lua`: 2000 compiled `[ch]` commands dispatched in **6.0 ms** (333,333 tokens/sec). Total pipeline (parse 225ms + compile 4ms + run 6ms) = **235.0 ms** (< 10.0s budget).
  - `test_label_bench.lua`: 1500-label index build and 3000 lookups verified O(1) indexed lookup is faster than linear scan.

### 1.5 Subsystem Area 4: Backlog Memory Overhead & Incremental Serialization
- **Implementation Paths**: `scripts/kag/commands/text.lua` (lines 333–352), `scripts/kag/commands/save.lua` (lines 78–90, 355–365), `scripts/history_ui.lua`.
- **Architecture & Hot-Path Behavior**:
  - **Bounded Backlog**: `TextCommands.push_backlog()` stores structured records (`{speaker, text, voiceFile, src}`) and enforces `ctx.backlog_max or 500` ring-buffer limit via `table.remove(ctx.backlog, 1)`.
  - **Save Serialization**: `save.lua` serializes up to the last 100 entries (`math.max(1, #ctx.backlog - 99)`) to keep save file payloads small (< 50 KB JSON), while runtime restore caps at 500 entries.
- **Empirical Scale Guard**: `test_scale_stress.lua` Section D: Accumulation of 500 backlog pages (2500 dialogue entries) results in **934.7 KB** heap growth (< 4096.0 KB budget, 77.2% headroom).

### 1.6 Subsystem Area 5: Frame Rendering Time & CPU Dispatch Budget
- **Implementation Paths**: `scripts/layers.lua` (lines 1–885), `src/render/BgfxRenderDevice.cpp`, `src/render/BgfxQuadBatch.cpp`, `src/render/SmaSkinner.h`, `tests/scripts/test_frame_bench.lua`.
- **Architecture & Hot-Path Behavior**:
  - **Lua Scene Graph Traversal**: 7 layer roles (base, layer0, layer1, fore, ui, message, effect). Per-frame DFS traversal, Z-sorting, RTT viewport mapping, dirty rect clipping, batch pooling (`pool.lua`), and unified batch submission to C++ `backend.submit_batch()`.
  - **C++ Quad Batching**: Batches quad draws into persistent 2048-quad dynamic vertex/index buffers, reducing draw calls to 1 draw per texture/state switch.
- **Empirical Scale Guards**:
  - `test_frame_bench.lua`: `layers.render()` 5000-frame traversal across 5 active layers averaged **~274–277 µs/frame** (< 500 µs/frame budget, ~45% budget utilization).
  - Mixed expression translation 1000x: **~54–57 ms** (< 2.0s budget).
  - `[add]` chain dispatch 1000x: **~4–9 ms** (< 2.0s budget).

### 1.7 Documentation State vs Reality
- **`docs/design/engine-performance-baseline.md`** (lines 1–33):
  - Currently contains only round 25 C++ microbenchmark data (33 lines total).
  - Missing all round 66–114 benchmarks: large-asset scale stress (atlas, audio handles, 9600-token scene, backlog heap, narrative translation), frame dispatch guards, and Web wasmoon benchmarks.
- **`docs/plans/2026-08-04-006-perf-baseline-update.md`** (lines 1–786) & **`docs/guides/performance-benchmarks.md`** (lines 1–86):
  - Contains the authoritative historical record and guide for all 5 Lua test suites and Web wasmoon baselines.

---

## 2. Logic Chain

1. **Observation 1.1 & 1.7**: The engine has established comprehensive benchmark suites (`test_frame_bench.lua`, `test_scale_stress.lua`, `test_benchmark.lua`, `test_bench_dispatch.lua`, `test_label_bench.lua`, `test_perf_bench.cpp`), but the primary design documentation file `docs/design/engine-performance-baseline.md` is outdated (round 25 snapshot).
2. **Observation 1.2**: Font glyph rasterization uses FreeType 2 for dynamic 2048² RGBA8 atlas generation and supports 4096² pre-baked CJK static atlases. Atlas tile accounting is proven to process 16.77M texels in 3.0ms (< 1s budget), and `MessageLayerCache` avoids per-frame glyph re-layout via dirty ranges.
3. **Observation 1.3**: The audio subsystem uses a 3-bus SoLoud mixer (BGM, VOICE, SE) with automatic voice ducking, 32-entry LRU wave caching, and frame-based handle culling. Concurrency stress verifies 80,000 alloc/free cycles on a 128-handle pool complete in 123ms (< 2s budget) with 0 handle leaks or ID runaway.
4. **Observation 1.4**: Script throughput is split into LPeg tokenization, AOT compilation, and coroutine scheduler dispatch. A 9600-token scene (4800 lines) parses in 1.47s (< 10s budget) and executes across 9601 frames in 62ms (~155 tok/ms, < 10s budget), while compiled hot-loop dispatch reaches >330,000 tokens/sec.
5. **Observation 1.5**: Backlog storage enforces a strict 500-page limit (desktop standard). Heap growth is measured at 934.7 KB (< 4096 KB ceiling), proving linear memory scaling without unbounded GC pressure. Save serialization caps to 100 recent entries to minimize save state footprint.
6. **Observation 1.6**: Frame rendering cost in Lua is bounded at ~277 µs/frame (< 500 µs/frame budget), and CPU soft skinning requires 0.92 ms/frame vs GPU compute 0.08 ms. Both provide ample headroom for 60 FPS (16.6 ms) and 120 FPS (8.3 ms) targets on desktop and mobile.
7. **Synthesis & Conclusion**: The 5 target benchmark areas already have robust test implementations and empirical baselines passing 100% with >45% to >99% headroom. The core required deliverable for Pillar R2 is:
   - Synchronize and expand `docs/design/engine-performance-baseline.md` to reflect the complete 5-subsystem benchmark telemetry, measurement methods, and target thresholds.
   - Maintain `scripts/run_benchmarks.sh` and CI integration to ensure zero performance regression.

---

## 3. Caveats

1. **Hardware & Power Management Variance**: Wall-clock timings (`os.clock()`, `std::chrono::steady_clock`) vary based on CPU power-saving states and background OS load. Baseline targets intentionally incorporate >2x safety margins (loose CI-safe ceilings) so slower CI VMs do not produce false positives.
2. **GPU vs Headless Context**: Headless Lua benchmark suites (`test_scale_stress.lua`, `test_frame_bench.lua`) mock backend GPU submission and audio driver hardware outputs to run deterministically in CI environments without display servers or audio sinks. Real GPU rendering is validated separately via C++ doctests (`test_render_integration.cpp`).
3. **Web Wasmoon Single-Threaded Constraints**: Web wasmoon player runs synchronously in the browser main thread without Web Workers. Web benchmarks (`perf-baseline.test.js`) measure scheduler tick throughput (frames/ms) and Lua heap memory rather than vsync display refresh rates.

---

## 4. Conclusion

1. **Subsystem Readiness**: All 5 target benchmark areas (Font atlas, Audio 3-bus mixer, Script tokenization/dispatch, Backlog memory scaling, Frame rendering budget) have dedicated test harnesses and pass all assertions with zero failures.
2. **Current Measured Metrics Summary**:
   | Subsystem / Benchmark Area | Key Test Harness | Budget / Ceiling | Measured Empirical Value | Headroom / Safety Margin |
   |---|---|---|---|---|
   | **1. Font Atlas & Cache** | `test_scale_stress.lua` (A) / `TextRenderer.cpp` | < 1.0 s (4096 tiles) | **3.0 ms** | > 99.7% |
   | **2. Audio 3-Bus Concurrency** | `test_scale_stress.lua` (B) / `SoLoudAudioEngine.cpp` | < 2.0 s (80k churn) | **123.0 ms** (120 live, ID=129) | > 93.8% |
   | **3. Script Tokenization** | `test_scale_stress.lua` (C) / `test_benchmark.lua` | < 10.0 s (9600 tok parse) | **1.47 s** (62 ms/1000tok) | > 85.3% |
   | **3. Script Execution** | `test_scale_stress.lua` (C) / `test_bench_dispatch.lua` | < 10.0 s (9600 tok run) | **62.0 ms** (333k tok/s) | > 99.3% |
   | **4. Backlog Memory Scaling** | `test_scale_stress.lua` (D) / `text.lua` | < 4096 KB (500 pages) | **934.7 KB** | > 77.2% |
   | **5. Frame Render Dispatch** | `test_frame_bench.lua` / `layers.lua` | < 500 µs / frame | **~274–277 µs / frame** | ~45.0% |
   | **5. CPU Soft Skinning** | `test_perf_bench.cpp` / `SmaSkinner.h` | < 10.0 ms (8k verts) | **0.92 ms / frame** (GPU ~0.08ms)| > 90.0% |
3. **Required Action Items for Pillar R2**:
   - Update `docs/design/engine-performance-baseline.md` with the full unified telemetry specification, 5 subsystem baseline tables, and reproducer commands.
   - Verify `scripts/run_benchmarks.sh` passes cleanly on all desktop platforms.
   - Verify 16/16 module coupling limits in `AGENTS.md` remain 0 violations.

---

## 5. Verification Method

To independently verify these findings, run the following commands:

1. **Execute All 5 Lua Benchmark Suites**:
   ```bash
   bash scripts/run_benchmarks.sh
   ```
   *Expected*: `Total: 5/5 PASS, 0 FAIL` in ~7s. Full trace logged to `tmp/bench-latest.txt`.

2. **Execute C++ CPU Hot-Path Benchmarks**:
   ```powershell
   build\tests\Debug\CaesuraTests.exe -tc="Perf:*" -s
   ```
   *Expected*: `3 passed, 0 failed, 1049 skipped`. Outputs Lua format+append (35ms), table reads (0.89ms), and CPU skinning (0.92ms).

3. **Verify All 16 Architecture Coupling Limits**:
   ```powershell
   python scripts/count_coupling.py
   ```
   *Expected*: All 16 module cross-include limits pass with 0 violations.
