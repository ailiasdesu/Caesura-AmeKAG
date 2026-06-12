# Caesura (AmeKAG) — Cross-Platform Visual Novel Engine

> **16 modules · 30 interfaces · 324 tests · 0 circular dependencies**
> C++20 · bgfx · SDL3 · SoLoud · Lua 5.4 · CMake · MIT License

Caesura is an open-source galgame/visual novel engine with Live2D, 3D mini-games, and AI-assisted workflows as first-class citizens. KAG 3.0 script compatible.

---

## Quick Start

### Build

```bash
# Windows (MSVC)
cmake -B build -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build --config Debug --parallel

# macOS / Linux
cmake -B build -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build -j$(nproc)
```

### Run Tests

```bash
cd build/tests/Debug
./CaesuraTests.exe        # 324/324 passed
```

### Launch Editor

The web-based editor is maintained separately by frontend collaborators. Start the engine with `--editor` to activate the HTTP RPC server (port 9876), then open the editor frontend.

### Your First KAG Script

```kag
*start
[bg storage="scene01.png"]
[ch name="Hero" text="Welcome to Caesura."]
[p]
[end]
```

Save as `.ks` and execute via the editor's Run button or `POST /api/run`.

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Web Editor (React)                    │
│              HTTP RPC (9 endpoints) :9876                │
├──────────────────────────────────────────────────────────┤
│                   Engine (C++20 entry/)                  │
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
│  │  rpc    │ │  debug   │ │   di     │ │             │   │
│  │  HTTP   │ │ log+prof │ │ registry │ │             │   │
│  └─────────┘ └──────────┘ └──────────┘ └─────────────┘   │
└──────────────────────────────────────────────────────────┘
```

**16 modules, all communicating through `BackendRegistry` via 30 pure-virtual interfaces.**
Zero circular dependencies. Zero concrete `#include` across module boundaries.

### Module Map

| # | Module | .cpp/.h | API | Role |
|---|--------|---------|-----|------|
| 1 | `render` | 22/25 | 6 | bgfx GPU rendering, layers, particles, video, GPU monitor |
| 2 | `script` | 9/10 | 1 | Lua VM, KAG bindings, GameState, tokenizer, scheduler |
| 3 | `resource` | 6/9 | 2 | Async asset loading, asset provider chain, image decode |
| 4 | `live2d` | 6/9 | 1 | Animation backend (Cubism SDK or PNG fallback) |
| 5 | `archive` | 6/10 | 3 | CARC archive format, AES-256-GCM, Ed25519 signing |
| 6 | `minigame` | 4/7 | 1 | 3D mini-game scenes (enter/update/render/leave loop) |
| 7 | `storage` | 4/5 | 2 | Save/load with encryption, schema migration, cloud sync |
| 8 | `audio` | 2/3 | 1 | SoLoud 3-bus audio (BGM/Voice/SE), 3D spatial |
| 9 | `entry` | 3/3 | — | Composition root: Engine + EngineConfig + 4-phase init |
| 10 | `di` | 3/6 | 2 | BackendRegistry + texture budget + sandbox quota |
| 11 | `debug` | 3/4 | 1 | Structured logging, frame profiling, subsystem stats |
| 12 | `platform` | 2/3 | 1 | SDL3 window, events, timing, native handles |
| 13 | `rpc` | 2/4 | 2 | HTTP editor server (9 endpoints), JSON-RPC |
| 14 | `input` | 1/2 | 1 | SDL event routing (KAG ↔ Game focus switch) |
| 15 | `job` | 1/2 | 1 | Multi-threaded task system + NullJobSystem mock |
| 16 | `steam` | 1/3 | 1 | Steamworks achievements, stats, cloud saves (conditional) |

---

## 30 Abstract Interfaces

