# Caesura (AmeKAG)

Cross-Platform Visual Novel Engine — SDL3 + bgfx + SoLoud + Lua 5.4

## Quick Start

```powershell
# Clone
git clone https://github.com/ailiasdesu/Caesura-AmeKAG.git
cd Caesura-AmeKAG

# Configure (Windows)
cmake -B build
cmake --build build --config Debug

# Run demo
.\build\Debug\CaesuraAmeKAG.exe
```

## Requirements

- CMake 3.25+
- C++20 compiler (MSVC 2022 / GCC 13+ / Clang 16+)
- SDL3 (system or CMake find_package)

## Optional Features

| Feature | CMake Option | Default | Description |
|---------|-------------|---------|-------------|
| Live2D Cubism 5 | `-DCAESURA_LIVE2D:BOOL=ON -DCUBISM_SDK_ROOT:PATH=...` | OFF | Live2D 动态立绘支持 |
| FFmpeg Video | `-DCAESURA_VIDEO_FFMPEG:BOOL=ON` | OFF | 高级视频解码 (当前使用 pl_mpeg) |

## Architecture

```
SDL3 → bgfx (D3D11/Metal/OpenGL) → SoLoud → Lua 5.4
  7 纯虚接口: IRenderDevice / IAudioBackend / IPlatformBackend /
              IAssetProvider / IAnimationBackend / ILive2DRenderPath / IMiniGameBackend
  61 KAG 命令: 完整覆盖文本/音频/图层/过渡/VFX/视频/存档
```

## Directory Structure

```
Caesura(AmeKAG)/
├── src/
│   ├── Core/          Engine, BackendRegistry, DebugManager, ErrorUI, JobSystem
│   ├── Render/        bgfx device, layers, text, textures, particles, video
│   ├── Audio/         SoLoud backend (BGM/VOICE/SE)
│   ├── Scripting/     Lua bindings (KAG/Render/VFX/DevCore/Debug)
│   ├── System/        Save/Load (JSON + migration)
│   ├── Carc/          AES-256-GCM encrypted resource packs
│   ├── Resource/      Asset pipeline (XP3/CARC/Dir + async loading)
│   ├── Animation/     Live2D Cubism 5 (conditional compile)
│   ├── MiniGame/      3D mini-game interface (reserved)
│   ├── Debug/         HotReload + debug protocol
│   ├── Editor/        JSON-RPC IDE server
│   └── Platform/      Mobile adapter stubs
├── scripts/           Lua game logic + KAG command handlers
├── shaders/           bgfx shader sources (dx11/glsl/metal)
├── tests/             C++ unit + Lua integration tests
├── external/          10 static libs (bgfx/bx/bimg/SoLoud/Lua/FreeType/zstd/ed25519/pl_mpeg/cpp-httplib)
└── tools/             CARC pack CLI tool
```

## Status

| Area | Completion |
|------|-----------|
| Core engine loop | 95% |
| KAG script compatibility | 61 commands |
| D3D11 rendering (Windows) | ✅ Stable |
| Live2D Cubism 5 | ✅ Windows D3D11 |
| Cross-platform CI | 🔜 Last priority |
| MiniGame 3D | ❌ Interface only |
| Editor (Electron) | ⚠️ Backend ready |

## License

MIT
