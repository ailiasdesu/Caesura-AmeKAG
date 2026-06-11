# Caesura (AmeKAG) — 次世代 Visual Novel 引擎

> 跨平台 · AI IDE 辅助 · Live2D · 3D 小游戏 · MIT 开源
> **v1.0-rc** | CI: **Win/Mac/Linux 全通** | 快捷键: **F5 运行** | 审查: **零违规 · 零泄漏**

Caesura 是为视觉小说创作者打造的现代化引擎。KAG 脚本兼容、跨平台渲染、内置 Electron 可视化编辑器、AI 辅助代码生成。

## 快速开始

### 构建引擎

`ash
# Windows (Visual Studio 2022)
cmake -B build_nol2d -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build_nol2d --config Release

# macOS / Linux
cmake -B build -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build
`

### 启动编辑器

`ash
cd web-editor
npm install
npm run dev
`

编辑器自动启动引擎，通过 stdin/stdout JSON-RPC 通信。按 **F5** 运行脚本，**Shift+F5** 停止。

### 运行 Demo

`ash
# Windows
build_nol2d/Release/CaesuraAmeKAG.exe --demo

# macOS / Linux
./build/CaesuraAmeKAG --demo
`

Demo 包含 3 个场景：教室渲染、MiniGame 3D、存档系统展示。

### 第一个 KAG 脚本

`lua
-- hello.cae
KAG.show_text(nil, \"Hello, Visual Novel!\")
KAG.wait_click()
KAG.clear_text()
`

在编辑器中粘贴上述代码，按 F5 运行。

## 架构概览

`
Electron Editor (React + Vite + CodeMirror 6)
         |  JSON-RPC (stdin/stdout, 8 methods)
Engine (C++20 + CMake 3.25+)
  Render(bgfx)  Audio(SoLoud)  Scripting(Lua5.4)  System(Save)
  MiniGame(PBR)  Live2D(Cubism5)  CARC(Crypto)  Debug(HotReload)
`

## 8 个纯虚接口

所有 Lua → C++ 访问必须通过 BackendRegistry + 抽象接口：

| 接口 | 实现 | 状态 |
|------|------|:---:|
| IRenderDevice | BgfxRenderDevice (D3D11/OpenGL/Metal) | ✅ |
| IAudioBackend | SoLoudAudioEngine | ✅ |
| IPlatformBackend | SDL3PlatformBackend | ✅ |
| IAssetProvider | Dir → XP3 → CARC 链 | ✅ |
| IAnimationBackend | Live2DBackend (Cubism 5) | ✅ Win |
| IMiniGameBackend | BgfxMiniGameBackend (PBR-lite) | ✅ |
| IVideoDecoder | PlMpegDecoder / FFmpegDecoder | ✅ |
| ISaveProvider | LocalFileSaveProvider (AES-256-GCM) | ✅ |

## 编辑器功能

| 面板 | 功能 |
|------|------|
| **舞台** | Canvas 2D 预览 + 引擎实时帧 (200ms 轮询) |
| **代码编辑器** | CodeMirror 6 语法高亮、自动补全、错误行标记 |
| **AI 面板** | @generate / @fix 指令，多后端，流式输出 |
| **素材** | 场景列表 + 素材树，拖入舞台生成代码 |
| **日志** | 引擎 stderr 实时转发，错误/警告归类 |
| **RPC 桥接** | ping/run/stop/eval/getFrame/getState/assets/logs |
| **快捷键** | F5 运行 / Shift+F5 停止 / Ctrl+, 设置 / Ctrl+S 存档 |
| **打包** | 一键打包 (Win/Mac/Linux)，electron-builder |

## KAG 脚本兼容

84 个 KAG 命令，9 个子模块：layer / text / audio / resource / save / system / transition / vfx / video。详见 [docs/api/KAG-API.md](docs/api/KAG-API.md)

## MiniGame 3D

