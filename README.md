# Caesura (AmeKAG)

Cross-Platform Visual Novel Engine — SDL3 + bgfx + SoLoud + Lua 5.4

## Quick Start

```powershell
# Clone
git clone https://github.com/ailiasdesu/Caesura-AmeKAG.git
cd Caesura-AmeKAG

# Configure & Build (Windows)
cmake -B build
cmake --build build --config Debug

# Run demo
.\build\Debug\CaesuraAmeKAG.exe
```

## Requirements

- CMake 3.25+
- C++20 compiler (MSVC 2022 / GCC 13+ / Clang 16+)
- SDL3 (bundled in external/)

## Optional Features

| Feature | CMake Option | Default | Description |
|---------|-------------|---------|-------------|
| Live2D Cubism 5 | `-DCAESURA_LIVE2D:BOOL=ON -DCUBISM_SDK_ROOT:PATH=...` | OFF | Live2D 动态立绘。需自行下载 Cubism 5 Native SDK |
| FFmpeg Video | `-DCAESURA_VIDEO_FFMPEG:BOOL=ON` | OFF | 高级视频解码 (当前默认 pl_mpeg) |

## Architecture

```
SDL3 → bgfx (D3D11/Metal/OpenGL) → SoLoud → Lua 5.4

8 个纯虚接口:
  IRenderDevice / IAudioBackend / IPlatformBackend / IAssetProvider /
  IAnimationBackend / ILive2DRenderPath / IMiniGameBackend / IVideoDecoder

61 个 KAG 命令:
  layer(4) + text(7) + audio(13) + system(15) + transition(18) + video(8)
  覆盖文本/音频/图层/过渡/VFX/视频/存档全流程

核心约束:
  所有 Lua 绑定仅通过 BackendRegistry + I*Backend 抽象接口访问 C++
  禁止直接引用具体类 (BgfxRenderDevice/SDL3PlatformBackend)
```

## Directory Structure

```
Caesura(AmeKAG)/
├── src/
│   ├── Core/          Engine, BackendRegistry, InputRouter, DebugManager, ErrorUI, JobSystem
│   ├── Render/        bgfx device, LayerManager, TextRenderer, TextureManager, Particles, Video
│   ├── Audio/         SoLoud backend (BGM/VOICE/SE 三通道)
│   ├── Scripting/     Lua bindings (KAG/Render/VFX/DevCore/Debug/Unified)
│   ├── System/        Save/Load (JSON + version migration)
│   ├── Carc/          AES-256-GCM encrypted resource packs
│   ├── Resource/      Asset pipeline (XP3/CARC/Dir + AsyncLoader)
│   ├── Animation/     Live2D Cubism 5 (CAESURA_HAS_LIVE2D conditional)
│   ├── MiniGame/      3D mini-game interface (reserved)
│   ├── Debug/         HotReload + debug protocol
│   ├── Editor/        JSON-RPC IDE server
│   └── Platform/      Mobile adapter stubs
├── scripts/           43 Lua files (KAG parser/scheduler/commands + game logic)
├── shaders/           bgfx shader sources (dx11/glsl/metal)
├── tests/             24 C++ unit tests
├── external/          10 static libs
└── tools/             CARC pack CLI
```

## Status

| Area | Completion |
|------|-----------|
| Core engine loop | 95% |
| KAG script compatibility | 61 commands |
| D3D11 rendering (Windows) | ✅ Stable, zero TDR |
| Core constraint (abstract interfaces) | ✅ Fully enforced |
| Live2D Cubism 5 | ✅ Windows D3D11 |
| Multi-threaded JobSystem | ✅ AsyncLoader/Particles/CARC/CPU |
| Cross-platform CI | 🔜 Last priority |
| MiniGame 3D | ❌ Interface only |
| Editor (Electron) | ⚠️ Backend ready |

## License

MIT — 详见 [LICENSE](LICENSE)

## Legal

Live2D Cubism SDK 为 Live2D Inc. 的专有软件，**不包含**在本仓库中。
启用 Live2D 支持需用户自行从 [Live2D 官网](https://www.live2d.com/) 下载 Cubism 5 Native SDK。