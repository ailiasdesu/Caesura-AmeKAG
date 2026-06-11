---
title: Caesar's Engine Module Architecture v2
type: design
date: 2026-06-11
---

## Design Goals

1. **单模块可修改。** 每个模块只有 `api/` 被外部依赖，实现文件内部可任意重构而不波及其他模块。
2. **剧情流畅度不降。** 脚本引擎通过接口编排 Render + Audio + Save，模块间调用非阻塞优先。
3. **渐进可实施。** 全部改动 = `#include` 路径重写 + `CMakeLists.txt` 调整，零逻辑变化。

## Module Map

```
entry/          — 入口 + 引擎组装 (composition root)
di/             — 依赖注入容器 + 资源配额
platform/       — 窗口/系统抽象
render/         — 渲染管线 (bgfx + 着色器 + 纹理 + 粒子 + 视频)
audio/          — 音频管线 (SoLoud)
script/         — Lua VM + KAG 脚本桥接
resource/       — 资产加载管线
archive/        — 打包/加密格式 (原 CARC)
storage/        — 存档/读档 (原 System)
live2d/         — Live2D 动画
minigame/       — 3D 迷你游戏
steam/          — Steamworks 集成
input/          — 输入路由
debug/          — 开发工具 + 热重载
job/            — 任务调度
rpc/            — 编辑器通信
```

## Dependency Rules

1. `api/` 子目录只依赖其他模块的 `api/`，绝不依赖 `impl/` 或模块根目录。
2. 模块根目录只依赖自己模块的 `api/` + 其他模块的 `api/` + `di/`（获取注入的服务）。
3. `di/BackendRegistry` 是唯一可以依赖全部模块 `api/` 的组件。
4. `entry/` 知道所有模块的 `api/` + `impl/`（组合根模式），但不知道模块内部实现细节。
5. 禁止循环：`A/api/` → `B/api/` → `A/api/` 不成立。

## Directory Tree

