# Engine Architecture Topology (Mermaid)

```mermaid
graph TB
    subgraph "Composition Root"
        main["main.cpp<br/>Create backends<br/>→ Engine"]
        engine["entry/Engine<br/>4-phase init<br/>registers to Registry"]
    end

    subgraph "Dependency Injection"
        registry["di/BackendRegistry<br/>26 I* pointers<br/>Singleton access"]
    end

    subgraph "Runtime Core"
        script["script (Lua 5.4)<br/>VM · coroutines · KAG bindings<br/>GameState · tokenizer<br/>instruction-budget sandbox"]
        render["render (bgfx)<br/>GPU device · layers<br/>textures · particles<br/>video · GPU monitor"]
        audio["audio (SoLoud)<br/>BGM/Voice/SE buses<br/>3D spatial · fade"]
        resource["resource<br/>async loader · asset providers<br/>image decoder"]
    end

    subgraph "Content Systems"
        live2d["live2d<br/>Cubism SDK / PNG fallback<br/>model · motion · expression"]
        minigame["minigame<br/>3D scenes<br/>enter → update → render → leave"]
        storage["storage<br/>encrypted save/load<br/>schema migration<br/>cloud sync"]
        archive["archive<br/>CARC packaging<br/>AES-256-GCM · Ed25519"]
    end

    subgraph "Platform & Infrastructure"
        platform["platform (SDL3)<br/>window · events · timing<br/>native handles"]
        input["input<br/>KAG ↔ Game focus<br/>resize callbacks"]
        job["job<br/>thread pool<br/>onComplete callbacks"]
        steam["steam<br/>achievements · stats<br/>cloud saves"]
        rpc["rpc<br/>HTTP editor server<br/>9 endpoints"]
        debug["debug<br/>structured logging<br/>frame profiling"]
        di_core["di<br/>texture budget 6 tiers<br/>sandbox quota"]
    end

    main --> engine
    engine --> registry

    registry -.->|"IRenderDevice*"| render
    registry -.->|"ILuaManager*"| script
    registry -.->|"IAudioBackend*"| audio
    registry -.->|"IAsyncLoader*"| resource
    registry -.->|"IAnimationBackend*"| live2d
    registry -.->|"IMiniGameBackend*"| minigame
    registry -.->|"ISaveManager*"| storage
    registry -.->|"IArchiveReader*"| archive
    registry -.->|"IPlatformBackend*"| platform
    registry -.->|"IInputRouter*"| input
    registry -.->|"IJobSystem*"| job
    registry -.->|"ISteamBackend*"| steam
    registry -.->|"IEditorServer*"| rpc
    registry -.->|"IDebugManager*"| debug
    registry -.->|"ITextureBudget*"| di_core

    script -->|"KAG commands →"| audio
    script -->|"KAG commands →"| render
    script -->|"KAG commands →"| storage
    script -->|"KAG commands →"| resource
    script -->|"KAG commands →"| live2d
    script -->|"KAG commands →"| minigame

    render -->|"flush RTT on shutdown"| platform
    audio -->|"decode on worker"| job
    resource -->|"image decode on worker"| job
    minigame -->|"update() on worker"| job
```

## Module Descriptions

### Composition Root (entry/)
- **entry/Engine.cpp** — The only place that creates concrete backend objects.
- Four-phase init: `initPlatformPhase()` → `initScriptingPhase()` → `initAssetPhase()` → `initOptionalPhase()`.
- Registers all backends into `BackendRegistry` for the rest of the engine to access.

### Dependency Injection (di/)
- **BackendRegistry** — Singleton storing 26 non-owning `I*` pointers. All subsystem access goes through `::instance().get*()`.
- **TextureBudget** — Auto-detects 6 memory tiers (128MB–4GB), LRU eviction on overflow.
- **SandboxQuota** — Resource counting for Lua sandbox (textures, emitters, handles).

### Runtime Core
- **render** (22 .cpp, 6 interfaces) — bgfx-based GPU rendering. IRenderDevice for draw calls, ILayerManager for BG/FG/MSG compositing with dirty-rect optimization, ITextureManager for async texture lifecycle with budget enforcement, IParticleSystem for 2D GPU particles, IGpuMonitor for adaptive quality, IVideoPlayer for MPEG-1/FFmpeg playback.
- **script** (9 .cpp, 1 interface) — Lua 5.4 VM with instruction-budget sandbox. KAG tokenizer and scheduler (coroutine-based). 7 Lua binding modules (Render, VFX, KAG, Debug, DevCore, Save, Steam).
- **audio** (2 .cpp, 1 interface) — SoLoud 3-bus audio. BGM (cross-fade), Voice (interrupt), SE (2D/3D spatial). Per-bus volume, playback position query.
- **resource** (6 .cpp, 2 interfaces) — Async asset loading pipeline. IAssetProvider chain (Dir → CARC, priority-ordered). Worker-thread image decode (bimg + stb fallback).

### Content Systems
- **live2d** (6 .cpp, 1 interface) — Animation backend abstraction. Live2DBackend (requires Cubism 5 SDK) or NullAnimationBackend (PNG/JPG static sprite fallback).
- **minigame** (4 .cpp, 1 interface) — 3D mini-game framework. Enter/update/render/leave lifecycle. update() safe for worker threads, render() main-thread only. Lua bridge.
- **storage** (4 .cpp, 2 interfaces) — JSON save/load with AES-256-GCM encryption. Schema migration (v1–v5). ISaveProvider abstraction (local filesystem / cloud).
- **archive** (6 .cpp, 3 interfaces) — CARC archive format. AES-256-GCM encryption + Ed25519 signing. Reader/writer with key management.

### Platform & Infrastructure
- **platform** (2 .cpp, 1 interface) — SDL3 window, event polling, native handles for bgfx.
- **input** (1 .cpp, 1 interface) — SDL event router with KAG/Game focus switching.
- **job** (1 .cpp, 1 interface) — Multi-threaded task system. submit() with priority + onComplete callback. NullJobSystem mock for synchronous testing.
- **steam** (1 .cpp, 1 interface) — Steamworks achievements, stats, cloud saves. Conditional compile.
- **rpc** (2 .cpp, 2 interfaces) — HTTP editor server (9 endpoints: ping, status, assets, run, stop, logs, Live2D models/load, build). CORS enabled.
- **debug** (3 .cpp, 1 interface) — Structured logging (ring buffer), frame profiling, subsystem stats, GPU submit tracking.

## Game Frame Data Flow

```
SDL3 pollEvent() → InputRouter → KAG callback
  → Lua coroutine resume
  → scheduler.run() processes tokens
  → KAG commands dispatch to C++ bindings
  → BackendRegistry.get*() → concrete backend calls

Per frame:
  1. platform::pollEvent()    → input routing
  2. job::pollMainThreadJobs() → collect async results
  3. script::update(dt)       → resume Lua coroutine
  4. audio::update(dt)       → SoLoud tick
  5. debug::endFrameProfile() → record GPU submits
  6. render::beginFrame() → LayerManager::render() → endFrame() → commit_frame()
```