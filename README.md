# Caesura (AmeKAG) — 次世代 Visual Novel 引擎

> 跨平台 · AI IDE 辅助 · Live2D · 3D 小游戏 · MIT 开源  
> 完成度: **~95% Beta+** | CI: **Win/Mac/Linux 全通 (零错误)** | 审查: **零违规 · 零泄漏**

Caesura 是为视觉小说创作者打造的现代化引擎。KAG 脚本兼容、跨平台渲染、内置 Electron 可视化编辑器、AI 辅助代码生成。

## 快速开始

### 构建引擎

```bash
# Windows (Visual Studio 2022)
cmake -B build_nol2d -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build_nol2d --config Release

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
# Windows
build_nol2d/Release/CaesuraAmeKAG.exe --demo

# macOS / Linux
./build/CaesuraAmeKAG --demo
```

Demo 包含 3 个场景：教室渲染、MiniGame 3D、存档系统展示。

### 第一个 KAG 脚本

```lua
-- hello.cae
KAG.show_text(nil, "Hello, Visual Novel!")
KAG.wait_click()
KAG.clear_text()
```

在编辑器中粘贴上述代码，按 F5 运行。

## 架构概览

```
┌─────────────────────────────────────────────────────────┐
│  Electron Editor (React + Vite)                         │
│  Stage View │ Scene List │ AI Panel │ Code Editor      │
└────────────┬────────────────────────────────────────────┘
             │ JSON-RPC (stdin/stdout, 8 methods: ping/run/stop/eval/getFrame/getState/assets/logs)
┌────────────▼────────────────────────────────────────────┐
│  Engine (C++20)                                         │
│  ┌──────────┐  ┌────────┐  ┌──────────┐  ┌───────────┐ │
│  │ Engine   │  │ Core   │  │ Scripting │  │ System    │ │
│  │ 主循环    │  │ Backend│  │ Lua 5.4  │  │ 存档+加密  │ │
│  │ 事件/渲染 │  │ Registry│  │ 61 KAG   │  │ AES-GCM   │ │
│  └──────────┘  └────────┘  └──────────┘  └───────────┘ │
│  ┌──────────┐  ┌────────┐  ┌──────────┐  ┌───────────┐ │
│  │ Render   │  │ Audio  │  │ Resource │  │ MiniGame  │ │
│  │ bgfx/D3D │  │ SoLoud │  │ AsyncLoad│  │ PBR-lite  │ │
│  │ Particle │  │ 3-Bus  │  │ CARC/xp3 │  │ 15 Lua API│ │
│  └──────────┘  └────────┘  └──────────┘  └───────────┘ │
│  ┌──────────┐  ┌────────┐  ┌──────────┐               │
│  │ Live2D   │  │ CARC   │  │ Debug    │               │
│  │ Cubism 5 │  │ ΔCARC  │  │ HotReload│               │
│  │ 4 路径   │  │ Crypto │  │ TDR防    │               │
│  └──────────┘  └────────┘  └──────────┘               │
└─────────────────────────────────────────────────────────┘
```

## 8 个纯虚接口

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
| **舞台** | Canvas 2D 预览 + 引擎实时帧 (RPC getFrame, 500ms 轮询) |
| **场景列表** | 场景拖拽排序、点击跳转、模板选择 |
| **代码编辑器** | 语法高亮、自动补全、错误行标记、文件标签栏 |
| **属性** | 选中→编辑参数→实时生成 KAG Lua 代码 |
| **AI 面板** | @generate / @fix 指令，多后端（OpenAI/Codex/Custom），流式输出 |
| **素材** | 场景列表 + 素材树上下分区，拖入舞台生成代码 |
| **日志** | ✅ 引擎实时 stderr 流，错误/警告自动归类，连接状态指示器 |
| **RPC 桥接** | ✅ 8 方法全通 (ping/run/stop/eval/getFrame/getState/assets/logs) |
| **实时帧预览** | ✅ StageView 500ms 轮询引擎渲染帧 (base64 PNG) |
| **实时日志** | ✅ 引擎 stderr 自动转发 LogPanel，错误/警告归类 |
| **打包** | 一键打包 (Win/Mac/Linux)，electron-builder NSIS/DMG/AppImage |

## KAG 脚本兼容

61 个 KAG 命令，7 个子模块：layer / text / audio / system / transition / vfx / video。
详见 [docs/api/KAG-API.md](docs/api/KAG-API.md)

## MiniGame 3D

15 个 Lua API：spawn_cube/sphere/plane、set_camera、材质、光照、碰撞检测、物理。
详见 [docs/api/MiniGame-API.md](docs/api/MiniGame-API.md)

## 多线程

