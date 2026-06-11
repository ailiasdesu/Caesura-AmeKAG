# Caesura (AmeKAG) — Cross-Platform Visual Novel Engine

> Cross-platform · AI-assisted IDE · Live2D · 3D Mini-games · MIT License
> **v1.0-rc** | CI: **Win/Mac/Linux** | Hotkeys: **F5 to Run** | Audit: **Zero Violations**

Caesura is a modern engine for visual novel creators. KAG script compatible, cross-platform rendering, built-in Electron visual editor, AI-assisted code generation.

---

## Quick Start

### Build

```bash
# Windows (Visual Studio 2022)
cmake -B build_nol2d -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build_nol2d --config Release

# macOS / Linux
cmake -B build -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build
```

### Launch Editor

```bash
cd web-editor
npm install
npm run dev
```

Editor auto-spawns the engine via stdin/stdout JSON-RPC. Press **F5** to run, **Shift+F5** to stop.

### Run Demo

```bash
build_nol2d/Release/CaesuraAmeKAG.exe --demo     # Windows
./build/CaesuraAmeKAG --demo                      # macOS / Linux
```

Demo covers 3 scenes: classroom rendering, MiniGame 3D, save system.

### Your First KAG Script

```lua
-- hello.cae
KAG.show_text(nil, "Hello, Visual Novel!")
KAG.wait_click()
KAG.clear_text()
```

Paste into the editor and press **F5**.

---

## Architecture

```
Electron Editor (React + Vite + CodeMirror 6)
       |  JSON-RPC (stdin/stdout, 8 methods)
Engine (C++20 + SDL3 + bgfx + SoLoud + Lua 5.4)
  |-- Render (bgfx D3D11/OpenGL/Metal)
  |-- Audio (SoLoud 3-bus: BGM/VOICE/SE)
  |-- Scripting (Lua VM + Sandbox + 84 KAG APIs)
  |-- System (Save + AES-256-GCM + v1-v5 Schema)
  |-- MiniGame (PBR-lite + 15 Lua APIs)
  |-- Live2D (Cubism 5 + 4 render paths)
  |-- CARC (Crypto + Delta updates + Ed25519)
  |-- Debug (HotReload + TDR protection)
```

---

## 8 Abstract Interfaces

All Lua to C++ access through BackendRegistry + pure virtual interfaces:

| Interface | Implementation | Status |
|-----------|---------------|:------:|
| IRenderDevice | BgfxRenderDevice (D3D11/OpenGL/Metal) | OK |
| IAudioBackend | SoLoudAudioEngine | OK |
| IPlatformBackend | SDL3PlatformBackend | OK |
| IAssetProvider | Dir -> XP3 -> CARC chain | OK |
| IAnimationBackend | Live2DBackend (Cubism 5) | OK Win |
| IMiniGameBackend | BgfxMiniGameBackend (PBR-lite) | OK |
| IVideoDecoder | PlMpegDecoder / FFmpegDecoder | OK |
| ISaveProvider | LocalFileSaveProvider (AES-256-GCM) | OK |

---

## Editor Features

| Panel | Function |
|-------|----------|
| Stage | Canvas 2D preview + engine real-time frames (200ms polling) |
| Code | CodeMirror 6 syntax highlight, autocomplete, error markers |
| AI | @generate / @fix commands, multi-backend (OpenAI/Codex/Custom) |
| Assets | Scene list + asset tree, drag to stage generates code |
| Log | Engine stderr real-time forwarding, error/warning classification |
| RPC | ping/run/stop/eval/getFrame/getState/assets/logs |
| Hotkeys | F5 run / Shift+F5 stop / Ctrl+, settings / Ctrl+S save |
| Package | One-click build (Win/Mac/Linux), electron-builder |

---

## KAG Script Compatibility

84 KAG commands, 9 sub-modules: layer / text / audio / resource / save / system / transition / vfx / video.

See: [docs/api/KAG-API.md](docs/api/KAG-API.md)

## MiniGame 3D

15 Lua APIs: spawn_cube/sphere/plane, set_camera, PBR materials, multi-light, collision, physics.

See: [docs/api/MiniGame-API.md](docs/api/MiniGame-API.md)

---

## Platform Support

| Platform | Renderer | Build | CI | Live2D |
|----------|----------|:-----:|:--:|:------:|
| Windows  | D3D11    | OK | OK | OK |
| Linux    | OpenGL   | OK | OK | Deferred |
| macOS    | Metal    | OK | OK | Deferred |

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Engine | C++20, CMake 3.25+ |
| Render | bgfx (D3D11/OpenGL/Metal) |
| Windowing | SDL3 |
| Audio | SoLoud (3 buses) |
| Scripting | Lua 5.4 + sandbox |
| Font | FreeType + CJK atlas |
| Crypto | BCrypt (Win) / OpenSSL (Unix) |
| Video | pl_mpeg + FFmpeg (optional) |
| Live2D | Cubism 5 SDK (optional, not committed) |
| Editor | Electron 42 + React 18 + Vite 5 + CodeMirror 6 |
| AI | OpenAI API / Codex CLI / custom endpoint |

---

## Directory Structure

```
Caesura(AmeKAG)/
src/              C++ engine source (65 .cpp + 73 .h, 10 modules)
  Core/           Engine, BackendRegistry, JobSystem, RpcServer
  Render/         BgfxRenderDevice, particles, text, video, shaders
  Audio/          SoLoud (BGM/VOICE/SE)
  Scripting/      Lua VM, KAGBinding (31), RenderBinding, VFXBinding
  System/         SaveManager (AES-256-GCM, v1-v5 schema)
  CARC/           CryptoEngine, CARC Reader/Writer, DeltaCARC
  Resource/       AssetManager, AsyncLoader, ProviderChain, XP3
  Live2D/         Cubism 5 backend (4 render paths, conditional)
  MiniGame/       PBR-lite, 15 Lua APIs, cross-platform shaders
  Debug/          DebugManager, HotReload, TDR protection
scripts/          Lua scripts (45 files)
  kag/            KAG modules (init, backend, sandbox)
  kag/commands/   9 sub-modules (audio/layer/text/system/...)
  demo/           Demo scenes (3 scenes)
web-editor/       Electron + React editor (12 components, F5 to run)
tests/            C++ unit tests (24 files, 135/138 pass) + Lua E2E
docs/             Documentation (API, solutions, plans)
external/         3rd-party libraries (12 total, all static)
```

---

## Known Limitations

| Item | Status | Notes |
|------|:------:|-------|
| Live2D macOS | Deferred | Metal render path for macOS developers |
| Live2D Linux | Deferred | OpenGL render path for Linux developers |
| Mobile | Planned | MobileAdapter interface complete, awaiting platform build |

---

## License

Caesura (AmeKAG) — Copyright (c) 2025-2026 AiliasDesu. MIT License.

Third-party libraries retain their original copyrights:
bgfx (BSD-2), SDL3 (zlib), SoLoud (zlib), Lua (MIT), FreeType (FTL),
zstd (BSD), nlohmann/json (MIT), ed25519 (CC0), stb (MIT/PD),
pl_mpeg (MIT), cpp-httplib (MIT).

Live2D Cubism SDK is proprietary software by Live2D Inc.
Users must download it separately from live2d.com.

