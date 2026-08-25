# Execution Plan: Caesura Post-RC Production Sprint

## Overview
This plan implements the comprehensive production sprint across four core pillars:
1. R1: Multi-Platform Release Packaging & Distribution Bundling
2. R2: Engine Performance Benchmarking & Baseline Profiling
3. R3: Web Player PWA & Mobile Web Offline Experience
4. R4: Creator Tools & Sample Game Polish

## Milestones & Work Breakdown

### M0: Technical Survey & Environment Baseline
- Survey current project directory, packaging scripts (`scripts/package_game.sh`, `scripts/build_android_release.sh`, CMake CPack configuration), web templates (`src/platform/web/` or `web/`), benchmark test harnesses (`tests/scripts/test_frame_bench.lua`), and sample projects (`demo/example_game/`, `tools/project_templates/`).
- Verify testing toolchains and current build test suite baseline.

### M1: Multi-Platform Release Packaging & Distribution Bundling (R1)
- Verify/enhance CPack config for Windows binary zip (`CaesuraAmeKAG-1.0.0-rc.1-win64.zip`) including executable, assets, runtime DLLs, licenses.
- Package Web standalone distribution bundle bundling engine wasm/js/html + demo game assets.
- Package Android Release APK & AAB generation via `scripts/build_android_release.sh` with keystore signing support.
- Generate unified SHA-256 checksums and `release-manifest.json`/markdown manifest in `artifacts/dist/`.

### M2: Engine Performance Benchmarking & Baseline Profiling (R2)
- Implement & execute micro and macro benchmark suites:
  1. Font glyph rasterization & atlas lookup throughput.
  2. Audio 3-bus mixer concurrency & handle alloc throughput.
  3. Large script tokenization & execution throughput (9600+ tokens).
  4. Backlog memory overhead & incremental serialization scaling (500+ records).
  5. Frame render time & CPU dispatch budget (`tests/scripts/test_frame_bench.lua`).
- Document all baseline telemetry, methodology, and targets in `docs/design/engine-performance-baseline.md`.

### M3: Web Player PWA & Mobile Web Offline Experience (R3)
- Integrate Service Worker (`sw.js`) with cache-first / stale-while-revalidate strategy for assets, WASM binary, and script data.
- Create Web App Manifest (`manifest.webmanifest`) with standalone display mode, orientation lock hints, icons, theme color.
- Implement mobile UX improvements: AudioContext unlock overlay on first touch/interaction, orientation lock helper, viewport fit/safe-area styling.

### M4: Creator Tools & Sample Game Polish (R4)
- Polish demo visual novel (`demo/example_game/`) and template projects (`tools/project_templates/`):
  - Showcase declarative tweening (`[tween]`), VFX bloom (`[vfx bloom]`), vignette (`[vfx vignette]`).
  - Introduce reusable UI style presets.
  - Ensure zero syntax/contract validation errors across all KAG scripts.

### M5: Full-Suite Verification & Release Sign-Off
- Run full C++ tests (1052+ test assertions) & Lua tests (158+ suites).
- Complete code review, challenger validation, and forensic audit.
- Generate comprehensive final report.
