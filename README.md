# Caesura (AmeKAG) — Cross-Platform Visual Novel Engine

> **16 modules · 31 interfaces · 119 KAG Neo-Genesis command contracts · 0 circular dependencies**
> C++20 · bgfx · SDL3 · SoLoud · Lua 5.4 · CMake · MIT License
> Live API census: `python scripts/api_stats.py` → `docs/api/api-stats.md`

<p align="center">
  <b>Platforms</b>&nbsp;🪟 Windows&nbsp;&nbsp;🍎 macOS&nbsp;&nbsp;🐧 Linux&nbsp;&nbsp;🌐 Web&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>Status</b>&nbsp;✅ Tests passing&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>Contracts</b>&nbsp;119 KAG commands&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>Interfaces</b>&nbsp;31 pure-virtual
  <br>
  <sub><i>文本徽章（不依赖外部 CI 服务，计数取自阶段 G 最近审计）— 测试状态见
  <a href="docs/plans/audit/ROADMAP-200.md">ROADMAP-200</a></i></sub>
</p>

Caesura is an open-source galgame/visual novel engine with Live2D, 3D mini-games, and AI-assisted workflows as first-class citizens. **Native KAG Neo-Genesis scripting** — the next-generation, modernized iteration of the KAG script language (evolved from KAG3, KAG3-compatible).

---

## Features

- **KAG Neo-Genesis scripting** — the next-generation KAG language: 119 contract commands with declarative schemas, expression evaluator, variables, control flow, `[expr]`/`[iscript]` Lua hybrid embedding, label/choice jumps, scenes, chapter routing. See the [KAG Neo-Genesis language whitepaper](docs/design/kag-neo-genesis-language.md).
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

## 示例游戏（Sample Games）

引擎自带一个完整的示例游戏 **《单程回信》（The One-Way Reply）**，位于
[`demo/example_game/`](demo/example_game/)。

- **类型**：现代校园 · 温情悬疑 · 短篇多结局视觉小说（约 15–18 分钟）
- **结局**：3 个——真结局「归零」 / 好结局「同行」 / 普通结局「守约」，均以 `[ending]` 解锁画廊
- **演示能力**：多章节流程、玩家选择与信任差分、变量/插值/表达式、参数化宏、Lua 混合、
  双存档点、中英 i18n 热切换、转场/粒子/后处理、SMA 骨骼动画小游戏融合、backlog/跳过/自动
- **启动**：`lua demo/example_game/entry.lua`（或 `bash scripts/package_game.sh` 一键打包为静态 Web 站）

| 文档 | 内容 |
|------|------|
| [DESIGN.md](demo/example_game/DESIGN.md) | 完整设计文档：世界观、角色、8 个流程节点、三结局分流、能力展示清单（验收依据） |
| [README.md](demo/example_game/README.md) | 快速上手：演示能力表、结构、修改剧本、静态校验命令 |

**验证**：`bash scripts/verify_sample_game.sh`（ks_check 契约校验 + headless 端到端跑通
DONE + 三结局可达，5/5 PASS，见 [sample-game-verification.md](docs/guides/sample-game-verification.md)）。

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

**16 modules with runtime backend access centralized through `BackendRegistry` and 31
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

## 31 Abstract Interfaces

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
| `IMeshRenderer` | render | MeshRenderer (2D skeletal-mesh / SMA characters) |
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
| `IMobileAdapter` | platform | MobileAdapter (SDL finger events, orientation, touch→mouse) |
| `IEditorServer` | rpc | EditorServer (httplib, 18 endpoints) |
| `IRpcServer` | rpc | RpcServer (JSON-RPC) |
| `IRpcDispatcher` | rpc | Composition-root owner-thread dispatcher |
| `IInputRouter` | input | InputRouter (KAG/Game focus) |
| `IJobSystem` | job | JobSystem / NullJobSystem |
| `ISteamBackend` | steam | SteamBackend (conditional compile) |

---

## KAG Script Compatibility