```
src/
├── entry/
│   ├── main.cpp                       # 创建所有实例，组装 EngineConfig
│   ├── Engine.cpp                      # 门面：持有各模块接口指针
│   ├── Engine.h
│   ├── EngineConfig.h                  # 聚合结构体，C++20 designated initializer
│   ├── ErrorUI.cpp                     # 错误提示 UI
│   └── ErrorUI.h
│
├── di/
│   ├── api/
│   │   ├── IBackendRegistry.h          # 纯虚接口（未来可 mock）
│   │   ├── ISandboxQuota.h
│   │   └── ITextureBudget.h
│   ├── BackendRegistry.cpp
│   ├── BackendRegistry.h               # 注册表 + 具体实现
│   ├── SandboxQuota.cpp
│   ├── SandboxQuota.h
│   ├── TextureBudget.cpp
│   ├── TextureBudget.h
│   └── thread/
│       └── ThreadAssert.h
│
├── platform/
│   ├── api/
│   │   └── IPlatformBackend.h
│   ├── SDL3PlatformBackend.cpp
│   ├── SDL3PlatformBackend.h
│   ├── MobileAdapter.cpp
│   └── MobileAdapter.h
│
├── render/
│   ├── api/
│   │   ├── IRenderDevice.h             # 核心渲染接口
│   │   ├── IVideoDecoder.h
│   │   ├── IParticleSystem.h
│   │   └── ITextureManager.h
│   ├── bgfx/
│   │   ├── BgfxRenderDevice.cpp        # 门面 + draw 代码
│   │   ├── BgfxRenderDevice.h
│   │   ├── BgfxShaderManager.cpp       # U7 已提取
│   │   ├── BgfxShaderManager.h
│   │   ├── BgfxDeviceCore.cpp          # U7 待提取
│   │   └── BgfxDeviceCore.h
│   ├── shader/
│   │   ├── EmbeddedShaders.cpp
│   │   ├── EmbeddedShaders.h
│   │   ├── EmbeddedShaders_MiniGame.cpp
│   │   ├── EmbeddedShaders_SPIRV.cpp
│   │   ├── ShaderCache.cpp
│   │   └── ShaderCache.h
│   ├── sprite/
│   │   ├── LayerManager.cpp
│   │   ├── LayerManager.h
│   │   ├── TextureManager.cpp
│   │   ├── TextureManager.h
│   │   ├── ParticleSystem.cpp
│   │   └── ParticleSystem.h
│   ├── text/
│   │   ├── TextRenderer.cpp
│   │   ├── TextRenderer.h
│   │   ├── FreeTypeContext.cpp
│   │   └── FreeTypeContext.h
│   └── video/
│       ├── VideoPlayer.cpp
│       ├── VideoPlayer.h
│       ├── GpuMonitor.cpp
│       ├── GpuMonitor.h
│       ├── RTTManager.cpp
│       └── RTTManager.h
│
├── audio/
│   ├── api/
│   │   └── IAudioBackend.h
│   ├── SoLoudAudioEngine.cpp
│   ├── SoLoudAudioEngine.h
│   ├── NullAudioBackend.cpp
│   └── NullAudioBackend.h
│
├── script/
│   ├── api/
│   │   ├── ILuaEngine.h                # 脚本引擎抽象
│   │   └── IKAGContext.h               # KAG 命令上下文
│   ├── vm/
│   │   ├── LuaManager.cpp
│   │   └── LuaManager.h
│   ├── state/
│   │   ├── GameState.cpp
│   │   └── GameState.h
│   └── bindings/                       # Lua ↔ C++ 桥接
│       ├── KAGBinding.cpp
│       ├── KAGBinding.h
│       ├── RenderBinding.cpp
│       ├── RenderBinding.h
│       ├── VFXBinding.cpp
│       ├── VFXBinding.h
│       ├── DebugBinding.cpp
│       ├── DebugBinding.h
│       ├── DevCoreBinding.cpp
│       ├── DevCoreBinding.h
│       ├── SteamBinding.cpp
│       ├── SteamBinding.h
│       ├── UnifiedBinding.cpp
│       └── UnifiedBinding.h
│
├── resource/
│   ├── api/
│   │   ├── IAssetProvider.h
│   │   └── IAsyncLoader.h
│   ├── AssetManager.cpp
│   ├── AssetManager.h
│   ├── AsyncLoader.cpp
│   ├── AsyncLoader.h
│   ├── ImageDecoder.cpp
│   ├── ImageDecoder.h
│   ├── ProviderChain.cpp
│   ├── ProviderChain.h
│   ├── DirAssetProvider.cpp
│   ├── DirAssetProvider.h
│   ├── XP3Archive.cpp
│   └── XP3Archive.h
│
├── archive/
│   ├── api/
│   │   └── ICryptoEngine.h             # 加密接口，CARC 专用
│   ├── CARCFormat.h
│   ├── CARCReader.cpp
│   ├── CARCReader.h
│   ├── CARCWriter.cpp
│   ├── CARCWriter.h
│   ├── CarcAssetProvider.cpp
│   ├── CarcAssetProvider.h
│   ├── CryptoEngine.cpp
│   ├── CryptoEngine.h
│   ├── CRLManager.cpp
│   ├── CRLManager.h
│   ├── DeltaCARC.cpp
│   └── DeltaCARC.h
│
├── storage/
│   ├── api/
│   │   ├── ISaveProvider.h
│   │   └── ISaveStore.h                # 存档存储抽象
│   ├── SaveManager.cpp
│   ├── SaveManager.h
│   ├── SaveBinding.cpp
│   ├── SaveBinding.h
│   ├── CloudSaveProvider.cpp
│   └── CloudSaveProvider.h
│
├── live2d/
│   ├── api/
│   │   └── IAnimationBackend.h
│   ├── cubism/                         # Cubism SDK 封装
│   │   ├── Live2DBackend.cpp
│   │   ├── Live2DBackend.h
│   │   ├── Live2DUserModel.h
│   │   ├── NullAnimationBackend.cpp
│   │   └── NullAnimationBackend.h
│   └── render/                         # Live2D 渲染路径
│       ├── ILive2DRenderPath.h
│       ├── D3D11NativeRenderPath.cpp
│       ├── D3D11NativeRenderPath.h
│       ├── MetalNativeRenderPath.cpp
│       ├── MetalNativeRenderPath.h
│       ├── OpenGLReadbackRenderPath.cpp
│       ├── OpenGLReadbackRenderPath.h
│       ├── OpenGLSharedRenderPath.cpp
│       └── OpenGLSharedRenderPath.h
│
├── minigame/
│   ├── api/
│   │   └── IMiniGameBackend.h
│   ├── BgfxMiniGameBackend.cpp
│   ├── BgfxMiniGameBackend.h
│   ├── NullMiniGameBackend.cpp
│   ├── NullMiniGameBackend.h
│   ├── MiniGeometry.cpp
│   ├── MiniGeometry.h
│   ├── MiniCollision.cpp
│   ├── MiniCollision.h
│   ├── MiniMaterial.h
│   └── MiniLight.h
│
├── steam/
│   ├── api/
│   │   └── ISteamBackend.h
│   ├── SteamBackend.cpp
│   ├── SteamBackend.h
│   └── NullSteamBackend.h
│
├── input/
│   ├── api/
│   │   └── IInputRouter.h
│   ├── InputRouter.cpp
│   └── InputRouter.h
│
├── debug/
│   ├── api/
│   │   └── IDebugManager.h
│   ├── DebugManager.cpp
│   ├── DebugManager.h
│   ├── DebugProtocol.cpp
│   ├── DebugProtocol.h
│   ├── HotReload.cpp
│   ├── HotReload.h
│   ├── BgfxDebugCallback.cpp          # 从 Render 移入
│   └── BgfxDebugCallback.h
│
├── job/
│   ├── api/
│   │   └── IJobSystem.h
│   ├── JobSystem.cpp
│   └── JobSystem.h
│
└── rpc/
    ├── api/
    │   └── IRpcTransport.h             # 传输层抽象 (stdin → WebSocket)
    ├── RpcServer.cpp
    ├── RpcServer.h
    ├── EditorServer.cpp
    └── EditorServer.h
```

