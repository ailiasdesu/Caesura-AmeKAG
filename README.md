# Caesura (AmeKAG)

Cross-Platform Visual Novel Engine — SDL3 + bgfx + SoLoud + Lua 5.4 + nlohmann/json

## Quick Start

```powershell
git clone --recursive https://github.com/ailiasdesu/Caesura-AmeKAG.git
cd Caesura-AmeKAG
cmake -B build
cmake --build build --config Debug
.\build\Debug\CaesuraAmeKAG.exe
```

## Features

- **61 KAG commands** — 完整视觉小说脚本兼容
- **8 pure virtual interfaces** — IRenderDevice/IAudioBackend/IPlatformBackend/...
- **Structured save system** — nlohmann/json v3.11.3, schema versioning, Lua table ↔ JSON auto-conversion`r`n- **Video playback** — pl_mpeg (默认) + FFmpeg (条件编译)，8 Lua 函数
- **Live2D Cubism 5** — D3D11 ✅, 条件编译安全
- **3D mini-games** — BgfxMiniGameBackend, cube rendering + orbit camera
- **Delta CARC updates** — AES-256-GCM 加密差分补丁
- **Multi-threaded JobSystem** — 粒子/纹理/CARC/视频异步
- **Sandbox security** — Lua require 白名单 + I/O 禁用

## Architecture

```
SDL3 → bgfx (D3D11/Metal/OpenGL) → SoLoud → Lua 5.4

核心约束: 所有 Lua 绑定仅通过 BackendRegistry + I*Backend 抽象接口访问 C++
```

## Status

| Area | Completion |
|------|-----------|
| Core engine loop | 95% |
| KAG + Video + MiniGame API | ✅ |
| D3D11 rendering | ✅ Zero TDR |
| Core constraint | ✅ Fully enforced |
| Live2D (Windows) | ✅ |
| Tech debt | 18/22 closed |
| Cross-platform | Windows ✅ / Linux ⚠️ / macOS ⚠️ |


## Directory Structure

`
Caesura(AmeKAG)/
├── src/              # C++ engine core
│   ├── Core/         # Engine, JobSystem, VideoDecoder
│   ├── Render/       # bgfx render device, RenderBinding
│   ├── Scripting/    # Lua bindings, KAG parser, sandbox
│   ├── Resource/     # CARC, XP3, TextureManager
│   ├── MiniGame/     # 3D mini-game backend (PBR-lite)
│   └── Platform/     # SDL3, FileSystem
├── scripts/          # Lua runtime (demo, layers, sandbox)
├── shaders/          # bgfx shaders (DX11/Metal/GLSL)
├── assets/           # Demo assets (images, fonts, audio)
├── docs/             # Documentation
│   └── solutions/    # Documented solutions: bugs, patterns, workflows
├── CONCEPTS.md       # Shared domain vocabulary
└── ENGINE_ANALYSIS.md
`

## License

MIT — Live2D Cubism SDK 不包含在本仓库中