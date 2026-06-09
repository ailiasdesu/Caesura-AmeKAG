# Caesura (AmeKAG) 引擎代码库深度分析

> 分析日期: 2026-06-09 | 构建配置: CMake 3.25+ / C++20
> 构建状态: Windows Debug + Release 全量通过，D3D11 零 TDR 稳定
> 前次分析后修复: TD-01~TD-06, TD-08~TD-09, TD-16, TD-18 全部闭合


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 1. 项目规模

| 维度 | 数值 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| C++ 源文件 | 57 .cpp + 66 .h = 123 个编译单元 |
| C++ 代码量 | 12,832 行 (.cpp) + 3,446 行 (.h) |
| Lua 脚本 | 43 个文件 / 8,410 行 |
| C++ 单元测试 | 24 个 test_*.cpp / 1,298 行 |
| Lua 集成测试 | 10 个脚本 |
| KAG 命令 | 61 个注册 handler + 10 个 legacy 别名 |
| 接口抽象 | 7 个纯虚接口 (IRenderDevice/IAudioBackend/IPlatformBackend/IAssetProvider/IAnimationBackend/ILive2DRenderPath/IMiniGameBackend) |
| 外部库 | 10 个全部静态编译 (bgfx/bimg/bx/SoLoud/Lua/FreeType/zstd/ed25519/pl_mpeg/cpp-httplib) |
| Shader | 10 DXBC 编译产物 + 11 GLSL + 10 Metal 源码 |


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 2. 架构总览

```
src/
├── Core/          ← 引擎中枢: Engine, BackendRegistry, InputRouter, DebugManager, ErrorUI
│                   JobSystem, SandboxQuota, TextureBudget, RpcServer
├── Render/        ← 渲染管线: BgfxRenderDevice, LayerManager, TextRenderer, TextureManager
│                   RTTManager, ShaderCache, ParticleSystem, GpuMonitor, VideoPlayer
├── Audio/         ← SoLoudAudioEngine + NullAudioBackend
├── Scripting/     ← Lua 绑定层: LuaManager, KAGBinding, RenderBinding, VFXBinding,
│                   DevCoreBinding, DebugBinding, UnifiedBinding, GameState
├── System/        ← SaveManager (JSON 保存/加载 + 版本迁移)
├── Carc/          ← 加密资源包: CryptoEngine (BCrypt), CARCReader/Writer, CRLManager
├── Resource/      ← AssetManager, AsyncLoader, ProviderChain, XP3Archive, ImageDecoder
├── Debug/         ← HotReload + DebugProtocol (Lua 断点调试)
├── Editor/        ← EditorServer (Electron IDE RPC 服务端)
├── MiniGame/      ← IMiniGameBackend + NullMiniGameBackend (3D 小游戏预留接口)
├── Animation/     ← IAnimationBackend + Live2D Backend (Cubism 5, 条件编译)
└── Platform/      ← MobileAdapter (移动端生命周期存根)
```

**初始化链**: SDL3 → bgfx → SoLoud → Lua → SaveManager → HotReload
**主循环**: processEvents → Lua engine_update → render (engine_render → bgfx draw → MiniGame hook) → 音频检测
**关闭链**: 逆序


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 3. 模块详细分析

### 3.1 Core (`src/Core/`)

| 文件 | 行 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `Engine.h/.cpp` | ~600 | 顶层引擎：初始化顺序链、主循环 (processEvents→Lua update→render→audio 检测)、关闭逆序。所有子系统 unique_ptr 所有权，通过 BackendRegistry 暴露引用。Lua 256MB 内存上限 + 自定义 alloc hook。 |
| `BackendRegistry.h/.cpp` | ~400 | 后端注册表单例：IRenderDevice/IAudioBackend/IPlatformBackend 的 getter/setter + 工厂查找。ResourceHandle 代际追踪。TM-03 已修复：工厂方法不再 new，Engine 统一 unique_ptr 所有。 |
| `DebugManager.h/.cpp` | ~440 | 诊断单例：6 级日志 (Trace→Fatal) + 环形缓冲区 (1024 条) + 7 子系统错误计数 + 帧分析 + 子系统状态快照。TD-04 已修复：`std::mutex` 替代 `recursive_mutex`。 |
| `ErrorUI.h/.cpp` | ~250 | 双级错误屏幕：Level1 (bgfx 存活: 深红画布 + 白色调试文字) / Level2 (SDL MessageBox 回退)。Retry/Title/Quit 三按钮 + 智能重试。 |
| `JobSystem.h/.cpp` | ~150 | 线程池 (std::thread + 无锁队列)：粒子模拟分片 + 纹理加载 + CARC 解密 + CPU 通用任务。 |
| `SandboxQuota.h/.cpp` | ~80 | Lua 沙箱配额管理：指令预算 + 内存上限检测。 |
| `RpcServer.h/.cpp` | ~120 | IDE 通信：JSON-RPC over TCP，EditorServer 的底层通道。 |
| `SDL3PlatformBackend.h/.cpp` | ~200 | SDL3 平台后端：窗口创建、事件轮询、全屏切换、原生句柄暴露。 |
| `TextureBudget.h/.cpp` | ~150 | 6 级纹理内存预算：128MB→4GB，自动检测系统 RAM 分档。 |

