# Engine Architecture Topology

> 16 modules, 30 interfaces, 0 circular dependencies. Last updated: 2026-06-13.

## Module Dependency Graph

```
                    ┌──────────────┐
                    │   main.cpp   │
                    │  (create all │
                    │   backends)  │
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │    entry/    │
                    │   Engine     │
                    │ 4-phase init │
                    └──────┬───────┘
                           │ registers all I* into
                    ┌──────▼───────┐
                    │     di/      │
                    │BackendRegistry│◄──── ALL module access through here
                    │ (26 I* ptrs) │
                    └───┬───┬───┬──┘
                        │   │   │  (interface pointers)
        ┌───────────────┼───┼───┼───────────────┐
        │               │   │   │               │
   ┌────▼────┐   ┌──────▼───▼───▼──────┐   ┌────▼────┐
   │ Runtime │   │    Content Systems   │   │Platform │
   │  Core   │   │                      │   │  & Infra│
   └─────────┘   └──────────────────────┘   └─────────┘
```

### Layer 1: Composition Root
- **main.cpp**: Creates concrete backends, fills EngineConfig, passes to Engine.
- **entry/Engine**: 4-phase init, registers all backends into BackendRegistry.

### Layer 2: Dependency Injection
- **di/BackendRegistry**: Singleton with 26 `I*` getters. The ONLY access point.
- **di/TextureBudget**: 6-tier auto-detection (128MB–4GB).
- **di/SandboxQuota**: Resource counting.

### Layer 3a: Runtime Core (per-frame hot path)

| Module | Files | Interfaces | Frame Role |
|--------|-------|-----------|------------|
| **render** | 22 .cpp, 25 .h | 6 (`IRenderDevice`, `ITextureManager`, `ILayerManager`, `IParticleSystem`, `IGpuMonitor`, `IVideoPlayer`) | GPU draw: layers → textures → particles → video → submit |
| **script** | 9 .cpp, 10 .h | 1 (`ILuaManager`) | Lua coroutine resume → scheduler.run() → KAG dispatch |
| **audio** | 2 .cpp, 3 .h | 1 (`IAudioBackend`) | SoLoud tick: BGM cross-fade, Voice interrupt, SE spatial |
| **resource** | 6 .cpp, 9 .h | 2 (`IAssetProvider`, `IAsyncLoader`) | Worker-thread decode → main-thread poll → texture registration |

### Layer 3b: Content Systems (scene-level)

| Module | Files | Interfaces | Role |
|--------|-------|-----------|------|
| **live2d** | 6 .cpp, 9 .h | 1 (`IAnimationBackend`) | Model load → motion play → expression set → render |
| **minigame** | 4 .cpp, 7 .h | 1 (`IMiniGameBackend`) | enter(3D) → update(physics) → render(GPU) → leave(KAG) |
| **storage** | 4 .cpp, 5 .h | 2 (`ISaveManager`, `ISaveProvider`) | Save/Load JSON → AES encrypt → schema migrate |
| **archive** | 6 .cpp, 10 .h | 3 (`IArchiveReader`, `IArchiveWriter`, `ICryptoEngine`) | CARC pack → AES-256-GCM → Ed25519 sign |

### Layer 3c: Platform & Infrastructure (supporting)

| Module | Files | Interfaces | Role |
|--------|-------|-----------|------|
| **platform** | 2 .cpp, 3 .h | 1 (`IPlatformBackend`) | SDL3 window → events → native handles |
| **input** | 1 .cpp, 2 .h | 1 (`IInputRouter`) | SDL events → KAG/Game focus dispatch |
| **job** | 1 .cpp, 2 .h | 1 (`IJobSystem`) | Thread pool → submit(prio, onComplete) → poll |
| **rpc** | 2 .cpp, 4 .h | 2 (`IEditorServer`, `IRpcServer`) | HTTP :9876 → 9 endpoints → CORS |
| **debug** | 3 .cpp, 4 .h | 1 (`IDebugManager`) | Ring buffer logs → frame profiles → subsys stats |
| **steam** | 1 .cpp, 3 .h | 1 (`ISteamBackend`) | Achievements → stats → cloud (conditional) |

## Per-Frame Data Flow

```
Frame N:
┌─────────────────────────────────────────────────────────┐
│ 1. platform::pollEvent()                                │
│    → SDL3 event → InputRouter.processEvent()            │
│    → if click: resume KAG coroutine                     │
├─────────────────────────────────────────────────────────┤
│ 2. job::pollMainThreadJobs()                            │
│    → collect async decode results                       │
│    → execute onComplete callbacks (main thread)         │
├─────────────────────────────────────────────────────────┤
│ 3. script::update(dt)                                   │
│    → Lua coroutine resume                               │
│    → scheduler.run() processes tokens                   │
│    → KAG commands → C++ bindings                        │
│    → BackendRegistry.get*() → concrete backend calls    │
│    → yield on [p] / [wait] / [endbutton]                │
├─────────────────────────────────────────────────────────┤
│ 4. audio::update(dt)                                    │
│    → SoLoud tick (streaming, fade, 3D)                  │
├─────────────────────────────────────────────────────────┤
│ 5. live2d::render(dt) + particles::update(dt)           │
│    → animation advance + particle physics               │
├─────────────────────────────────────────────────────────┤
│ 6. render::beginFrame()                                 │
│    → LayerManager::render() (BG → FG → MSG)             │
│    → ParticleSystem::render()                           │
│    → VideoPlayer::update() → texture upload             │
│    → endFrame() → commit_frame()                        │
├─────────────────────────────────────────────────────────┤
│ 7. debug::endFrameProfile()                             │
│    → record GPU submits, transient allocs, Lua GC       │
└─────────────────────────────────────────────────────────┘
```

## Dependency Rules (from AGENTS.md)

- Each module exposes only `api/I*.h` headers.
- No cross-module concrete `#include`.
- `entry/` and `main.cpp` are the only composition roots.
- `BackendRegistry` stores non-owning pointers; Engine owns `unique_ptr`.
- Adding a new backend: create `I*` → implement → add `set/get` to Registry → register in `Engine::init()`.
- 16 module directories: all lowercase (git case-sensitive).
- Interface files: `I` prefix + PascalCase (`IRenderDevice.h`).