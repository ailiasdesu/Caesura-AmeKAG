# Caesura (AmeKAG)

Visual novel engine. Cross-platform. C++20 + SDL3 + bgfx + Lua 5.4.

[![CI](https://github.com/ailiasdesu/Caesura-AmeKAG/actions/workflows/ci.yml/badge.svg)](https://github.com/ailiasdesu/Caesura-AmeKAG/actions/workflows/ci.yml)
[![Alpha](https://img.shields.io/badge/status-alpha-blue)](https://github.com/ailiasdesu/Caesura-AmeKAG/releases)

## Quick Start

Download the [latest release](https://github.com/ailiasdesu/Caesura-AmeKAG/releases), extract, and run `CaesuraAmeKAG.exe`. A visual demo plays immediately — no dependencies.

## Build from Source
## Build from Source

**Requires:** CMake 3.25+, SDL3

### Windows (MSVC)
```
git clone https://github.com/ailiasdesu/Caesura-AmeKAG.git
cd Caesura-AmeKAG
cmake -B build -S .
cmake --build build --config Debug --parallel
```

### macOS (Clang)
```
brew install cmake sdl3 freetype zstd openssl@3
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix openssl@3)
cmake --build build --config Debug --parallel $(sysctl -n hw.logicalcpu)
```

### Linux (GCC)
```
sudo apt install cmake build-essential libfreetype-dev libzstd-dev libssl-dev libx11-dev
cmake -B build -S .
cmake --build build --config Debug --parallel $(nproc)
```

Run the engine:
```
./build/Debug/CaesuraAmeKAG.exe   # Windows


### Live2D Cubism (optional)

Enable via `-DCAESURA_LIVE2D=ON`. Requires [Cubism 5 SDK for Native](https://www.live2d.com/en/download/cubism-sdk/) placed at `CubismSdkForNative-5-r.5/`.

> ⚠️ **macOS**: OpenGL has been deprecated by Apple since 2018. Live2D uses OpenGL ES 2 for rendering and may not work on future macOS versions. A Metal-native renderer is planned but not yet implemented.

```
cmake -B build -S . -DCAESURA_LIVE2D=ON
cmake --build build --config Debug
```

./build/CaesuraAmeKAG             # macOS/Linux
```

## Tests

```
# C++ (140/140)
./build/tests/Debug/CaesuraTests.exe

# Lua (4 suites)
lua tests/scripts/run_lua_tests.lua
```

## Architecture

```
src/
  Core/        Engine lifecycle, BackendRegistry, Input, TextureBudget
  Render/      bgfx device, Text/Font, RTT manager, Particles, Video, ShaderCache
  Audio/       SoLoud engine (BGM / VOICE / SE buses, 3D spatial)
  Scripting/   Lua VM, KAG bindings, Render/VFX/Debug/IDE bindings
  Resource/    Asset providers (Dir, CARC, XP3), AsyncLoader, ImageDecoder
  System/      Save/Load with schema migration, SandboxQuota
  Carc/        Encrypted archive (ed25519 + AES-GCM + zstd + BCrypt/OpenSSL)
  Debug/       HotReload, DebugProtocol, GpuMonitor
  Editor/      IDE protocol (JSON-RPC stdin/stdout)
  Platform/    Mobile adapter, CRL manager
  MiniGame/    3D mini-game subsystem

scripts/
  kag/         Parser, scheduler, conductor, 9 command modules
  demo.lua     4-phase visual demo (runs on launch)
  sandbox.lua  Security rules — DEFAULT DENY, EXPLICIT ALLOW
  config.lua   Backend selection, engine configuration

tests/
  cpp/         140 C++ test cases (doctest)
  scripts/     4 Lua test suites (KAG commands, sandbox, etc.)
```

## Capabilities

| Module | Detail |
|---|---|
| KAG Script | 9 command modules, 53 tag handlers — bg, fg, text, audio, VFX, transitions, save/load |
| Lua Sandbox | Strict mode — all rules in `scripts/sandbox.lua`, AI-auditable |
| CARC Packaging | ed25519 signature + AES-GCM encryption + zstd compression |
| VFX System | Quake, flash, blur, fade, snow, rain — 1024 particle cap |
| 3D LUT Grading | Real-time palette-based color correction |
| Video Playback | FFmpeg with hardware decode fallback to pl_mpeg |
| Multi-Core | JobSystem work-stealing thread pool for async texture/video loading |
| IDE Protocol | JSON-RPC over stdin/stdout for AI agent / external editor |
| Hot Reload | Script changes take effect without restart |

## Dependencies

Vendored — no system packages required beyond SDL3:

| Library | Purpose |
|---|---|
| SDL3 | Windowing, input, platform |
| bgfx + bx + bimg | GPU rendering (D3D11 / Metal / Vulkan) |
| SoLoud | Audio engine |
| Lua 5.4 | Scripting runtime |
| FreeType 2 | Font rasterization |
| zstd | Compression |
| ed25519 | Digital signatures |
| doctest | C++ unit testing |
| stb | Image I/O |
| pl_mpeg | MPEG1 software decode |

## KAG Script

```
[bg storage="sky.png"]
[wait time=500]
[ch name="Hero" text="The wind carries the scent of cherry blossoms."]
[p]
[fg storage="hero_smile.png"]
[position layer="fg0" x=0.5 y=0.5 scale=1.0]
[playbgm storage="bgm/peaceful.ogg" volume=0.8]
```

## Release

```
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cd build && cpack -C Release -G ZIP
```

Output: `CaesuraAmeKAG-1.0.0-win64.zip` — ready to distribute.

## Live2D Cubism (Optional)

Live2D support requires the **Live2D Cubism 5 Native SDK**, which is proprietary software
and is **not** included in this repository.

### Setup

1. Download Cubism 5 Native SDK from [live2d.com](https://www.live2d.com/en/download/cubism-sdk/)
2. Configure with the SDK path:

```
cmake -B build -DCAESURA_LIVE2D=ON -DLIVE2D_SDK_ROOT="path/to/CubismSdkForNative" ...
```

### Legal

- Cubism SDK is Live2D Inc. proprietary software — subject to its own license
- This engine does **not** redistribute Cubism SDK in source or binary form
- The engine's MIT license does **not** apply to Cubism SDK


## License

TBD
