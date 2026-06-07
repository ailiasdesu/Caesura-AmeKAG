---
title: "Engine Architecture: Directory Layout, Naming, Dependencies, API Boundaries & Build Rules"
date: 2026-06-07
category: architecture-patterns
module: All
problem_type: architecture_pattern
component: development_workflow
severity: medium
applies_when:
  - "Adding a new subsystem or module"
  - "Writing cross-module includes"
  - "Creating a new shader variant or platform backend"
  - "Reviewing a PR for structural compliance"
tags: [architecture, directory-structure, naming-conventions, dependency-graph, cmake, api-boundary]
---

# Engine Architecture Reference

## 1. Directory Layout & Responsibility Boundaries

```
Caesura(AmeKAG)/
├── src/                    # C++ engine source (single CMake target)
│   ├── Core/               # Engine lifecycle, backend registry, platform abstraction
│   ├── Render/             # bgfx device, layer compositing, shaders, font, particles, RTT
│   ├── Audio/              # SoLoud wrapper + null backend stub
│   ├── Scripting/          # Lua bindings: KAG, Render, VFX, Debug, DevCore, GameState
│   ├── Carc/               # CARC container: crypto, CRL, reader/writer, asset provider
│   ├── Resource/           # IAssetProvider chain, async loader, XP3 reader, handles
│   ├── System/             # Save manager and save binding
│   ├── Debug/              # Debug protocol + hot reload
│   ├── Platform/           # Mobile adapter (P2 placeholder)
│   └── main.cpp            # Entry point: engine init, config, CARC verify, main loop
│
├── scripts/                # Lua runtime (loaded at engine start)
│   ├── kag/                # KAG parser, scheduler, conductor, init
│   │   └── commands/       # One file per KAG tag category (audio, layer, text, vfx...)
│   ├── dev/                # Developer tools (hotreload)
│   ├── *.lua               # Top-level modules: config, layers, blend, flow, i18n...
│   └── game_logic.lua      # Per-game entry point (gitignored)
│
├── shaders/                # GPU shader source + compiled bytecode
│   ├── dx11/               # HLSL source (.hlsl) + compiled DXBC (.dxbc)
│   ├── glsl/               # bgfx shaderc source (.sc) for Vulkan/OpenGL
│   └── metal/              # Metal Shading Language (.metal) for macOS/iOS
│
├── tests/                  # Test infrastructure
│   ├── *.lua               # Lua test scripts (full_story_parse, integration, g9)
│   ├── scripts/            # Test .ks story files
│   └── carc_test/          # CARC test fixtures (keys, test archive, input)
│
├── tools/                  # Standalone build tools (separate CMake targets)
│   └── carc_pack/          # CARC archive packer
│
├── external/               # Vendored third-party libraries (gitignored, built via CMake)
│   ├── bgfx/               # Cross-platform rendering library (bx + bimg + bgfx)
│   ├── freetype/           # Font rasterization
│   ├── lua/                # Lua 5.4 VM
│   ├── soloud/             # Audio engine
│   ├── SDL3/               # Platform abstraction (windowing, input)
│   ├── zstd/               # Compression
│   ├── ed25519/            # Ed25519 signature (public domain)
│   ├── lpeg/               # Lua parsing expression grammar
│   ├── pl_mpeg/            # MPEG1 video decode
│   └── stb/                # stb_image, stb_truetype
│
├── assets/                 # Game assets (i18n language files)
├── docs/                   # Specification, plans, solutions archive
├── saves/                  # Runtime save data (gitignored)
├── CMakeLists.txt          # Single top-level build file
├── CONCEPTS.md             # Domain vocabulary
├── README.md / BUILD.md / ALPHA_NOTES.md
└── Caesura_*.md            # Implementation specification
```

### Responsibility Matrix

| Directory | Owns | Does NOT Own |
|-----------|------|-------------|
| `Core/` | Engine, BackendRegistry, DebugManager, ErrorUI, platform interfaces, SDL3 backend | Rendering, audio processing |
| `Render/` | bgfx device, layers, shaders, fonts, particles, RTT, textures, video | Platform window creation |
| `Audio/` | SoLoud init, bus control, waveform cache, fade | Platform audio device selection |
| `Scripting/` | Lua bindings (C++ → Lua glue) | Lua script logic (that lives in `scripts/`) |
| `Carc/` | AES-GCM, Ed25519, CRL, archive I/O | Asset provider orchestration |
| `Resource/` | Provider chain, async loading, asset handles | Crypto (delegates to Carc/) |
| `System/` | Save/load JSON, migration chain | Serialization format of game state |
| `Debug/` | Debug protocol, hot reload file watcher | Debug UI rendering |

---

## 2. Naming Conventions