## API Contract: Galgame Plot Flow

剧情顺畅度取决于三条调用链，它们只通过接口通信：

```
Script::KAGBinding                  # KAG 命令解释器
    │
    ├─→ Render::IRenderDevice       # show_text / show_sprite / play_transition
    │   └─→ 非阻塞：命令进入渲染队列，帧同步执行
    │
    ├─→ Audio::IAudioBackend        # play_bgm / play_voice / play_se
    │   └─→ isVoiceComplete() 轮询在 Engine 主循环，不在脚本层
    │
    └─→ Storage::ISaveStore         # autosave / save / load
        └─→ AES-256-GCM 加密在 job/ 线程执行，不阻塞主循环
```

**保持顺畅度的核心约束：**

1. `IAudioBackend::isVoiceComplete()` 是主循环轮询（每帧一次），不是脚本层的忙等待。KAG 的 `wait_voice()` 命令注册一个回调，声音结束时触发下一行——不需要脚本线程 `while(playing){}`。
2. `Storage` 的 save/load 操作通过 `job/JobSystem` 异步执行。存档加密不阻塞渲染帧。
3. `Live2D` 的 `IAnimationBackend::update(deltaTime)` 在每帧渲染前调用，与 bgfx frame 同步，不在独立线程。避免帧撕裂。

## 变更清单（pr 1: 目录重组）

| 原路径 | 新路径 | 文件数 |
|--------|--------|--------|
| `src/Core/BackendRegistry.*` | `src/di/BackendRegistry.*` | 2 |
| `src/Core/SandboxQuota.*` | `src/di/SandboxQuota.*` | 2 |
| `src/Core/TextureBudget.*` | `src/di/TextureBudget.*` | 2 |
| `src/Core/ThreadAssert.h` | `src/di/thread/ThreadAssert.h` | 1 |
| `src/Core/IPlatformBackend.h` | `src/platform/api/IPlatformBackend.h` | 1 |
| `src/Core/SDL3PlatformBackend.*` | `src/platform/SDL3PlatformBackend.*` | 2 |
| `src/Core/MobileAdapter.*` | `src/platform/MobileAdapter.*` | 2 |
| `src/Core/IAudioBackend.h` | `src/audio/api/IAudioBackend.h` | 1 |
| `src/Core/InputRouter.*` | `src/input/InputRouter.*` | 2 |
| `src/Core/Engine.*` | `src/entry/Engine.*` | 2 |
| `src/Core/EngineConfig.h` | `src/entry/EngineConfig.h` | 1 |
| `src/Core/ErrorUI.*` | `src/entry/ErrorUI.*` | 2 |
| `src/Core/RpcServer.*` | `src/rpc/RpcServer.*` | 2 |
| `src/Core/EditorServer.*` | `src/rpc/EditorServer.*` | 2 |
| `src/Core/JobSystem.*` | `src/job/JobSystem.*` | 2 |
| `src/Scripting/*` | `src/script/*` | 18 |
| `src/System/*` | `src/storage/*` | 4 |
| `src/CARC/*` → `src/archive/*` | 全部文件 | 10 |
| `src/live2d/*` | `src/live2d/*` | 7 |
| `src/minigame/*` | `src/minigame/*` | 8 |
| `src/debug/BgfxDebugCallback.*` | 移入 `src/debug/` | 2 |
| 总计 | 78 文件移动 |

**不在此范围：** 函数重命名、接口重构、逻辑修改。本 PR 纯移动 + `#include` 路径修正。

## 验证

- 移动后 `cmake --build` 编译通过
- `python scripts/count_coupling.py --ci` PASS
- 剧情 Demo（3 场景）正常运行：text → voice → transition → save → load → minigame
