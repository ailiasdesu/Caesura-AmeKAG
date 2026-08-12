# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **角色分工**：`AGENTS.md` 是权威规则宪章（模块边界、接口规范、BackendRegistry、命名、耦合预算、文档分类）；本文件是 Claude 操作手册（插件、构建/测试命令、架构速览、提交规范），不重复规则内容。

## Project Overview

Caesura (AmeKAG) is a cross-platform visual novel engine — C++20, bgfx rendering, SDL3 windowing, SoLoud audio, Lua 5.4 scripting. Its 16 internal static module libraries are separated by 15 API-only CMake targets and 30 pure-virtual interfaces (live census: `python scripts/api_stats.py`), then linked into one executable. Module-boundary and interface rules are authoritative in `AGENTS.md` §1–3.

## Plugin Requirements

**项目全程必须使用 compound-engineering 插件** (`compound-engineering@compound-engineering-plugin`)。所有开发任务——包括代码编写、审查、调试、规划、commit——都应优先使用 compound-engineering 提供的技能和 Agent，而非手动执行。

常用命令：
- `/ce-code-review` — 多维度代码审查
- `/ce-plan` — 制定实施计划
- `/ce-commit` / `/ce-commit-push-pr` — 规范提交
- `/ce-debug` — 系统化调试
- `/ce-brainstorm` — 方案构思
- `/ce-optimize` — 性能优化
- `/ce-polish` / `/ce-simplify-code` — 代码打磨/简化
- `/ce-doc-review` — 文档审查
- `/ce-release-notes` — 生成 release notes
- `/ce-worktree` — Git worktree 管理

## Build Commands

```bash
# Windows (MSVC, primary dev platform)
cmake -B build -DCAESURA_LIVE2D=OFF
cmake --build build --config Debug --parallel

# macOS / Linux (FFmpeg often unavailable — disable it)
cmake -B build -DCAESURA_LIVE2D=OFF -DCAESURA_ENABLE_FFMPEG=OFF
cmake --build build -j$(nproc)

# Optional: Live2D Cubism SDK (requires manual SDK download)
cmake -B build -DCAESURA_LIVE2D=ON -DCUBISM_SDK_ROOT="path/to/CubismSdkForNative-5-r.5"
```

**CMake options:**

| Option | Default | Purpose |
|--------|---------|---------|
| `CAESURA_LIVE2D` | `OFF` | Live2D Cubism SDK animation (needs manual SDK) |
| `CAESURA_ENABLE_FFMPEG` | `ON` | Hardware-accelerated video decoding (falls back to pl_mpeg) |
| `CAESURA_DEBUG` | `ON` (Debug) / `OFF` (Release) | Debug logging and assertions (`CAESURA_DEBUG` preprocessor define) |
| `CAESURA_HAS_STEAM` | (unset) | Set by toolchain when Steamworks SDK is available |

**Running the engine:**

```bash
# Normal launch
./build/Debug/CaesuraAmeKAG.exe

# Launch editor mode with stdin/stdout JSON-RPC and a hidden GPU window
./build/Debug/CaesuraAmeKAG.exe --editor
```

## Test Commands

```bash
# Run all tests (must run from build/tests/Debug — CWD matters for resource paths)
cd build/tests/Debug && ./CaesuraTests.exe

# doctest filters (combine freely):
./CaesuraTests.exe -tc="*SaveManager*"     # by test case name (wildcards)
./CaesuraTests.exe -ts="*Render*"          # by test suite name
./CaesuraTests.exe -tce="*Slow*"           # exclude matching cases
./CaesuraTests.exe -s                      # show success output even for passing tests
./CaesuraTests.exe -d                      # show test durations

# Run tests via CTest (specify the configuration for multi-config generators)
ctest -C Debug --test-dir build --output-on-failure

# Coupling analysis
python scripts/count_coupling.py --ci
```

Test sources are explicitly listed in `tests/CMakeLists.txt` (currently 55 `test_*.cpp` files, including `test_main.cpp`). `CaesuraTests` links the same internal static module libraries as the application through `Caesura::Engine`, plus `Caesura::Rpc`; it does not recompile a second copy of the production sources. The runner reports the authoritative test-case total, and every discovered case must pass. `NullJobSystem` in `tests/mocks/` provides synchronous testing. The test binary is at `build/tests/Debug/CaesuraTests.exe`.

## Lint & Format

```bash
# clang-format (WebKit-based, C++20, 120col, 4-space indent, pointer alignment left)
clang-format -i src/path/to/file.cpp
```

Key style points from `.clang-format`: `PointerAlignment: Left`, `ReferenceAlignment: Left`, `BreakBeforeBraces: Attach`, `SortIncludes: false`.

## Commit Conventions

Follow the format visible in git history: `type(scope): description`

- **Types**: `feat`, `fix`, `test`, `docs`, `review`, `merge`, `plan`
- **Scopes**: module name (`render`, `script`, `storage`, `rpc`, …) or layer (`p1`, `p2`, `kag`, `backend`)
- Examples: `feat(palette): add day/night mode toggle`, `fix(backend): route render_text to KAG.render_text`, `test(p2): add gallery, music_room, palette, i18n module tests`

Branches use `codex/<description>` or `feature/<description>` naming.

## High-Level Architecture

### Composition Root Pattern

The engine follows strict DI via `BackendRegistry` (in `src/di/`):

```
main.cpp  →  creates concrete backends  →  EngineConfig  →  Engine::init()
                                                              ↓
                                              registers I* pointers into BackendRegistry
                                                              ↓
                                              all other modules call BackendRegistry::instance().getXxx()
```

Rules (composition root privileges, registration order, `new`/`make_unique` restrictions) are in `AGENTS.md` §3–4 — not repeated here.