### 3.2 Render (`src/Render/`)

| 文件 | 行 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `BgfxRenderDevice.h/.cpp` | ~1000 | bgfx 渲染后端：init/setup views/shutdown 生命周期 + blitTexture/stretchBlt/affineBlt + 批量提交协议 + debug overlay。D3D11 为主，支持 multi-renderer 选择。 |
| `LayerManager.h/.cpp` | ~400 | 图层管理器：bg/fg/message/overlay 4 层 + Z 排序 + 批处理集合 + RTT 视图映射。 |
| `TextRenderer.h/.cpp` | ~900 | 统一文本渲染器（已吸收 FontRenderer）：位图字体 + TTF/CJK FreeType 渲染 + 批缓存 + Ruby 注音 + cursor 管理。 |
| `TextureManager.h/.cpp` | ~350 | 纹理生命周期管理：引用计数 + LRU 驱逐 + 空纹理占位。 |
| `RTTManager.h/.cpp` | ~300 | 离屏渲染目标管理器：创建/销毁/复用 ViewportHandle + blitViewport。 |
| `ShaderCache.h/.cpp` | ~250 | Shader 程序缓存：10 种混合模式预编译 + 按名称查找。 |
| `ParticleSystem.h/.cpp` | ~250 | 粒子系统：多线程模拟分片 + GPU 渲染。TD-06 已修复：通过 IRenderDevice* 抽象，无 BgfxRenderDevice 具体依赖。 |
| `GpuMonitor.h/.cpp` | ~80 | GPU 帧时间监控：min/max/avg + 降级策略触发。 |
| `VideoPlayer.h/.cpp` | ~150 | 视频播放器：pl_mpeg (MPEG1) 软件解码 → bgfx 纹理更新。 |
| `EmbeddedShaders.h/.cpp` | ~200 | 内嵌 shader 字节码：DXBC + SPIR-V + Metal，编译期嵌入。 |
| `FreeTypeContext.h/.cpp` | ~100 | FreeType 库上下文单例（RAII 包装）。 |

### 3.3 Audio (`src/Audio/`)

| 文件 | 行 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `SoLoudAudioEngine.h/.cpp` | ~250 | SoLoud 引擎后端：BGM/VOICE/SE 三总线 + 3D 空间音频 + 全局/总线音量 + 淡入淡出 + 波形缓存刷新。 |
| `NullAudioBackend.h/.cpp` | ~100 | 无操作音频后端（headless 模式回退）。 |

### 3.4 Scripting (`src/Scripting/`)

| 文件 | 行 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `LuaManager.h/.cpp` | ~250 | Lua 5.4 VM 管理：init/sandbox/shutdown + 模块注册 + 指令预算 + 安全限制。sandbox.lua 外部化（TD-05 已修复）。 |
| `KAGBinding.h/.cpp` | ~300 | C++→Lua KAG API 绑定：32+ API 通过 BackendRegistry 间接调用渲染/音频。 |
| `RenderBinding.h/.cpp` | ~200 | Lua 渲染绑定：submit_batch/beginBatch/flushBatch/blitTexture 等。 |
| `VFXBinding.h/.cpp` | ~120 | 粒子/VFX 效果 Lua 绑定。 |
| `DevCoreBinding.h/.cpp` | ~150 | 开发核心绑定：引擎状态查询、配置设置等。 |
| `DebugBinding.h/.cpp` | ~100 | 调试 API 绑定：8 个调试函数。 |
| `UnifiedBinding.h/.cpp` | ~150 | 统一后端绑定：便捷方法代理 (show_text 等)。 |
| `GameState.h/.cpp` | ~80 | 游戏状态表管理：ctx 注册表 push/pop。 |

### 3.5 System (`src/System/`)