### Files
- **Header/source pairing**: `Foo.cpp` + `Foo.h`, same directory. No `include/` subdirectory.
- **Interface prefix `I`**: `IAudioBackend.h`, `IPlatformBackend.h`, `IRenderDevice.h`, `IAssetProvider.h`
- **Binding suffix `Binding`**: `KAGBinding.cpp/h`, `RenderBinding.cpp/h`, `VFXBinding.cpp/h`
- **Manager suffix `Manager`**: `LayerManager`, `RTTManager`, `TextureManager`, `SaveManager`, `LuaManager`, `DebugManager`, `CRLManager`
- **Provider suffix `Provider`**: `CarcAssetProvider`, `DirAssetProvider`

### Classes (mirror file names)
- `class Engine` → `Engine.h` (PascalCase, no prefix)
- `class IAssetProvider` → `IAssetProvider.h` (interface)
- `class SoLoudAudioEngine` → `SoLoudAudioEngine.h` (concrete backend)
- `class BgfxRenderDevice` → `BgfxRenderDevice.h` (concrete backend)

### Namespaces
| Namespace | Modules | Convention |
|-----------|---------|-----------|
| `Caesura` | Core, Audio, Render, Debug, Scripting, System | PascalCase, top-level |
| `caesura` | Resource (interfaces) | lowercase, for abstract provider layer |
| `caesura::carc` | Carc (all classes) | lowercase nested, security isolation |
| `caesura::platform` | Platform/MobileAdapter | lowercase nested |

### Lua Modules
- **Top-level**: `config.lua`, `layers.lua`, `blend.lua` — lowercase, descriptive
- **Subsystem**: `kag/`, `dev/` directories with `init.lua` entry points
- **Commands**: `kag/commands/audio.lua`, `kag/commands/text.lua` — one file per tag category
- **Global proxy**: `_CAESURA_BACKEND` — all-caps snake for engine-internal globals

---

## 3. Dependency Rules

### Dependency Graph

```
                    ┌──────────┐
                    │   Core   │ (Engine, BackendRegistry, interfaces)
                    └────┬─────┘
           ┌─────────────┼─────────────┐
           ▼             ▼             ▼
      ┌─────────┐  ┌──────────┐  ┌──────────┐
      │  Audio  │  │  Render  │  │ Debug    │
      └─────────┘  └────┬─────┘  └────┬─────┘
                         │             │
                         ▼             ▼
                    ┌──────────┐  ┌──────────┐
                    │Scripting │  │System(save)│
                    └──────────┘  └──────────┘

   Independent modules (zero inbound deps from other src/):
      Carc/    Resource/    Platform/
```

### Rule Summary

| Rule | Detail |
|------|--------|
| **Core is the hub** | All subsystems depend on `Core/` for Engine, BackendRegistry, and interfaces. Core depends on nothing in `src/` except Audio/Render (which it owns via unique_ptr). |
| **Carc is isolated** | `Carc/` has zero dependencies on other `src/` modules. It is a pure crypto/container library in `caesura::carc` namespace. |
| **Resource is isolated** | `Resource/` depends only on `Carc/` (for `CarcAssetProvider`) and standard library. Does not depend on Core, Render, or Audio. |
| **Scripting bridges** | `Scripting/` depends on `Core/` (for Engine, LuaManager) and `Render/` (for IRenderDevice). It is the glue between C++ and Lua. |
| **Render never calls Audio** | No cross-dependency between media subsystems. |
| **No upward references** | A lower-level module never includes a higher-level module. `Carc/` never includes `Core/`. |
| **Include style** | Within same module: `"Foo.h"`. Cross-module: `"../OtherModule/Bar.h"` or `"OtherModule/Bar.h"` when the module name is unambiguous. |

### Forbidden Cross-Module References

- `Audio/` → `Render/` ❌
- `Render/` → `Scripting/` ❌
- `Carc/` → `Core/` ❌
- `Resource/` → `Core/` or `Render/` or `Audio/` ❌
- `System/` → `Render/` or `Audio/` ❌

---

## 4. Public API Boundaries

### Interface Headers (Abstract Contracts)

| Header | Defines | Consumed By |
|--------|---------|-------------|
| `Core/IAudioBackend.h` | `playBGM()`, `playVoice()`, `playSE()`, `stop*()`, `setVolume()` | Scripting bindings, Engine |
| `Core/IPlatformBackend.h` | `init()`, `getNativeWindowHandle()`, `pollEvents()` | Engine, Render (for window handle) |
| `Render/IRenderDevice.h` | `submitBatch()`, `submitBlend()`, `createTexture()`, `flushAllRTT()` | Scripting bindings |
| `Resource/IAssetProvider.h` | `read()`, `exists()`, `verify()`, `priority()` | ProviderChain, Engine CARC verify |

