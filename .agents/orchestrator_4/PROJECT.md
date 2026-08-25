# Project: Caesura (AmeKAG) Post-RC Production Sprint

## Architecture
- **Core Engine (C++17)**: 16 decoupled static modules (`archive`, `audio`, `debug`, `di`, `entry`, `input`, `job`, `live2d`, `minigame`, `platform`, `render`, `resource`, `rpc`, `script`, `steam`, `storage`).
- **Composition Root**: `src/main.cpp` + `src/entry/` as the sole backend instantiators.
- **Dependency Injection**: `BackendRegistry` with pure virtual `api/I*.h` interfaces and zero concrete cross-module inclusions.
- **Script Runtime**: LuaJIT/Wasmoon VM with LPeg tokenization, AOT compilation, and coroutine scheduler.
- **Web Player**: Vite/Wasmoon PWA with offline Service Worker and Web App Manifest.
- **Distribution Pipeline**: CPack Windows ZIP, Vite/WASM Web ZIP, Android Gradle Release APK/AAB, SHA-256 checksums, and JSON release manifest.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | Windows CPack Binary Package | `CaesuraAmeKAG-1.0.0-rc.1-win64.zip` bundling exe, DLLs, assets, shaders | M1 (R1) | ORIGINAL_REQUEST §1 |
| 2 | Web Standalone Static Bundle | Static Web distribution bundling engine + demo game via `package_game.sh` (`CaesuraAmeKAG-1.0.0-rc.1-web.zip`) | M1 (R1) | ORIGINAL_REQUEST §1 |
| 3 | Android Signed Release Packages | `CaesuraAmeKAG-1.0.0-rc.1-android.apk` and `.aab` via `build_android_release.sh` with keystore signing & zipalign | M1 (R1) | ORIGINAL_REQUEST §1 |
| 4 | Unified SHA-256 & Release Manifest | `artifacts/dist/checksums.txt` and `artifacts/dist/release-manifest.json` | M1 (R1) | ORIGINAL_REQUEST §1 |
| 5 | Font Glyph Atlas & Cache Benchmark | Dynamic FreeType atlas + CJK pre-baked atlas rasterization and lookup speed | M2 (R2) | ORIGINAL_REQUEST §2 |
| 6 | Audio 3-Bus Mixer Concurrency Benchmark | SoLoud BGM/VOICE/SE bus allocation, voice ducking, handle churn & culling | M2 (R2) | ORIGINAL_REQUEST §2 |
| 7 | Large Script Tokenization Benchmark | 9600+ token LPeg parsing and scheduler throughput (>150 tok/ms) | M2 (R2) | ORIGINAL_REQUEST §2 |
| 8 | Backlog Memory Overhead Benchmark | 500+ page backlog heap growth (<4096 KB) and save serialization scaling | M2 (R2) | ORIGINAL_REQUEST §2 |
| 9 | Frame Render & CPU Dispatch Benchmark | DFS layer traversal (<500 µs/frame) and CPU/GPU skinning budget | M2 (R2) | ORIGINAL_REQUEST §2 |
| 10 | Performance Baseline Documentation | Comprehensive telemetry documentation in `docs/design/engine-performance-baseline.md` | M2 (R2) | ORIGINAL_REQUEST §2 |
| 11 | Web Player Service Worker (sw.js) | Cache-First static asset caching, offline gameplay, and fast load times | M3 (R3) | ORIGINAL_REQUEST §3 |
| 12 | Web App Manifest & Icons | `manifest.webmanifest` standalone mode, landscape orientation, icon assets | M3 (R3) | ORIGINAL_REQUEST §3 |
| 13 | Mobile Web Audio & Orientation Helpers | Touch-to-unlock AudioContext + `screen.orientation.lock` + CSS portrait overlay | M3 (R3) | ORIGINAL_REQUEST §3 |
| 14 | Sample Game Polish | Enhance `demo/example_game/` with `[tween]`, `[vfx bloom/vignette]`, UI style presets | M4 (R4) | ORIGINAL_REQUEST §4 |
| 15 | Project Templates Polish | Polish `tools/project_templates/` and validate zero syntax/contract errors | M4 (R4) | ORIGINAL_REQUEST §4 |
| 16 | Full Suite E2E Regression Verification | Pass all 1052+ C++ tests and 158+ Lua test suites (0 failed, 0 skipped) | M5 (Final) | ORIGINAL_REQUEST §Constraints |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | R1: Multi-Platform Release Packaging | Windows ZIP, Web ZIP, Android APK/AAB, SHA-256 checksums, manifest | none | IN_PROGRESS |
| 2 | R2: Performance Baseline Profiling | 5 subsystem benchmarks and documentation in engine-performance-baseline.md | none | PLANNED |
| 3 | R3: Web Player PWA & Mobile UX | sw.js caching, manifest, icons, orientation helpers | none | PLANNED |
| 4 | R4: Creator Tools & Sample Game Polish | [tween], [vfx], UI presets in demo & templates, contract validation | none | PLANNED |
| 5 | Final: Full Suite & Release Gate | 1052+ C++ tests, 158+ Lua tests, coupling count, forensic audit | M1, M2, M3, M4 | PLANNED |

## Code Layout
- `src/` — 16 C++ engine modules with strict `api/` interfaces.
- `scripts/` — Lua runtime, KAG commands, tokenizer, compiler, packaging scripts (`package_game.sh`, `package_distribution.py`).
- `web/` — Web runtime, Vite build, `sw.js`, `manifest.webmanifest`, AudioContext bridge.
- `android/` — Android Gradle project, JNI bindings, asset packaging.
- `demo/` & `tools/project_templates/` — Visual novel sample games and starter templates.
- `tests/` — C++ doctest suites (`tests/cpp/`) and Lua test suites (`tests/scripts/`).
- `docs/` — Documentation (`docs/design/`, `docs/plans/`, `docs/api/`).
- `artifacts/dist/` — Final release distribution packages, checksums, and manifest.