| 文件 | 行 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `SaveManager.h/.cpp` | ~450 | JSON 保存/加载系统：schema 版本控制 + 迁移链 (v1→v2) + 引擎版本管理。TD-08 已修复：jsonGetRawValue 支持嵌套 JSON。 |
| `SaveBinding.h/.cpp` | ~80 | 存档 Lua 绑定。 |

### 3.6 Carc (`src/Carc/`)

| 文件 | 行 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `CryptoEngine.h/.cpp` | ~200 | Windows BCrypt AES-256-GCM + Ed25519 签名验证。遗留：无 Linux/macOS 回退 (全局约束：跨平台 CI 最后修)。 |
| `CARCReader.h/.cpp` | ~300 | CARC 加密包读取器：zstd 解压 + 解密 + 签名验证。 |
| `CARCWriter.h/.cpp` | ~200 | CARC 打包器（工具端）。 |
| `CRLManager.h/.cpp` | ~200 | 证书吊销列表管理：Local/Online/Hybrid 三模式。TD-18 已修复：fetchOnline 参数简化。 |
| `CarcAssetProvider.h/.cpp` | ~100 | CARC 资源提供者（IAssetProvider 实现）。 |

### 3.7 Resource (`src/Resource/`)

| 文件 | 行 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `AssetManager.h/.cpp` | ~200 | 资源管理器：ProviderChain 代理 + 缓存协调。 |
| `AsyncLoader.h/.cpp` | ~200 | 异步加载器：JobSystem 驱动的后台纹理/音频加载 + 完成回调队列。 |
| `ProviderChain.h/.cpp` | ~80 | 资源提供者链：按优先级回退 (Dir→XP3→CARC)。 |
| `XP3Archive.h/.cpp` | ~250 | KiriKiri XP3 包格式读取器。 |
| `ImageDecoder.h/.cpp` | ~120 | 图像解码器：stb_image 封装 + 格式检测。 |
| `DirAssetProvider.h/.cpp` | ~60 | 目录资源提供者。 |

### 3.8 Debug (`src/Debug/`)

| 文件 | 行 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `HotReload.h/.cpp` | ~250 | 脚本热重载：文件监控 + coroutine 重建 + GameState 重置。TD-09 已修复：移除不必要的 50ms sleep。 |
| `DebugProtocol.h/.cpp` | ~150 | Lua 断点调试协议 (基于 JSON 消息)。 |

### 3.9 Live2D Animation (`src/Animation/Live2D/`)

| 文件 | 状态 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `ILive2DRenderPath.h` | ? | 渲染路径抽象接口 |
| `Live2DBackend.h/.cpp` | ? | Live2D 后端：模型加载/显示/隐藏/透明度 + 动作/表情 + Lua 绑定 |
| `Live2DUserModel.h` | ? | 用户模型封装 |
| `D3D11NativeRenderPath.h/.cpp` | ? Windows | D3D11 本地渲染路径（主力平台已实现） |
| `OpenGLReadbackRenderPath.h/.cpp` | ? Linux | OpenGL FBO → glReadPixels → bgfx（CPU 回读路径） |
| `OpenGLSharedRenderPath.h/.cpp` | ?? 存根 | OpenGL 共享上下文渲染（待实现） |
| `MetalNativeRenderPath.h/.cpp` | ?? 存根 | macOS Metal 渲染（Cubism Metal 渲染器未验证 macOS） |

**条件编译**: `CAESURA_LIVE2D` CMake option (默认 OFF) → `CAESURA_HAS_LIVE2D` 宏守卫

### 3.10 其他模块

| 文件 | 状态 | 功能 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `Editor/EditorServer.h/.cpp` | ?? 待构建 | JSON-RPC over TCP 编辑器后端 |
| `MiniGame/NullMiniGameBackend.cpp` | ? 占位 | 3D 小游戏空实现 |
| `MiniGame/IMiniGameBackend.h` | ? 接口 | 3D 小游戏抽象接口 |
| `Platform/MobileAdapter.h/.cpp` | ? 存根 | 移动端生命周期 + 触摸映射（2 TODO） |


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 4. KAG 命令覆盖

### 4.1 命令清单（61 个 handler）

| 子模块 | 数量 | 命令 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

