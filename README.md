# Caesura (AmeKAG) — Cross-Platform Visual Novel Engine

SDL3 + bgfx + SoLoud + Lua 5.4 · C++20

[![Alpha](https://img.shields.io/badge/status-alpha-blue)](https://github.com/ailiasdesu/Caesura-AmeKAG/releases)
[![Tests](https://img.shields.io/badge/tests-109%2F109%20%E2%9C%93-brightgreen)]()

## Quick Start

### Download & Run

1. [Download the latest release](https://github.com/ailiasdesu/Caesura-AmeKAG/releases)
2. Extract the zip
3. Double-click `CaesuraAmeKAG.exe`

A visual demo runs immediately — no setup, no dependencies.

### Build from Source

**Prerequisites:** Visual Studio 2022 (MSVC) + CMake 3.25+

```
git clone https://github.com/ailiasdesu/Caesura-AmeKAG.git
cd Caesura-AmeKAG
cmake -B build -S .
cmake --build build --config Debug
```

Run the engine:

```
./build/Debug/CaesuraAmeKAG.exe
```

### Run Tests

```
# C++ tests (109/109)
./build/tests/Debug/CaesuraTests.exe

# Lua tests (3 suites)
lua tests/scripts/run_lua_tests.lua
```

## What's Inside

| Module | Description |
|---|---|
| **KAG Script Engine** | 9 command modules, 53 tag handlers — bg, fg, text, audio, VFX, transitions, save/load |
| **Lua Sandbox** | Track 3 strict mode — DEFAULT DENY, EXPLICIT ALLOW. All rules in `scripts/sandbox.lua` (AI-auditable) |
| **CARC Packaging** | Encrypted archives with ed25519 signature + zstd compression |
| **VFX System** | Quake, flash, blur, fade, snow, rain — 1024 particle cap |
| **3D LUT Grading** | Real-time palette-based color correction |
| **Video Playback** | MPEG1 software decode via pl_mpeg |
| **IDE Connection** | stdin/stdout JSON-RPC for AI agent / external editor integration |
| **Hot Reload** | Monitors `scripts/` — changes take effect without restart |

## Architecture

```
src/
  Core/       Engine, BackendRegistry, Input, Debug, TextureBudget
  Render/     bgfx device, Text, Font, Textures, Particles, RTT, Video, ShaderCache
  Audio/      SoLoud (BGM / VOICE / SE buses)
  Scripting/  Lua VM, KAG bindings, Render/VFX/Debug bindings
  Resource/   Asset providers (Dir, CARC, XP3), Async loader
  System/     Save/Load manager with schema migration
  Carc/       Encrypted archive (ed25519 + AES-GCM + zstd)
  Debug/      Hot reload, Debug protocol
  Platform/   Mobile adapter

scripts/
  kag/         KAG parser, scheduler, conductor, 9 command modules
  demo.lua     4-phase visual demo (runs on launch)
  sandbox.lua  Security rules (AI-readable)
  config.lua   Engine configuration, backend selection
```

## Dependencies (vendored)

| Library | Purpose |
|---|---|
| SDL3 | Windowing, input, platform abstraction |
| bgfx + bx + bimg | Cross-platform GPU rendering (D3D11 / Metal / Vulkan) |
| SoLoud | Audio engine |
| Lua 5.4 | Embedded scripting runtime |
| FreeType 2 | TTF/OTF font rasterization |
| zstd | Fast compression |
| ed25519 | Digital signatures |
| doctest | C++ unit testing |
| stb | Image loading |
| pl_mpeg | MPEG video decoding |

## KAG Script Example

```kag
[bg storage="sky.png"]
[wait time=500]
[ch name="Hero" text="The wind carries the scent of cherry blossoms."]
[p]
[fg storage="hero_smile.png"]
[position layer="fg0" x=0.5 y=0.5 scale=1.0]
[playbgm storage="bgm/peaceful.ogg" volume=0.8]
```

## License

TBD