### Module Map

| Module | Role | Key Interfaces |
|--------|------|---------------|
| `entry` | Composition root, Engine lifecycle, EngineConfig | — (allowed to include everything) |
| `di` | BackendRegistry + TextureBudget + SandboxQuota | `ITextureBudget`, `ISandboxQuota` |
| `render` | bgfx GPU rendering, layers, particles, text, video, GPU recovery | `IRenderDevice`, `ITextureManager`, `ILayerManager`, `IParticleSystem`, `IGpuMonitor`, `IVideoPlayer`, `IDeviceLostListener` |
| `script` | Lua 5.4 VM, KAG bindings, GameState, tokenizer | `ILuaManager` |
| `audio` | SoLoud 3-bus audio (BGM/Voice/SE) | `IAudioBackend` |
| `resource` | Async asset loading, provider chain, image decode | `IAssetProvider`, `IAsyncLoader` |
| `archive` | CARC format, AES-256-GCM, Ed25519 signing | `IArchiveReader`, `IArchiveWriter`, `ICryptoEngine` |
| `storage` | Save/load with encryption, schema migration | `ISaveManager`, `ISaveProvider` |
| `platform` | SDL3 window, events, timing | `IPlatformBackend` |
| `rpc` | stdin/stdout JSON-RPC plus an unwired HTTP editor implementation | `IEditorServer`, `IRpcServer` |
| `input` | SDL event routing (KAG ↔ Game focus switch) | `IInputRouter` |
| `job` | Multi-threaded task system | `IJobSystem` |
| `live2d` | Animation (Cubism SDK or PNG fallback) | `IAnimationBackend` |
| `minigame` | 3D mini-game scenes (enter/update/render/leave) | `IMiniGameBackend` |
| `steam` | Steamworks achievements, stats, cloud saves (conditional: `CAESURA_HAS_STEAM`) | `ISteamBackend` |
| `debug` | Structured logging, frame profiling | `IDebugManager` |

### Scripting Layer

The Lua runtime lives in `scripts/` (copied to build output). Two execution modes:
- **Direct API**: Lua scripts call `backend.*` and `layers.*` directly (like Ren'Py)
- **KAG .ks scripts**: `scheduler.lua` tokenizes `.ks` files and dispatches to command handlers in `scripts/kag/commands/`

Both modes support **KAG+Lua hybrid scripting**: `[eval]`, `[emb]`, and `[iscript]...[/endscript]` tags embed Lua inside .ks files, and the `kag.*` API (`kag.jump`, `kag.call`, `kag.save_game`) enables Lua-to-KAG callbacks. Engine-side bindings are in `src/script/bindings/` — these expose C++ backends to Lua via the `ILuaManager` interface.

### Editor / RPC Server

Launch with `--editor` to start newline-delimited JSON-RPC on stdin/stdout with a hidden GPU window; `--editor-stdio` forces stdio (default), and `--editor` without it starts the HTTP editor on port 9876 (bearer token via `CAESURA_EDITOR_TOKEN`). Both transports are verified by `tests/headless_http_smoke.py` (21/21) and `tests/headless_rpc_smoke.py`.

### Test Architecture

Tests link the same internal static module libraries as the application. Each `test_<module>.cpp` uses doctest and `test_main.cpp` defines `main()`. Tests that need GPU resources should use default-construction + accessor verification unless they explicitly provide a GPU-capable environment.

### Coupling Budget

见 `AGENTS.md` §9（`entry`/`di`/`script` ≤14，其余模块 ≤4，超 5 个跨模块依赖必须先解耦）。检查命令：`python scripts/count_coupling.py`。

## Key Documentation

- `AGENTS.md` — authoritative rules for module boundaries, interfaces, BackendRegistry, naming (read first; 完整文档分类见 AGENTS.md §12)
- `docs/api/command-contracts.md` — auto-generated KAG Neo-Genesis command contracts reference (78 commands; supersedes kag-commands.md)
- `docs/api/lua-modules.md` — Lua binding API reference
- `docs/api/cpp-interfaces.md` — all 30 C++ interface definitions
- `docs/api/editor-api-reference.md` — RPC endpoints for the web editor
- `docs/design/engine-architecture-topology.md` — module dependency topology and data flow
- `docs/design/engine-capability-matrix.md` — 54 tracked capabilities and readiness limits
- `docs/guides/getting-started.md` — from clone to running demo
- `docs/solutions/` — past problem solutions organized by category (YAML frontmatter, searchable)

## CI

Three-platform CI (`.github/workflows/ci.yml`): Windows MSVC (Debug+Release), macOS Clang, Linux GCC. Linux builds SDL3 from source. The `release` job packages Windows via CPack (ZIP). Coupling count runs on Linux CI.

## Naming Conventions

见 `AGENTS.md` §6（模块目录全小写、`I` 前缀接口、PascalCase 实现、`Caesura::` 命名空间、include 路径约定）——规则不在此重复。

## Design Decisions (Recorded)

### No ImGui / Immediate-Mode GUI Framework
[R10-FIX] The engine intentionally does NOT use ImGui or any immediate-mode GUI framework.
Rationale:
- "Engine UI" (ErrorUI) uses direct bgfx rendering for crash resilience — no GUI framework dependency means no framework state corruption can block error display.
- All game-facing UI (Gallery, MusicRoom, Settings, History) is written in Lua using backend.render_text() + backend.create_solid_texture() + backend.draw_viewport().
- This keeps the C++ surface area minimal and gives content creators full control over UI appearance via Lua scripts.
- If a developer needs an editor/debug UI panel, they should add it via the Lua DevCore binding, not by linking ImGui into the engine binary.