-----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `text.lua` | 13 | `[ch]` `[text]` `[l]` `[r]` `[er]` `[p]` `[ruby]` `[font]` `[skip]` `[reset]` `[pt]` `[button]` `[endbutton]` |
| `audio.lua` | 14 | `[playbgm]` `[stopbgm]` `[playbgmstop]` `[fadebgm]` `[xfadebgm]` `[playse]` `[stopse]` `[playvoice]` `[stopvoice]` `[waitsound]` `[waitbgm]` `[setbgmvolume]` `[setsevolume]` `[setvoicevolume]` |
| `layer.lua` | 6 | `[bg]` `[fg]` `[cl]` `[image]` `[position]` `[layopt]` |
| `transition.lua` | 10 | `[pregen]` `[get]` `[release]` `[eval]` `[solve_y]` `[apply]` `[trans]` `[move]` `[quake]` `[fade]` |
| `video.lua` | 2 | `[video]` `[stopvideo]` |
| `vfx.lua` | 1 | `[vfx]` (particle/quake/shake/flash/fade/blur/stop 子类型) |
| `save.lua` | 3 | `[save]` `[load]` `[listsaves]` |
| `system.lua` | 4 | `[wait]` `[emb]` `[eval]` `[history]` |
| `resource.lua` | 8 | `[preload]` `[get_texture]` `[is_loaded]` `[is_pending]` `[flush_cache]` `[preload_transition]` `[promote_transition_slot]` `[has_pending_transition]` |

### 4.2 Legacy 别名 + 控制命令

| 命令 | 类型 | 说明 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `[showtext]` | 别名 | → `[text]` |
| `[clearscreen]` | 别名 | → `[cl]` |
| `[br]` `[hr]` | 装饰 | 换行 / 水平线 |
| `[cancel]` | 控制 | 取消当前语音/过渡 |
| `[close]` | 控制 | 关闭场景返回菜单 |
| `[saveplace]` `[loadplace]` | 书签 | 内存场景书签 |
| `[macro]` `[endmacro]` `[erasemacro]` | 文档存根 | 由 scheduler 内联处理 |

### 4.3 流程控制（scheduler 内联，不经过 KAG 表）

`[if]` `[jump]` `[call]` `[return]` `[label]` `[end]` `[macro]` `[eval]` `[wait]` `[stop]`


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 5. 技术债清单（实时更新）

### P0 — 零项 ??

### P1 — 阻塞功能正确性

| ID | 位置 | 描述 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| TD-07 | `Carc/CryptoEngine` | Windows-only BCrypt，无 Linux/macOS OpenSSL 回退 |

### P2 — 影响可维护性

| ID | 位置 | 描述 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| TD-12 | `Render/IVideoDecoder.h` | FFmpeg 视频解码器接口存根（当前使用 pl_mpeg） |
| TD-13 | `Render/BgfxRenderDevice.cpp:980` | @Beta: 过渡规则图集预烘焙优化 |
| TD-14 | `Platform/MobileAdapter` | 移动端生命周期存根（2 TODO） |

### P3 — 低优先级

| ID | 位置 | 描述 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| TD-19 | `Animation/Live2D/OpenGLSharedRenderPath` | OpenGL 共享上下文渲染存根 |
| TD-20 | `Animation/Live2D/MetalNativeRenderPath` | macOS Metal 渲染存根 |
| TD-21 | `Animation/Live2D/Live2DBackend.cpp:228` | TODO: 纹理加载到 Cubism 渲染器 |

### 已闭合 (2026-06-09)

TD-01~TD-06, TD-08~TD-11, TD-15~TD-18 — 全部在本轮或前序回合修复。


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 6. 测试覆盖

### C++ 单元测试 (doctest, 24 文件)

| 测试文件 | 覆盖模块 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
| `test_core.cpp` | BackendRegistry + DebugManager |
| `test_engine_lifecycle.cpp` | Engine 全生命周期 |
| `test_render.cpp` | BgfxRenderDevice + RTTManager |
| `test_carc.cpp` | CryptoEngine + CARCReader/Writer |
| `test_system.cpp` | SaveManager |
| `test_resource.cpp` | AssetManager + ProviderChain |
| `test_async.cpp` | AsyncLoader |
| `test_job_system.cpp` | JobSystem 线程池 |
| `test_kag_binding.cpp` | KAGBinding |
| `test_kag_integration.cpp` | KAG 集成测试 |
| `test_lua_manager.cpp` | LuaManager |
| `test_audio.cpp` | SoLoudAudioEngine |
| `test_input.cpp` | InputRouter |
| `test_debug.cpp` | DebugManager + HotReload |
| `test_image_decoder.cpp` | ImageDecoder |
| `test_texture_budget_edge.cpp` | TextureBudget 边界 |
| `test_resource_handle.cpp` | ResourceHandle 代际 |
| `test_error_ui.cpp` | ErrorUI |
| `test_save_migration.cpp` | SaveManager 迁移链 |
| `test_mobile_adapter.cpp` | MobileAdapter |
| `test_particle_system.cpp` | ParticleSystem |
| `test_sandbox_quota.cpp` | SandboxQuota |
| `test_mini_game.cpp` | MiniGame 接口 |
| `test_main.cpp` | 测试入口 |

