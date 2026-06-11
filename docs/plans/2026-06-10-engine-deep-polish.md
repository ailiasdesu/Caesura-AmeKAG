---
title: 引擎深度完善 — RPC 帧预览 + MiniGame Shader + 技术债清零
type: feature
status: planned
date: 2026-06-10
origin: FFmpeg 默认启用完成 → 引擎 ~94% → 打磨至 ~98% v1.0-rc 就绪
prerequisites: FFmpeg 默认启用 (✅), CMake 构建正常 (✅), 编辑器 RPC 桥接 (✅)
---

# 引擎深度完善 — RPC 帧预览 + MiniGame Shader + 技术债清零

## Summary

引擎核心 ~94%。本计划打磨剩余 4%：
1. **RPC 帧预览**：bgfx 帧回读 → base64 PNG → 编辑器实时预览
2. **MiniGame Shader 预编译**：GLSL/Metal 源码嵌入改为 shaderc 预编译二进制
3. **技术债清零**：缩略图截图、ISaveProvider 文件列表、BgfxMiniGameBackend 链接、Live2D 纹理 TODO

## Implementation Units

### IU-1: RPC 帧预览 — bgfx 帧回读 + base64 PNG 编码

**当前状态**：RpcServer::handleGetFrame 已实现，setFrameCaptureCallback 接口就绪，但 Engine.cpp 从未设置回调。

**实施内容**：
- Engine.cpp 中实现 `requestFrameCapture(w, h)` — 触发 bgfx 帧回读 → 编码 PNG → base64
- 设置 `RpcServer::instance().setFrameCaptureCallback(...)` 
- 回读方式：bgfx::requestScreenShot 写入内存 buffer（非文件），或用 `bgfx::readTexture` 直接读 backbuffer
- 编码：stb_image_write 已就绪，用于 PNG 编码
- 编辑器侧：StageView 轮询 getFrame (500ms 间隔) → 显示 `<img>` base64 帧

**关键文件**：`src/Core/Engine.cpp`, `src/Core/RpcServer.cpp`, `web-editor/src/components/StageView.jsx`

### IU-2: MiniGame Shader 预编译 — GLSL/Metal 嵌入式二进制

**当前状态**：DXBC 预编译 ✅，GLSL 和 Metal 源码嵌入（运行时编译）。shaderc 脚本已就绪 (`shaders/compile_shaders.sh/.bat`)。

**实施内容**：
- 用 shaderc 将 GLSL `.sc` 编译为 SPIR-V → GLSL 运行时用 bgfx 内置 cross-compile
- 将 Metal `.metal` 编译为 `.metallib` 二进制
- 更新 `EmbeddedShaders_MiniGame_GLSL.cpp` / `_Metal.cpp` 嵌入编译后二进制
- 验证：三平台 shader 均可正常加载

**关键文件**：`src/Render/EmbeddedShaders_MiniGame_GLSL.cpp`, `_Metal.cpp`, `shaders/compile_shaders.bat`

### IU-3: SaveManager 缩略图截图 (SU-4)

**当前状态**：`SaveManager::captureThumbnailPNG()` 为存根（返回空字符串）。

**实施内容**：
- 调用 bgfx 帧回读（复用 IU-1 的回读机制）
- 缩放至 320×180 缩略图尺寸
- 编码为 PNG base64
- 写入存档 envelope["thumbnail"]

**关键文件**：`src/System/SaveManager.cpp`, `src/System/SaveManager.h`

### IU-4: ISaveProvider 文件列表存根

**当前状态**：`ISaveProvider::listSaveFiles()` 返回空列表。

**实施内容**：
- 遍历 `saves/` 目录
- 解析每个 `.sav` 文件的 envelope（跳过 body）
- 返回 `vector<SaveSlotInfo>` 含 slot_id / timestamp / scene_label / token_index / thumbnail

**关键文件**：`src/System/ISaveProvider.cpp`, `src/System/SaveManager.cpp`

### IU-5: BgfxMiniGameBackend 测试链接 (TD-23)

**当前状态**：测试 target `CaesuraTests` 链接 `BgfxMiniGameBackend` 但 MiniGame 模块编译为引擎内部源（非独立库），测试需链接整个引擎或 mock。

**实施内容**：
- 将 MiniGame 后端的公共接口编译为独立 OBJECT library `minigame_core`
- 测试链接 `minigame_core` + mock bgfx
- 或：在 `tests/cpp/` 中添加 MiniGame API 的 Lua 脚本测试（不依赖 C++ 链接）

**关键文件**：`CMakeLists.txt`, `tests/cpp/test_minigame.cpp`

### IU-6: Live2D 纹理加载 TODO

**当前状态**：`Live2DBackend.cpp:227` — "TODO: Load texture into Cubism renderer"。

**实施内容**：
- 从 AssetManager 加载纹理（通过 `IAssetProvider`）
- 调用 Cubism SDK `csmLoadTexture` 注册到 Cubism 渲染器
- 错误处理：纹理加载失败时使用占位纹理

**关键文件**：`src/Live2D/Live2D/Live2DBackend.cpp`

## Key Decisions

### KD1: 帧回读方式
用 `bgfx::requestScreenShot(BGFX_INVALID_HANDLE, memoryBuffer)` 写入内存，避免文件 I/O。备选：`bgfx::readTexture(backbuffer)` 直接读纹理。

### KD2: Shader 预编译时机
GLSL 预编译为 SPIR-V 嵌入（bgfx 运行时跨编译到目标平台）。Metal 预编译为 metallib 嵌入。shaderc 已就绪。

### KD3: 缩略图与帧预览共享回读
IU-1 和 IU-3 共享同一个 bgfx 帧回读基础设施。缩略图在存档时触发（按需），帧预览周期性触发（500ms）。

## Scope Boundaries

- **不做**：Live2D OpenGLShared/Metal（移交共同开发者）
- **不做**：移动端适配（需真机）
- **不做**：编辑器 UI 更改（编辑器功能正常，仅打通 RPC 帧预览）
- **不做**：跨平台 CI 测试启用（非 GPU 环境）

## Test Plan

- IU-1：启动引擎 `--editor`，编辑器 ping → getFrame → 显示帧图像
- IU-2：Windows 构建 → MiniGame spawning → 3D 物体渲染正常
- IU-3：游戏中按 F5 快存 → 检查 `.sav` 文件含 thumbnail 字段
- IU-4：`listSaveFiles()` 返回 8 档位信息
- IU-5：`cmake --build build_nol2d --target CaesuraTests` → 全部通过
- IU-6：Live2D 模型加载 → 纹理显示正常