119 KAG Neo-Genesis commands with declarative contracts across 9 categories (generated from `docs/api/command-contracts.md`): system, text, layer, audio, transition, vfx, save, resource, video.

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
├── tests/                  C++ 976 用例 · Lua 131 套件 + 24 孤儿 · Web 297 · Editor 530（阶段 G round 108-109 基线）
│   └── mocks/              NullJobSystem for synchronous testing
├── demo/                   Sample library（tutorial 01–16 + showcase + example_game + sma_demo）
├── docs/
│   ├── api/                Interface docs (Lua, KAG, C++, RPC, API statistics)
│   ├── design/             Architecture topology, safety, capability matrix
│   ├── guides/             Getting started, asset pipeline, sample library, packaging UX
│   └── plans/              Execution plans and summaries
├── external/               3rd-party (bgfx, SDL3, SoLoud, Lua, FreeType...)
├── assets/                 Game assets
├── build/                  CMake build output
└── AGENTS.md               Agent constitutional constraints
```

---

## Documentation Index

> 文档按用途分 5 类，规则见 [AGENTS.md §12](AGENTS.md#12-文档分类)。阶段 G 新增文档以 **🆕** 标记。

### api/ — API 参考（自动生成文档为权威）

| 文件 | 内容 |
|------|------|
| [command-contracts.md](docs/api/command-contracts.md) | 119 个 KAG Neo-Genesis 命令的声明式契约参考（自动生成，权威） |
| [lua-modules.md](docs/api/lua-modules.md) | Lua 模块 API 参考 |
| [cpp-interfaces.md](docs/api/cpp-interfaces.md) | 全部 31 个 C++ 纯虚接口定义 |
| [editor-api-reference.md](docs/api/editor-api-reference.md) | 编辑器 RPC 端点参考 |
| 🆕 [scene-builder-rpc-bridge.md](docs/api/scene-builder-rpc-bridge.md) | Scene Builder 面板 ↔ 引擎 RPC 桥接手册（round 108） |
| [api-stats.md](docs/api/api-stats.md) | 实时 API 普查（自动生成） |
| [kag-commands.md](docs/api/kag-commands.md) | 已弃用的 KAG3 兼容参考（被 command-contracts.md 取代） |
| [kag-expression-language.md](docs/api/kag-expression-language.md) | `[if]`/`[eval]`/`${}` 表达式语法参考 |

### design/ — 架构与设计文档

| 文件 | 内容 |
|------|------|
| [engine-architecture-topology.md](docs/design/engine-architecture-topology.md) | 引擎架构拓扑说明（16 模块 + 数据流） |
| [engine-capability-matrix.md](docs/design/engine-capability-matrix.md) | 79 项能力的完成状态矩阵 |
| [engine-safety-and-qa-mechanisms.md](docs/design/engine-safety-and-qa-mechanisms.md) | JobSystem 线程安全、Lua 沙箱、BackendRegistry 依赖说明 |
| [engine-topology-mermaid.md](docs/design/engine-topology-mermaid.md) | Mermaid 架构拓扑图源码 |
| [backend-registry-dependency-guide.md](docs/design/backend-registry-dependency-guide.md) | BackendRegistry 依赖矩阵与使用规范 |
| [nextgen-kag-standard.md](docs/design/nextgen-kag-standard.md) | KAG Neo-Genesis 标准定义 |
| [kag-neo-genesis-language.md](docs/design/kag-neo-genesis-language.md) | KAG Neo-Genesis 语言白皮书（下一代设计、KAG3 演变） |
| [engine-performance-baseline.md](docs/design/engine-performance-baseline.md) | 性能基线（round 101–109 持续刷新：大型资源压测 / Web 帧率预算 / bundle vs source） |
| 🆕 [save-security-audit.md](docs/design/save-security-audit.md) | 存档防篡改审计：AES-GCM 覆盖、nonce 复用、回滚防护（round 104） |
| [engine-market-comparison.md](docs/design/engine-market-comparison.md) | 2026-08-03 市场对比（历史快照） |
| [engine-market-analysis-2026-08-06.md](docs/design/engine-market-analysis-2026-08-06.md) | 2026-08-06 市场分析（数据更新版） |

### guides/ — 用户与开发者指南

| 文件 | 内容 |
|------|------|
| [getting-started.md](docs/guides/getting-started.md) | 从克隆到 Demo 可跑的入门指南 |
| [sample-library.md](docs/guides/sample-library.md) | **示例库总览**：已收录示例 + 教程路径 01–16 + 覆盖矩阵 |
| [asset-pipeline.md](docs/guides/asset-pipeline.md) | 支持的资源格式与目录规范 |
| [carc-packaging.md](docs/guides/carc-packaging.md) | CARC 打包格式与工具使用 |
| [live2d-setup.md](docs/guides/live2d-setup.md) | Cubism SDK 集成步骤 |
| [kag3-import.md](docs/guides/kag3-import.md) | KAG3 工程导入 |
| [kag-language-tour.md](docs/guides/kag-language-tour.md) | KAG Neo-Genesis 语言速查 |
| 🆕 [metal-readiness.md](docs/guides/metal-readiness.md) | Metal 渲染路径就绪度审计（round 103） |
| 🆕 [android-build.md](docs/guides/android-build.md) | Android（NDK/arm64-v8a）构建链（round 103） |
| 🆕 [cross-platform-verification.md](docs/guides/cross-platform-verification.md) | 三平台 × 能力域验证矩阵（round 103） |
| 🆕 [sample-game-assets.md](docs/guides/sample-game-assets.md) | 示例游戏《单程回信》资产审计与 i18n 键预留（round 105） |
| 🆕 [sample-game-verification.md](docs/guides/sample-game-verification.md) | 示例游戏端到端验证设施（round 105） |
| 🆕 [packaging-ux.md](docs/guides/packaging-ux.md) | 一键打包分发：itch.io / GitHub Pages / Netlify（round 108） |

### plans/ — 执行记录与当前计划（按日期命名）

| 文件 | 内容 |
|------|------|
| [2026-08-16-021-delivery-handoff.md](docs/plans/2026-08-16-021-delivery-handoff.md) | **阶段 F 收官交接**（round 100 / round 99 完成态基线） |
| [ROADMAP-200.md](docs/plans/audit/ROADMAP-200.md) | **阶段 G 路线图**（round 101–，产品化：真机验证/后处理/SMA/示例游戏/打包分发） |
| [2026-08-12-004-generation-gap-roadmap.md](docs/plans/2026-08-12-004-generation-gap-roadmap.md) | 代差路线图（五大战役，权威规划） |
| [ROADMAP-100.md](docs/plans/audit/ROADMAP-100.md) | 100 轮冲刺轮次记录（round 1–100，权威） |

### solutions/ — 经验与模式（YAML frontmatter 可搜索）

| 文件 | 内容 |
|------|------|
| [architecture-patterns/engine-constructor-sigsegv-testing.md](docs/solutions/architecture-patterns/engine-constructor-sigsegv-testing.md) | Engine 构造崩溃的 NullGpuMonitor 解决模式 |
| [architecture-patterns/header-only-to-instance-class.md](docs/solutions/architecture-patterns/header-only-to-instance-class.md) | 头文件内联类重构为实例类模式 |
| [build-errors/clean-build-include-path.md](docs/solutions/build-errors/clean-build-include-path.md) | 全量构建 include 路径修复模式 |
| [runtime-crashes/bgfx-predefined-uniform-name-conflict.md](docs/solutions/runtime-crashes/bgfx-predefined-uniform-name-conflict.md) | bgfx 预定义 uniform 命名冲突 |
| [deferred-gpu-tests.md](docs/solutions/deferred-gpu-tests.md) | 无 GPU 环境下无法覆盖的测试项清单 |

---

## 示例库清单（Sample Library）

> 完整覆盖矩阵与教程逐行讲解见 [docs/guides/sample-library.md](docs/guides/sample-library.md)。

| 路径 | 主题 | 验证状态 |
|------|------|----------|
| [demo/example_game/](demo/example_game/) | 《单程回信》完整示例游戏（15–18 分钟，三结局 + SMA + i18n） | ✅ headless E2E 5/5 PASS（DONE + 三结局可达），ks_check 零警告 |
| [demo/tutorial/](demo/tutorial/) | 教程路径 **01–16**：从最小剧本到声明式补间 [tween]（递进式教学，每例独立可跑） | ✅ 引擎 tokenize/compile + Web 播放器双验证，ks_bake 通过 |
| [demo/showcase.ks](demo/showcase.ks) | Command Showcase：26 个命令全展示 | ✅ 引擎 tokenize/compile + Web 播放器（DONE:53 + ending 解锁） |
| [demo/galgame_demo.ks](demo/galgame_demo.ks) | 核心 VN 流程演示（bg/ch/playbgm/voice/sprite/ending） | ✅ Web flow 集成测试（round 109 部署默认 game） |
| [demo/full_pipeline_demo.ks](demo/full_pipeline_demo.ks) | 全管线流程（资产+脚本+播放） | ✅ ks_bake bundle |
| [demo/sma_demo.ks](demo/sma_demo.ks) | SMA 骨骼动画命令演示 | ✅ ks_bake bundle |

**示例游戏详情**：《单程回信》DESIGN/README 见上文「示例游戏」节；教程 01–16 的学习内容与所用命令表见 sample-library.md。

---

## 社区与支持（Community & Support）

想提问、晒作品、参与引擎开发还是分享创作心得？社区主阵地是 **GitHub Discussions**
（对话）与 **Issues**（Bug / 功能追踪）。完整入口、话题分类与学习路径见
**[docs/guides/community.md](docs/guides/community.md)**。

- **💬 提问 / 求助** — Discussions「提问」（先看 [getting-started](docs/guides/getting-started.md) 与已有讨论）
- **🎨 作品展示** — Discussions「作品展示」：用引擎做的游戏 / 场景 / 立绘
- **🔧 引擎开发** — Discussions「引擎开发」：接口 / 渲染 / 脚本 / 构建
- **✍️ 内容创作** — Discussions「内容创作」：剧本写作 / 美术音频 / 经验
- **🐛 Bug / 功能请求** — Issues（见 [CONTRIBUTING.md](CONTRIBUTING.md) 与 Issue 模板）
- **🚀 发布作品** — `bash scripts/package_game.sh` 一键打包为静态站 → itch.io / GitHub Releases / GitHub Pages（见 [packaging-ux.md](docs/guides/packaging-ux.md)）

> **想参与贡献？** 从 [CONTRIBUTING.md](CONTRIBUTING.md) 开始：Fork → 分支 → 语义提交 → PR
> （合并门禁：全量构建零错误 + C++ / Lua / Web / Editor 四套件测试全绿 + 耦合门禁）。

## License

Caesura (AmeKAG) — Copyright (c) 2025-2026 AiliasDesu. MIT License.

Third-party libraries retain their original copyrights: bgfx (BSD-2), SDL3 (zlib), SoLoud (zlib), Lua 5.4 (MIT), FreeType (FTL), zstd (BSD), nlohmann/json (MIT), ed25519 (CC0), stb (MIT/PD), pl_mpeg (MIT), cpp-httplib (MIT), doctest (MIT).

Live2D Cubism SDK is proprietary software by Live2D Inc. — users download separately.