```
主线剧情（单线程）: processEvents → update → render
辅助多线程（JobSystem）: AsyncLoader | ParticleSim | CARC | CPU Tasks | Video
```

## 平台支持

| 平台 | 渲染 | 构建 | Live2D |
|------|------|:---:|:---:|
| Windows | D3D11 | ✅ | ✅ |
| Linux | OpenGL | ✅ | ⚠️ 移交 |
| macOS | Metal | ✅ | ⚠️ 移交 |

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
| AI | OpenAI API / Codex CLI / 自定义端点 |

## 目录结构

```
Caesura(AmeKAG)/
├── src/               C++ 引擎源码 (64 .cpp + 73 .h, 10 模块)
│   ├── Core/          Engine, BackendRegistry, InputRouter, JobSystem, RpcServer
│   ├── Render/        BgfxRenderDevice, LayerManager, ParticleSystem
│   ├── Audio/         SoLoudAudioEngine
│   ├── Scripting/     LuaManager, KAGBinding, 6 绑定模块
│   ├── System/        SaveManager (JSON + AES-256-GCM)
│   ├── CARC/          CryptoEngine, CARC Reader/Writer, DeltaCARC
│   ├── Resource/      AssetManager, AsyncLoader, ProviderChain
│   ├── Live2D/        Cubism 5 Backend (4 渲染路径, 条件编译)
│   ├── MiniGame/      BgfxMiniGameBackend (PBR-lite)
│   └── Debug/         HotReload, DebugProtocol, DebugManager
├── scripts/           Lua 脚本 (44 文件)
│   ├── kag/           KAG 命令处理器 + 子模块
│   └── sandbox.lua    安全沙箱规则
├── tests/             C++ 单元测试 (24 文件)
├── web-editor/        Electron 可视化编辑器
│   ├── src/           React 组件 (11 组件 + 3 AI provider)
│   ├── electron/      Electron 主进程 + IPC
├── docs/              文档
│   ├── api/           API 参考
│   ├── strategy/      战略分析
│   ├── plans/         实施计划
│   └── solutions/     知识库
├── external/          第三方库 (bgfx, SDL3, Lua, SoLoud, ...)
├── assets/            游戏素材 (bg, char, bgm, voice, se)
└── shaders/           着色器源码
```


## 第三方库许可

本引擎静态/动态链接以下第三方库。所有库的原始许可证文本保留在各自目录中。

### C++ 引擎依赖

