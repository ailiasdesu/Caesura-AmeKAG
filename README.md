# Caesura (AmeKAG) — 次世代 Visual Novel 引擎

> 跨平台 · AI IDE 辅助 · Live2D · 3D 小游戏 · MIT 开源

Caesura 是为视觉小说创作者打造的现代化引擎。KAG 脚本兼容、跨平台渲染、内置 Electron 可视化编辑器、AI 辅助代码生成。

## 快速开始

### 构建引擎

```bash
# Windows (Visual Studio 2022)
cmake -B build_nol2d -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build_nol2d --config Debug

# macOS / Linux
cmake -B build -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build
```

### 启动可视化编辑器

```bash
cd web-editor
npm install
npm run dev
```

编辑器自动启动引擎（headless 模式），通过 stdin/stdout JSON-RPC 通信。

### 运行 Demo

```bash
build_nol2d/Debug/CaesuraAmeKAG.exe --demo
```

## 架构概览

```
┌─────────────────────────────────────────────────────────┐
│  Electron Editor (React + Vite)                         │
│  Stage View │ Timeline │ AI Panel │ Code Editor        │
└────────────┬────────────────────────────────────────────┘
             │ JSON-RPC (stdin/stdout)
┌────────────▼────────────────────────────────────────────┐
│  Engine (C++20)                                         │
│  ┌──────────┐  ┌────────┐  ┌──────────┐  ┌───────────┐ │
│  │ Engine   │  │ Core   │  │ Scripting │  │ System    │ │
│  │ 主循环    │  │ Backend│  │ Lua 5.4  │  │ SaveManager│ │
│  │ 事件/渲染 │  │ Registry│  │ 61 KAG   │  │ AES-GCM   │ │
│  └──────────┘  └────────┘  └──────────┘  └───────────┘ │
│  ┌──────────┐  ┌────────┐  ┌──────────┐  ┌───────────┐ │
│  │ Render   │  │ Audio  │  │ Resource │  │ MiniGame  │ │
│  │ bgfx/D3D │  │ SoLoud │  │ AsyncLoad│  │ PBR-lite  │ │
│  │ Particle │  │ 3-Bus  │  │ CARC/xp3 │  │ 15 Lua API│ │
│  └──────────┘  └────────┘  └──────────┘  └───────────┘ │
│  ┌──────────┐  ┌────────┐  ┌──────────┐               │
│  │ Animation│  │ Carc   │  │ Editor   │               │
│  │ Live2D 5 │  │ ΔCARC  │  │ JSON-RPC │               │
│  │ 4 路径   │  │ Crypto │  │ Server   │               │
│  └──────────┘  └────────┘  └──────────┘               │
└─────────────────────────────────────────────────────────┘
```

## 8 个纯虚接口（核心约束）

所有 Lua → C++ 访问必须通过 BackendRegistry + 抽象接口：

| 接口 | 实现 | 状态 |
|------|------|:---:|
| `IRenderDevice` | BgfxRenderDevice (D3D11/OpenGL/Metal) | ✅ |
| `IAudioBackend` | SoLoudAudioEngine | ✅ |
| `IPlatformBackend` | SDL3PlatformBackend | ✅ |
| `IAssetProvider` | Dir → XP3 → CARC 链 | ✅ |
| `IAnimationBackend` | Live2DBackend (Cubism 5) | ✅ Win |
| `ILive2DRenderPath` | D3D11Native / OpenGLShared / Metal / Readback | ⚠️ 移交 |
| `IMiniGameBackend` | BgfxMiniGameBackend (PBR-lite) | ✅ |
| `IVideoDecoder` | PlMpegDecoder / FFmpegDecoder | ✅ |

## 编辑器功能