15 个 Lua API：spawn_cube/sphere/plane、set_camera、PBR 材质、多光源、碰撞检测、物理。详见 [docs/api/MiniGame-API.md](docs/api/MiniGame-API.md)

## 平台支持

| 平台 | 渲染 | 构建 | CI | Live2D |
|------|------|:---:|:---:|:---:|
| Windows | D3D11 | ✅ | ✅ | ✅ |
| Linux | OpenGL | ✅ | ✅ | ⚠️ 移交 |
| macOS | Metal | ✅ | ✅ | ⚠️ 移交 |

## 技术栈

| 层 | 技术 |
|----|------|
| 引擎 | C++20, CMake 3.25+ |
| 渲染 | bgfx (D3D11/OpenGL/Metal) |
| 窗口 | SDL3 |
| 音频 | SoLoud (3-Bus: BGM/VOICE/SE) |
| 脚本 | Lua 5.4 + 沙箱 |
| 字体 | FreeType + CJK atlas |
| 加密 | BCrypt (Win) / OpenSSL EVP (Unix) |
| 视频 | pl_mpeg + FFmpeg (条件编译) |
| 立绘 | Live2D Cubism 5 (条件编译, SDK 不提交) |
| 编辑器 | Electron 42 + React 18 + Vite 5 + CodeMirror 6 |
| AI | OpenAI API / Codex CLI / 自定义端点 |

## 目录结构

`
Caesura(AmeKAG)/
├── src/               C++ 引擎源码 (65 .cpp + 73 .h, 10 模块)
│   ├── Core/          引擎主循环, BackendRegistry, JobSystem, RpcServer
│   ├── Render/        BgfxRenderDevice, 粒子, 文字, 视频, 跨平台 shader
│   ├── Audio/         SoLoud (BGM/VOICE/SE 3总线)
│   ├── Scripting/     Lua VM, KAGBinding (31), RenderBinding, VFXBinding
│   ├── System/        SaveManager (AES-256-GCM, v1→v5 Schema)
│   ├── CARC/          CryptoEngine, CARC Reader/Writer, DeltaCARC
│   ├── Resource/      AssetManager, AsyncLoader, ProviderChain, XP3
│   ├── Live2D/        Cubism 5 Backend (4 渲染路径, 条件编译)
│   ├── MiniGame/      PBR-lite, 15 Lua API, 跨平台 shader
│   └── Debug/         DebugManager, HotReload, TDR防护
├── scripts/           Lua 脚本 (45 文件)
│   ├── kag/           KAG 模块 (init.lua, backend.lua, sandbox.lua)
│   ├── kag/commands/  命令子模块 (9 个: audio/layer/text/…)
│   └── demo/          Demo 场景 (3 场景)
├── web-editor/        Electron + React 编辑器 (12 组件, F5 运行)
├── tests/             C++ 单元测试 (24 文件, 135/138 通过)
├── docs/              文档 (API, 解决方案, 计划)
└── external/          第三方库 (12 个, 全部静态编译)
`

## 已知限制

| 项目 | 状态 | 说明 |
|------|:---:|------|
| Live2D macOS | ⚠️ | Metal 渲染路径移交 macOS 开发者 |
| Live2D Linux | ⚠️ | OpenGL 渲染路径移交 Linux 开发者 |
| 移动端适配 | 📋 | MobileAdapter 接口完备，待平台构建 |

## 许可证

Caesura (AmeKAG) — Cross-Platform Visual Novel Engine
Copyright (c) 2025-2026 AiliasDesu，MIT License。

第三方库保留各自原始版权：bgfx (BSD-2), SDL3 (zlib), SoLoud (zlib), Lua (MIT), FreeType (FTL), zstd (BSD), nlohmann/json (MIT), ed25519 (CC0), stb (MIT/PD), pl_mpeg (MIT), cpp-httplib (MIT)。Live2D Cubism SDK © Live2D Inc. (专有，用户自行下载)。
