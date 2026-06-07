# Caesura (AmeKAG) — Cross-Platform Visual Novel Engine

SDL3 + bgfx + SoLoud + Lua · C++20 · Electron Editor

## Quick Start

### Prerequisites

- **Windows**: Visual Studio 2022 (MSVC), CMake 3.25+
- **macOS**: Xcode CLT, CMake 3.25+
- **Linux**: GCC 13+ / Clang 17+, CMake 3.25+, X11 devel
- **Editor**: Node.js 20+

### Clone & Build Engine

\\\ash
git clone https://github.com/your-org/Caesura-AmeKAG.git
cd Caesura-AmeKAG
cmake -B build -S .
cmake --build build --config Debug
\\\

### Run Tests

\\\ash
cmake --build build --config Debug --target CaesuraTests
./build/tests/Debug/CaesuraTests.exe
\\\

### Run Standalone Engine

\\\ash
./build/Debug/CaesuraAmeKAG.exe
\\\

### Headless Mode (for Editor)

\\\ash
./build/Debug/CaesuraAmeKAG.exe --headless --web-root web-editor/dist
\\\

### Visual Editor (Electron)

\\\ash
cd web-editor
npm install
npm run dev
\\\

## Architecture

\\\
src/
  Core/       Engine, BackendRegistry, Platform, Input, Sandbox
  Render/     bgfx device, Text, Textures, Particles, RTT, Video
  Audio/      SoLoud engine (BGM/VOICE/SE buses)
  Scripting/  Lua VM, KAG bindings, Render bindings, VFX, Sandbox
  Resource/   Asset providers (Dir, CARC, XP3), Async loader
  System/     Save/Load manager
  Carc/       Encrypted archive (Ed25519 + AES-GCM + zstd)
  Editor/     HTTP server for visual editor (cpp-httplib)
  Debug/      Hot reload, Debug protocol
  Platform/   Mobile adapter (stubs)

Electron (React 18 + Vite 5) ←→ HTTP :9876 ←→ EditorServer
                                         ↕
                                    Engine (SDL3+bgfx or headless)
\\\

## Dependencies (vendored in external/)

| Library | Purpose |
|---|---|
| SDL3 | Windowing, input, platform |
| bgfx + bx + bimg | Cross-platform GPU rendering |
| SoLoud | Audio engine |
| Lua 5.4 | Scripting VM |
| FreeType 2 | Font rasterization |
| zstd | CARC compression |
| ed25519 | CARC signing |
| cpp-httplib | Editor HTTP server |
| doctest | Unit testing |
| stb | Single-header image loaders |
| pl_mpeg | MPEG video decoding |

## License

TBD