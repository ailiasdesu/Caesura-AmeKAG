# Caesura (AmeKAG) — Cross-Platform Visual Novel Engine

> **16 modules · 30 interfaces · 605 tests / 2968 assertions · 78 KAG commands · 0 circular dependencies**
> C++20 · bgfx · SDL3 · SoLoud · Lua 5.4 · CMake · MIT License
> Live API census: `python scripts/api_stats.py` → `docs/api/api-stats.md`

Caesura is an open-source galgame/visual novel engine with Live2D, 3D mini-games, and AI-assisted workflows as first-class citizens. **Native KAG Neo-Genesis scripting** — the next-generation, modernized iteration of the KAG script language (evolved from KAG3, KAG3-compatible).

---

## Features

- **KAG Neo-Genesis scripting** — the next-generation KAG language: 78 contract commands with declarative schemas, expression evaluator, variables, control flow, `[expr]`/`[iscript]` Lua hybrid embedding, label/choice jumps, scenes, chapter routing. See the [KAG Neo-Genesis language whitepaper](docs/design/kag-neo-genesis-language.md).
- **Lua-first runtime** — direct `backend.*`/`layers.*` API or KAG scheduler; sandboxed strict mode with per-module whitelists
- **Multi-backend GPU** — D3D11 + OpenGL 4.3 verified on real GPUs; Metal render path complete (embedded GLSL/DXBC/Metal shaders)
- **Live2D Cubism 5** — D3D11 verified with zero shader warnings; OpenGL/Metal render paths implemented (SDK bundled in `thirdparty/`)
- **Scene-level debugger** — breakpoints, step/continue, pause-driven state inspection over RPC (editor workflow)
- **Hot scene reload** — edit `.ks` files and re-apply to the running scene without restart
- **Mod loader** — drop-in mods with merged config, script hooks and asset overrides
- **Recording / playback** — input capture drives auto-demo; replay → deterministic PNG frame export (`--export-replay`)
- **Accessibility** — closed captions, TTS hook, colorblind/contrast filter presets (deuteranopia/protanopia/tritanopia/grayscale/high-contrast)
- **Cloud saves** — pluggable providers: local, Steam Remote Storage (full Lua surface), HTTP REST endpoint with offline degrade
- **Steamworks** — achievements, stats, rich presence, cloud list/write/read/delete/quota; unconditionally registered with safe Null defaults
- **Mobile-ready** — SDL finger-event bridge, orientation change events, touch→mouse injection (P7)
- **AI-assisted authoring** — local LLM integration (`[ai_dialog]`), DevCore RPC `eval`/`run`
- **Editor host** — HTTP editor on :9876 + stdin/stdout JSON-RPC; web-editor frontend served when built

## Quick Start

### Build

```bash
# Windows (MSVC)
cmake -B build -DCAESURA_LIVE2D=OFF
cmake --build build --config Debug --parallel

# macOS / Linux
cmake -B build -DCAESURA_LIVE2D=OFF
cmake --build build -j$(nproc)
```

### Run Tests

```bash
cd build/tests/Debug
./CaesuraTests.exe --no-skip
```

### Launch Editor

`--editor` starts the HTTP editor host on `http://localhost:9876` with a hidden GPU window. Use `--editor-stdio` for newline-delimited JSON-RPC with GPU, or `--headless` for stdio RPC without GPU.

Set the environment variable `CAESURA_EDITOR_TOKEN` to require a bearer token on every HTTP editor request (sent as `Authorization: Bearer <token>`); when unset or empty, the editor stays open to local callers (loopback trust boundary). The token is visible to same-UID processes and inherited by child processes.

### Your First KAG Script

```kag
*start
[bg storage="scene01.png"]
[ch name="Hero" text="Welcome to Caesura."]
[p]
[end]
```

Save as `.ks` and execute it through the runtime. Remote `run`/`eval` currently return `unsupported_yieldable_execution` until their managed-coroutine path is implemented.

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                 Editor / automation client               │
└────────────────────────────┬─────────────────────────────┘
                             │ HTTP or stdin/stdout RPC DTO