### Lua 集成测试 (10 脚本)

| 测试脚本 | 覆盖 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| `test_kag_commands.lua` | KAG 命令执行 |
| `test_kag_e2e.lua` | KAG 端到端 |
| `test_integration.lua` | 全栈集成 |
| `test_sandbox.lua` | 沙箱安全 |
| `test_scheduler.lua` | 调度器 |
| `test_tokenizer.lua` + `test_tokenizer_adv.lua` | KAG 分词器 |
| `test_full_story_parse.lua` | 完整剧情解析 |
| `g9_test.lua` `run_lua_tests.lua` | 测试运行器 |

### 冒烟/集成场景

`tests/scripts/smoke_test.ks` `integration_test.ks` `save_test.ks` `full_story.ks` `u5a_smoke.ks`


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 7. 构建体系

- **项目**: `CaesuraAmeKAG` v1.0.0, C++20
- **CMake**: 3.25+
- **Windows**: MSVC, D3D11 后端, BCrypt 加密
- **Linux** (CI 目标): GCC, OpenGL 后端
- **macOS** (CI 目标): Clang, Metal 后端
- **预定义宏**: `SDL_MAIN_HANDLED`, `_CRT_SECURE_NO_WARNINGS`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`
- **可选功能**: `CAESURA_LIVE2D` (OFF), `CAESURA_VIDEO_FFMPEG` (OFF), `CAESURA_DEBUG` (OFF by default)

### 构建命令

```powershell
# Windows (无 Live2D)
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug

# Windows (含 Live2D)
cmake -B build_l2d -G "Visual Studio 17 2022" -DCAESURA_LIVE2D:BOOL=ON -DCUBISM_SDK_ROOT:PATH="CubismSdkForNative-5-r.5"
cmake --build build_l2d --config Debug
```


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 8. Live2D Cubism 5 集成状态

| 平台 | 渲染路径 | 状态 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
| **Windows** | D3D11 原生 | ? 已实现 |
| **Windows/Linux** | OpenGL 读回 | ? 已实现 (CPU 回读) |
| **Linux** | OpenGL 共享上下文 | ?? 存根 |
| **macOS** | Metal 原生 | ?? 存根 (Cubism Metal 未验证 macOS) |
| **iOS** | Metal | ? 未测试 |
| **Android** | OpenGL | ? 未测试 |

**法律安全**: SDK 二进制不入库，`#ifdef CAESURA_HAS_LIVE2D` 条件编译，CMake option 默认 OFF。


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 9. 跨平台 CI 状态

| 平台 | 编译器 | 后端 | 状态 | 阻塞项 |
|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

-----|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---|
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---
## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

-----|
| Windows | MSVC | D3D11 | ? 本地通过 | — |
| Linux | GCC | OpenGL | ?? CI 失败 | CryptoEngine BCrypt→OpenSSL |
| macOS | Clang | Metal | ?? CI 失败 | CryptoEngine BCrypt→OpenSSL |

> **全局约束**: 跨平台 CI 修复排到最后。


## 11. 核心约束

> **?? 所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> 禁止路径: Lua 直接引用 BgfxRenderDevice、SoLoudAudioEngine 等具体类；
> C++ 端禁止向 Lua 暴露具体类指针（只允许接口指针）。

---

## 10. 总体评估

### 优势
- 7 个纯虚接口，全部支持多态替换
- BackendRegistry 单例解耦 C++ 内核与 Lua 脚本层（TD-03 已闭合）
- 沙箱安全模型完善：Lua 环境锁定 + 文件 I/O 禁用 + require 白名单
- 双级 ErrorUI + 智能重试错误恢复链
- 6 级自适应纹理预算 + 3 级 GPU 降级策略
- 多线程粒子模拟 + 异步纹理加载 + CARC 解密 + CPU 通用任务
- 61 个 KAG 命令覆盖文本/音频/图层/过渡/VFX/视频/存档全流程
- 条件编译 Live2D 集成无法律风险

### 待推进
- 跨平台 CI (Linux/macOS) — 全局约束排最后
- Live2D OpenGLShared + Metal 渲染路径
- MiniGame 3D 小游戏后端实现
- FFmpeg 视频解码替代 pl_mpeg
- 移动端适配 (MobileAdapter 2 TODO)