| 库 | 版本 | 许可证 | 用途 | 上游 |
|----|------|--------|------|------|
| **bgfx** | 主线 | BSD 2-Clause | 跨平台渲染 (D3D11/OpenGL/Metal) | [bkaradzic/bgfx](https://github.com/bkaradzic/bgfx) |
| **bx** | 主线 | BSD 2-Clause | bgfx 基础设施 | [bkaradzic/bgfx](https://github.com/bkaradzic/bgfx) |
| **bimg** | 主线 | BSD 2-Clause | 图像加载/压缩 | [bkaradzic/bgfx](https://github.com/bkaradzic/bgfx) |
| **SDL3** | 3.2.0 | zlib | 窗口/输入/平台抽象 | [libsdl-org/SDL](https://github.com/libsdl-org/SDL) |
| **SoLoud** | 主线 | zlib/libpng | 音频引擎 (WASAPI/WinMM) | [jarikomppa/soloud](https://github.com/jarikomppa/soloud) |
| **Lua** | 5.4 | MIT | 脚本运行时 | [lua/lua](https://github.com/lua/lua) |
| **FreeType** | 主线 | FreeType License (BSD-like) | CJK 字体渲染 | [freetype/freetype](https://github.com/freetype/freetype) |
| **zstd** | 主线 | BSD + GPLv2 双许可 | CARC 压缩 | [facebook/zstd](https://github.com/facebook/zstd) |
| **pl_mpeg** | 主线 | MIT | MPEG-1 视频解码 (零依赖回退) | [phoboslab/pl_mpeg](https://github.com/phoboslab/pl_mpeg) |
| **nlohmann/json** | 主线 | MIT | JSON 解析 (存档/CARC 索引) | [nlohmann/json](https://github.com/nlohmann/json) |
| **ed25519** | 主线 | 公共领域 / CC0 | CARC 数字签名 | [orlp/ed25519](https://github.com/orlp/ed25519) |
| **stb** | 主线 | 公共领域 / MIT | 图像加载 (stb_image) | [nothings/stb](https://github.com/nothings/stb) |
| **cpp-httplib** | 主线 | MIT | HTTP 服务 (RPC Server / 热重载) | [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib) |

### 条件编译依赖

| 库 | 版本 | 许可证 | 条件 | 用途 |
|----|------|--------|------|------|
| **Live2D Cubism 5 Native SDK** | 5-r.5 | 专有 (Live2D 社) | `CAESURA_ENABLE_LIVE2D=ON` | 2D 动态立绘 |
| **FFmpeg** | 系统 | LGPL 2.1+ | `CAESURA_ENABLE_FFMPEG=ON` | 硬件加速视频解码 |
| **GLEW** | 主线 | BSD-like | `CAESURA_ENABLE_LIVE2D=ON` (Win) | OpenGL 扩展加载 |

### 平台加密后端

| 平台 | 库 | 许可证 |
|------|----|--------|
| Windows | BCrypt (系统内置) | Microsoft Windows SDK |
| macOS / Linux | OpenSSL EVP | Apache 2.0 |

### Electron 编辑器依赖

| 库 | 版本 | 许可证 | 用途 |
|----|------|--------|------|
| **Electron** | 42.x | MIT | 桌面壳 / Node.js 运行时 |
| **React** | 18.x | MIT | UI 框架 |
| **React DOM** | 18.x | MIT | DOM 渲染 |
| **Vite** | 5.x | MIT | 构建工具 / HMR |
| **CodeMirror 6** | 6.x | MIT | 代码编辑器 (语法高亮/补全) |
| **@codemirror/view** | 6.x | MIT | 编辑器视口 |
| **@codemirror/state** | 6.x | MIT | 编辑器状态模型 |
| **@codemirror/language** | 6.x | MIT | 语言支持 (Lua) |
| **@codemirror/autocomplete** | 6.x | MIT | 自动补全 (KAG 命令) |
| **@codemirror/theme-one-dark** | 6.x | MIT | One Dark 主题 |
| **@codemirror/legacy-modes** | 6.x | MIT | Lua 语法模式 |
| **@lezer/highlight** | 1.x | MIT | 语法树高亮 |
| **@vitejs/plugin-react** | 4.x | MIT | Vite React 插件 |
| **concurrently** | 10.x | MIT | 并发启动 Vite + Electron |
| **electron-builder** | 26.x | MIT | 打包分发 (NSIS/DMG/AppImage) |
| **wait-on** | 9.x | MIT | 等待 Vite 就绪后启动 Electron |

## 已知限制

| 项目 | 状态 | 说明 |
|------|:---:|------|
| Live2D macOS | ⚠️ | Metal 渲染路径移交 macOS 开发者 |
| Live2D Linux | ⚠️ | OpenGL 渲染路径移交 Linux 开发者 |
| 移动端适配 | 📋 | MobileAdapter 存根 (P2 预留) |
| 缩略图截图 | ⚠️ | 磁盘 I/O 模式，存档场景可接受 |

## 版权声明

```
Caesura (AmeKAG) — Cross-Platform Visual Novel Engine
Copyright (c) 2025-2026 AiliasDesu

本引擎以 MIT 许可证发布，详见 LICENSE 文件。

包含的第三方库保留其原始版权和许可证：
- bgfx/bx/bimg — Copyright (c) 2012-2025 Branimir Karadžić
- SDL3 — Copyright (c) 1997-2025 Sam Lantinga
- SoLoud — Copyright (c) 2013-2025 Jari Komppa
- Lua — Copyright (c) 1994-2025 Lua.org, PUC-Rio
- FreeType — Copyright (c) 1996-2025 David Turner, Robert Wilhelm, Werner Lemberg
- Facebook zstd — Copyright (c) 2016-2025 Meta Platforms, Inc.
- pl_mpeg — Copyright (c) 2019-2025 Dominic Szablewski
- nlohmann/json — Copyright (c) 2013-2025 Niels Lohmann
- ed25519 — Copyright (c) 2015 Orson Peters
- stb — Copyright (c) 2017 Sean Barrett
- cpp-httplib — Copyright (c) 2017-2025 Yuji Hirose
- Live2D Cubism SDK — Copyright (c) Live2D Inc. (专有许可)
- CodeMirror — Copyright (c) 2018-2025 Marijn Haverbeke
- React — Copyright (c) Meta Platforms, Inc.
- Electron — Copyright (c) GitHub, Inc.
- Vite — Copyright (c) 2019-2025 Evan You

保留所有权利。
```

> **免责声明**: 本文件中的许可证信息为摘要。各第三方库的完整许可证文本
> 位于 `external/<库名>/LICENSE*` 或相关 npm 包的 `node_modules/<包名>/LICENSE*`。
> Live2D Cubism SDK 为 Live2D 社的专有软件，不包含在引擎源代码中，
> 用户需自行从 [live2d.com](https://www.live2d.com/) 获取并遵守其最终用户许可协议。