| 面板 | 功能 |
|------|------|
| **舞台** | Canvas 2D 预览，拖入素材定位，网格吸附，分辨率切换 |
| **时间线** | 事件块可视化（图/文/音/特效），累计时间轴 |
| **属性** | 选中→编辑参数→实时生成 KAG Lua 代码 |
| **AI 面板** | @generate / @fix 指令，多后端（OpenAI/Codex/Custom） |
| **素材** | 按类型分组浏览，拖拽到舞台 |
| **日志** | 引擎实时日志，错误高亮 |

## KAG 脚本兼容

61 个 KAG 命令，6 个子模块：layer / text / audio / system / transition / video。
详见 [docs/api/KAG-API.md](docs/api/KAG-API.md)

## MiniGame 3D

15 个 Lua API：spawn_cube/sphere/plane、set_camera、材质、光照、碰撞检测、物理。
详见 [docs/strategy/SPRINT-PLAN.md](docs/strategy/SPRINT-PLAN.md)

## 多线程

```
主线剧情（单线程）: processEvents → update → render
辅助多线程（JobSystem）: AsyncLoader | ParticleSim | CARC | CPU Tasks | Video
```

## 平台支持

| 平台 | 渲染 | 构建 | Live2D |
|------|------|:---:|:---:|
| Windows | D3D11 | ✅ | ✅ |
| Linux | OpenGL | ⚠️ CI | ⚠️ 移交 |
| macOS | Metal | ⚠️ CI | ⚠️ 移交 |

## 技术栈

| 层 | 技术 |
|----|------|
| 引擎 | C++20, CMake 3.25+ |
| 渲染 | bgfx (D3D11/OpenGL/Metal) |
| 窗口 | SDL3 |
| 音频 | SoLoud |
| 脚本 | Lua 5.4 |
| 字体 | FreeType + CJK atlas |
| 加密 | BCrypt (Win) / OpenSSL EVP (Unix) |
| 视频 | pl_mpeg + FFmpeg (条件编译) |
| 立绘 | Live2D Cubism 5 (条件编译) |
| 编辑器 | Electron 42 + React 18 + Vite 5 |
| 打包 | electron-builder (NSIS/DMG/AppImage) |
| AI | OpenAI API / Codex / 自定义端点 |

## 目录结构

```
Caesura(AmeKAG)/
├── src/               C++ 引擎源码 (63 .cpp + 73 .h)
│   ├── Core/          Engine, BackendRegistry, InputRouter, JobSystem
│   ├── Render/        BgfxRenderDevice, LayerManager, ParticleSystem
│   ├── Audio/         SoLoudAudioEngine
│   ├── Scripting/     LuaManager, KAGBinding, 6 绑定模块
│   ├── System/        SaveManager (JSON + AES-256-GCM)
│   ├── Carc/          CryptoEngine, CARC Reader/Writer, DeltaCARC
│   ├── Resource/      AssetManager, AsyncLoader, XP3/CARC Provider
│   ├── Animation/     Live2D Backend (4 渲染路径)
│   ├── MiniGame/      BgfxMiniGameBackend (PBR-lite)
│   ├── Editor/        EditorServer (RPC)
│   ├── Debug/         HotReload, DebugProtocol
│   └── Platform/      MobileAdapter (存根)
├── scripts/           Lua 脚本 (44 文件)
│   ├── kag/           KAG 命令处理器 + 子模块
│   ├── sandbox.lua    安全沙箱规则
│   └── ...
├── tests/             C++ 单元测试 (24 文件)
├── web-editor/        Electron 可视化编辑器
│   ├── src/           React 组件
│   ├── electron/      Electron 主进程 + IPC
│   └── ...
├── docs/              文档
│   ├── api/           KAG API 参考
│   ├── strategy/      战略分析
│   ├── plans/         实施计划
│   └── solutions/     知识库
├── external/          第三方库 (bgfx, SDL3, Lua, SoLoud, ...)
└── assets/            游戏素材 (bg, char, bgm, voice, se)
```

## 许可

MIT License — 详见 [LICENSE](LICENSE)

Live2D Cubism SDK 为单独授权，需从 [Live2D 官网](https://www.live2d.com/) 获取。