┌────────────────────────────▼─────────────────────────────┐
│                 Host transport (main.cpp)                │
│       RpcServer / EditorServer · no direct Lua access    │
└────────────────────────────┬─────────────────────────────┘
                             │ owner-thread dispatcher pump
┌────────────────────────────▼─────────────────────────────┐
│                 Engine (C++20 src/entry/)                │
│  ┌─────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────┐   │
│  │ render  │ │  audio   │ │  script  │ │  resource   │   │
│  │  bgfx   │ │  SoLoud  │ │  Lua 5.4 │ │  async load │   │
│  └─────────┘ └──────────┘ └──────────┘ └─────────────┘   │
│  ┌─────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────┐   │
│  │ live2d  │ │ minigame │ │ storage  │ │  archive    │   │
│  │ Cubism  │ │  3D PBR  │ │save/load │ │ CARC+crypto │   │
│  └─────────┘ └──────────┘ └──────────┘ └─────────────┘   │
│  ┌─────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────┐   │
│  │platform │ │  input   │ │   job    │ │    steam    │   │
│  │  SDL3   │ │  router  │ │  thread  │ │  Steamworks │   │
│  └─────────┘ └──────────┘ └──────────┘ └─────────────┘   │
│  ┌─────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────┐   │
│  │  debug  │ │    di    │ │          │ │             │   │
│  │log+debug│ │ registry │ │          │ │             │   │
│  └─────────┘ └──────────┘ └──────────┘ └─────────────┘   │
└──────────────────────────────────────────────────────────┘
```

**16 modules with runtime backend access centralized through `BackendRegistry` and 30
pure-virtual interfaces.** The current CMake target graph has no circular dependencies;
remaining implementation-level dependencies are tracked in the architecture topology.

The engine is built as 16 internal static module libraries (15 subsystem libraries plus
the `entry` composition root) and 15 API-only `INTERFACE` targets. Applications and tests
link the source-free `Caesura::Engine` aggregate target; host applications and tests link
`Caesura::Rpc` explicitly. Production sources are compiled once while the shipped artifact
remains a single executable.

### Module Map

| # | Module | .cpp/.h | API | Role |
|---|--------|---------|-----|------|
| 1 | `render` | 20/26 | 6 | bgfx GPU rendering, layers, particles, video, GPU monitor |
| 2 | `script` | 10/11 | 1 | Lua VM, KAG bindings, GameState, tokenizer, scheduler |
| 3 | `resource` | 6/10 | 3 | Async asset loading, asset provider chain, image decode |
| 4 | `live2d` | 6/9 | 1 | Animation backend (Cubism SDK or PNG fallback) |
| 5 | `archive` | 6/10 | 3 | CARC archive format, AES-256-GCM, Ed25519 signing |
| 6 | `minigame` | 7/8 | 1 | 3D mini-game scenes (enter/update/render/leave loop) |
| 7 | `storage` | 3/5 | 2 | Save/load with encryption, schema migration, cloud sync |
| 8 | `audio` | 2/3 | 1 | SoLoud 3-bus audio (BGM/Voice/SE), 3D spatial |
| 9 | `entry` | 8/3 | — | Composition root: Engine + EngineConfig + 4-phase init |
| 10 | `di` | 4/7 | 3 | BackendRegistry + texture budget + sandbox quota |
| 11 | `debug` | 3/4 | 1 | Structured logging, frame profiling, subsystem stats |
| 12 | `platform` | 3/4 | 1 | SDL3/Null window, events, timing, native handles |
| 13 | `rpc` | 2/5 | 3 | HTTP/stdio transports plus owner-thread command DTO interface |
| 14 | `input` | 1/2 | 1 | SDL event routing (KAG ↔ Game focus switch) |
| 15 | `job` | 1/2 | 1 | Multi-threaded task system + NullJobSystem mock |
| 16 | `steam` | 1/3 | 1 | Steamworks achievements, stats, cloud saves (conditional) |

---

## 30 Abstract Interfaces

Runtime engine services are accessed through `BackendRegistry::instance()`. Host transport
adapters stay outside Engine/Registry and submit self-contained DTOs through `IRpcDispatcher`.
`main.cpp` owns `RpcServer` or `EditorServer`; neither transport holds `lua_State*`.
Every interface is a pure-virtual class in `src/<module>/api/I*.h`.

| Interface | Module | Implementation |
|-----------|--------|---------------|
| `IRenderDevice` | render | BgfxRenderDevice (D3D11/OpenGL/Metal) |
| `ITextureManager` | render | TextureManager (bimg + stb) |
| `ILayerManager` | render | LayerManager (BG/FG/MSG compositing) |
| `IParticleSystem` | render | ParticleSystem (2D GPU particles) |
| `IGpuMonitor` | render | GpuMonitor / NullGpuMonitor |
| `IVideoPlayer` | render | VideoPlayer (pl_mpeg / FFmpeg) |
| `ILuaManager` | script | LuaManager (Lua 5.4, instruction budget) |
| `IAssetProvider` | resource | DirProvider / CARCProvider chain |
| `IAsyncLoader` | resource | AsyncLoader (worker-thread decode) |
| `IResourceGenerationTracker` | resource | GenerationTracker (hot-reload handle generations) |
| `IAnimationBackend` | live2d | Live2DBackend / NullAnimationBackend |
| `IArchiveReader` | archive | CARCReader |
| `IArchiveWriter` | archive | CARCWriter |
| `ICryptoEngine` | archive | CryptoEngine (AES-256-GCM + Ed25519) |
| `IMiniGameBackend` | minigame | BgfxMiniGameBackend (reserved) |
| `ISaveManager` | storage | SaveManager (JSON, encrypted, schema v5) |
| `ISaveProvider` | storage | LocalFileSaveProvider / Cloud |
| `IAudioBackend` | audio | SoLoudAudioEngine |
| `ITextureBudget` | di | TextureBudget (auto-detect 6 tiers) |
| `ISandboxQuota` | di | SandboxQuotaService (Lua resource limits) |
| `IDeviceLostListener` | di | GPU resource loss/restoration observer contract |
| `IDebugManager` | debug | DebugManager (ring buffer, profiling) |
| `IPlatformBackend` | platform | SDL3PlatformBackend |
| `IEditorServer` | rpc | EditorServer (httplib, 18 endpoints) |
| `IRpcServer` | rpc | RpcServer (JSON-RPC) |
| `IRpcDispatcher` | rpc | Composition-root owner-thread dispatcher |
| `IInputRouter` | input | InputRouter (KAG/Game focus) |
| `IJobSystem` | job | JobSystem / NullJobSystem |
| `ISteamBackend` | steam | SteamBackend (conditional compile) |

---

## KAG Script Compatibility

72 KAG Neo-Genesis commands with declarative contracts across 9 categories (generated from `docs/api/command-contracts.md`): audio, layer, text, system, flow control, transition, VFX, video, resource/save.

```kag
*start
[bg storage="classroom.png" time="500"]
[playbgm storage="theme.ogg" volume="0.8"]
[ch name="Mei" text="Good morning!"]
[p]
[stopbgm time="300"]
[link target="chapter2"]
```

See: [Command Contracts](docs/api/command-contracts.md) (auto-generated, authoritative)

---

## Platform Support

| Platform | Renderer | Build | CI | Notes |
|----------|----------|:-----:|:--:|-------|
| Windows (MSVC) | D3D11 | ✓ | ✓ | Primary dev platform |
| Linux (GCC) | OpenGL | ✓ | ✓ | Source-build SDL3 |
| macOS (Clang) | Metal | ✓ | ✓ | Homebrew deps |

CI workflow: `.github/workflows/ci.yml` — build + test on all 3 platforms.

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++20 |
| Build | CMake 3.25+ |
| Rendering | bgfx (D3D11 / OpenGL / Metal) |
| Windowing | SDL 3.4 |
| Audio | SoLoud (BGM / Voice / SE buses) |
| Scripting | Lua 5.4 + sandbox + instruction budget |
| Text | FreeType + ASCII fallback atlas (CJK font asset not bundled) |
| Crypto | BCrypt (Win) / OpenSSL (Unix) |
| Networking | cpp-httplib (HTTP), nlohmann/json |
| Video | pl_mpeg (MPEG-1) + FFmpeg (optional) |
| Live2D | Cubism 5 SDK (optional, not bundled) |
| Archive | CARC format (AES-256-GCM + Ed25519) |
| Testing | doctest + CTest (use current fresh-build output) |
| Editor | HTTP on `--editor`; stdio RPC on `--editor-stdio` / `--headless` |

---

## Directory Structure

```
Caesura(AmeKAG)/
├── src/                    C++ engine (84 .cpp)
│   ├── archive/            CARC format + crypto
│   ├── audio/              SoLoud backend
│   ├── debug/              Logging + profiling
│   ├── di/                 BackendRegistry + quotas
│   ├── entry/              Engine composition root
│   ├── input/              SDL event routing
│   ├── job/                Thread pool
│   ├── live2d/             Animation backends
│   ├── minigame/           3D mini-game framework
│   ├── platform/           SDL3 window/events
│   ├── render/             bgfx GPU rendering
│   ├── resource/           Asset loading pipeline
│   ├── rpc/                HTTP/RPC servers
│   ├── script/             Lua VM + KAG bindings
│   ├── steam/              Steamworks (conditional)
│   └── storage/            Save/load system
├── scripts/                Lua runtime (kag/, tokenizer, scheduler)
├── tests/                  52 test files (605 cases / 2968 assertions)
│   └── mocks/              NullJobSystem for synchronous testing
├── docs/
│   ├── api/                Interface docs (Lua, KAG, C++, RPC, API statistics)
│   ├── design/             Architecture topology, safety, capability matrix
│   ├── guides/             Getting started, asset pipeline, Live2D setup
│   └── plans/              Execution plans and summaries
├── external/               3rd-party (bgfx, SDL3, SoLoud, Lua, FreeType...)
├── assets/                 Game assets
├── build/                  CMake build output
└── AGENTS.md               Agent constitutional constraints
```

---

## Documentation Index

| Document | Audience | Content |
|----------|----------|---------|
| [AGENTS.md](AGENTS.md) | AI agents & contributors | Module boundaries, interface rules, build/test gates |
| [api-stats.md](docs/api/api-stats.md) | Everyone | Live API census (auto-generated: interfaces, bindings, RPC, tests) |
| [kag-neo-genesis-language.md](docs/design/kag-neo-genesis-language.md) | Script authors | KAG Neo-Genesis language whitepaper: next-generation design, advanced features, KAG3 evolution |
| [editor-api-reference.md](docs/api/editor-api-reference.md) | Editor developers | RPC endpoints, Lua bindings, KAG commands, C++ interfaces |
| [cpp-interfaces.md](docs/api/cpp-interfaces.md) | Engine developers | All 28 I* pure-virtual interfaces (30 headers) |
| [command-contracts.md](docs/api/command-contracts.md) | Script authors | Auto-generated 78 KAG Neo-Genesis command contracts (types, clamping, interpolation) |
| [kag-commands.md](docs/api/kag-commands.md) | Script authors | Deprecated KAG3-compat reference (see command-contracts.md) |
| [lua-modules.md](docs/api/lua-modules.md) | Script authors | Lua binding module APIs (Render, VFX, KAG, Debug...) |
| [getting-started.md](docs/guides/getting-started.md) | New users | Build, project setup, first scene |
| [engine-architecture-topology.md](docs/design/engine-architecture-topology.md) | Architects | Module dependency topology, data flow |
| [engine-capability-matrix.md](docs/design/engine-capability-matrix.md) | Evaluators | 54 tracked capabilities and readiness limits |
| [backend-registry-dependency-guide.md](docs/design/backend-registry-dependency-guide.md) | QA engineers | BackendRegistry dependency matrix; thread-safety, sandbox and audit mechanisms (JobSystem, Lua sandbox, quotas) |

---

## License

Caesura (AmeKAG) — Copyright (c) 2025-2026 AiliasDesu. MIT License.

Third-party libraries retain their original copyrights: bgfx (BSD-2), SDL3 (zlib), SoLoud (zlib), Lua 5.4 (MIT), FreeType (FTL), zstd (BSD), nlohmann/json (MIT), ed25519 (CC0), stb (MIT/PD), pl_mpeg (MIT), cpp-httplib (MIT), doctest (MIT).

Live2D Cubism SDK is proprietary software by Live2D Inc. — users download separately.