All subsystem access through `BackendRegistry::instance()`. Every interface is a pure-virtual class in `src/<module>/api/I*.h`.

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
| `IAnimationBackend` | live2d | Live2DBackend / NullAnimationBackend |
| `IArchiveReader` | archive | CARCReader |
| `IArchiveWriter` | archive | CARCWriter |
| `ICryptoEngine` | archive | CryptoEngine (AES-256-GCM + Ed25519) |
| `IMiniGameBackend` | minigame | BgfxMiniGameBackend (reserved) |
| `ISaveManager` | storage | SaveManager (JSON, encrypted, schema v5) |
| `ISaveProvider` | storage | LocalFileSaveProvider / Cloud |
| `IAudioBackend` | audio | SoLoudAudioEngine |
| `ITextureBudget` | di | TextureBudget (auto-detect 6 tiers) |
| `ISandboxQuota` | di | SandboxQuota (Lua resource limits) |
| `IDebugManager` | debug | DebugManager (ring buffer, profiling) |
| `IPlatformBackend` | platform | SDL3PlatformBackend |
| `IEditorServer` | rpc | EditorServer (httplib, 9 endpoints) |
| `IRpcServer` | rpc | RpcServer (JSON-RPC) |
| `IInputRouter` | input | InputRouter (KAG/Game focus) |
| `IJobSystem` | job | JobSystem / NullJobSystem |
| `ISteamBackend` | steam | SteamBackend (conditional compile) |

---

## KAG Script Compatibility

68 KAG 3.0 commands across 9 categories: audio (13), layer (6), text (13), system (4), flow control (16), transition (4), VFX (1), video (2), resource/save (9).

```kag
*start
[bg storage="classroom.png" time="500"]
[playbgm storage="theme.ogg" volume="0.8"]
[ch name="Mei" text="Good morning!"]
[p]
[stopbgm time="300"]
[link target="chapter2"]
```

See: [KAG Command Reference](docs/api/kag-commands.md)

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
| Text | FreeType (embedded atlas + CJK) |
| Crypto | BCrypt (Win) / OpenSSL (Unix) |
| Networking | cpp-httplib (HTTP), nlohmann/json |
| Video | pl_mpeg (MPEG-1) + FFmpeg (optional) |
| Live2D | Cubism 5 SDK (optional, not bundled) |
| Archive | CARC format (AES-256-GCM + Ed25519) |
| Testing | doctest (324/324) |
| Editor | React + Vite (separate repo by collaborators) |

---

## Directory Structure

```
Caesura(AmeKAG)/
├── src/                    C++ engine (76 .cpp, 105 .h)
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
├── tests/                  48 test files (324/324)
│   └── mocks/              NullJobSystem for synchronous testing
├── docs/
│   ├── api/                Interface docs (Lua, KAG, C++, RPC)
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
| [editor-api-reference.md](docs/api/editor-api-reference.md) | Editor developers | RPC endpoints, Lua bindings, KAG commands, C++ interfaces |
| [cpp-interfaces.md](docs/api/cpp-interfaces.md) | Engine developers | All 30 I* pure-virtual interfaces |
| [kag-commands.md](docs/api/kag-commands.md) | Script authors | 68 KAG commands with parameter types |
| [lua-modules.md](docs/api/lua-modules.md) | Script authors | Lua binding module APIs (Render, VFX, KAG, Debug...) |
| [getting-started.md](docs/guides/getting-started.md) | New users | Build, project setup, first scene |
| [engine-architecture-topology.md](docs/design/engine-architecture-topology.md) | Architects | Module dependency topology, data flow |
| [engine-capability-matrix.md](docs/design/engine-capability-matrix.md) | Evaluators | 79 capabilities across modules |
| [engine-safety-and-qa-mechanisms.md](docs/design/engine-safety-and-qa-mechanisms.md) | QA engineers | Thread safety, sandbox, audit mechanisms |

---

## License

Caesura (AmeKAG) — Copyright (c) 2025-2026 AiliasDesu. MIT License.

Third-party libraries retain their original copyrights: bgfx (BSD-2), SDL3 (zlib), SoLoud (zlib), Lua 5.4 (MIT), FreeType (FTL), zstd (BSD), nlohmann/json (MIT), ed25519 (CC0), stb (MIT/PD), pl_mpeg (MIT), cpp-httplib (MIT), doctest (MIT).

Live2D Cubism SDK is proprietary software by Live2D Inc. — users download separately.