### Backend Registry (Service Locator)

```cpp
BackendRegistry::instance().setRenderDevice(m_renderDevice.get());   // Engine init
BackendRegistry::instance().setAudioBackend(m_audioBackend.get());
BackendRegistry::instance().setPlatformBackend(m_platformBackend.get());

// Lua bindings resolve at call time:
IRenderDevice& rd = BackendRegistry::instance().getRenderDevice();
```

The Registry holds raw pointers. The Engine owns the `unique_ptr` instances. Bindings never own backends.

### Lua Binding Pattern

Each binding module exposes a global Lua table:

```cpp
// KAGBinding creates global "KAG" table with functions like:
//   KAG.play_bgm(path), KAG.show_image(layer, path, x, y, opacity)
// RenderBinding creates global "Render" table
// DevCoreBinding creates global "DevCore" table
```

The `UnifiedBinding` module registers all bindings in dependency order. Engine init calls `UnifiedBinding::registerAll(L)`.

### Shader Public Surface

- **Source**: `shaders/dx11/*.hlsl`, `shaders/glsl/*.sc`, `shaders/metal/*.metal`
- **Compiled**: `shaders/dx11/*.dxbc` (embedded at build time via `EmbeddedShaders.cpp`)
- **Adding a shader**: (1) Write .hlsl/.sc/.metal source → (2) Compile to .dxbc → (3) Embed in `EmbeddedShaders.cpp` as `static const uint8_t[]` → (4) Register in `BgfxRenderDevice::initEmbeddedShaders()`

---

## 5. Build Rules (CMake)

### Single Target Architecture

The entire engine compiles as one CMake target `CaesuraAmeKAG`. There are no internal static libraries — all `src/` directories are flat-listed in `add_executable`. Tools like `carc_pack` are separate `add_subdirectory()` targets.

### Third-Party Integration Pattern

```cmake
# 1. Disable unwanted features before add_subdirectory
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/zstd/build/cmake ${CMAKE_BINARY_DIR}/zstd)

# 2. Single-header libraries compiled as STATIC
add_library(miniz STATIC external/miniz_fwd.h)
add_library(ed25519 STATIC ${ED25519_SOURCES})
add_library(lpeg STATIC ${LPEG_SOURCES})

# 3. Find-package for system libraries
find_package(SDL3 REQUIRED)
```

### Source File Organization in CMake

```cmake
set(CAESURA_SOURCES
    src/Core/Engine.cpp
    src/Render/BgfxRenderDevice.cpp
    # ... all .cpp files listed flat, no GLOB
)
set(CAESURA_HEADERS
    src/Core/Engine.h
    src/Render/BgfxRenderDevice.h
    # ... all .h files listed for IDE visibility
)
```

### Platform Isolation

```cmake
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_PLATFORM_WINDOWS)
    target_sources(${PROJECT_NAME} PRIVATE
        src/Core/SDL3PlatformBackend.cpp  # Windows SDL3 backend
    )
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_PLATFORM_MACOS)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_PLATFORM_LINUX)
endif()
```

### Optional Feature Flags

```cmake
option(CAESURA_VIDEO_FFMPEG "Enable FFmpeg H.264 decoding" OFF)
if(CAESURA_VIDEO_FFMPEG)
    find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat swscale avutil)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_VIDEO_FFMPEG)
endif()
```

In code, optional features are guarded with `#ifdef CAESURA_VIDEO_FFMPEG`.

### Build Configurations

| Config | Purpose |
|--------|---------|
| Debug | `CAESURA_ASSERT_MAIN_THREAD()` active, debug logging, Lua `debug.*` subset |
| Release | Assertions compiled out, full sandbox lockdown, optimized |

### Post-Build Copy

```cmake
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/external/SDL3/.../SDL3.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
)
```

---

## Quick Reference Card

| Question | Answer |
|----------|--------|
| Where do I add a new rendering feature? | `src/Render/` — header + source, update CMakeLists sources |
| Where do I add a Lua binding? | `src/Scripting/NewBinding.cpp/h` — register in `UnifiedBinding` |
| Can I include `Render/BgfxRenderDevice.h` from `Audio/`? | **No** — forbidden cross-module dependency |
| Can I include `Core/Engine.h` from `Scripting/`? | **Yes** — Scripting depends on Core |
| How do I add a shader? | 4-step pipeline: source → compile → embed → register |
| How do I add a third-party lib? | Add to `external/`, integrate in CMakeLists following existing patterns |
| How does Lua call C++? | Via Binding modules — `KAGBinding`, `RenderBinding`, etc. register global tables |
| How does C++ call Lua? | Via `LuaManager::loadScript()`, `lua_getglobal()` in bindings |
