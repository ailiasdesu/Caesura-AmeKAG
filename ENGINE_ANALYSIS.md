# Caesura (AmeKAG) 引擎代码库深度分析

> 分析日期: 2026-06-09 | 构建配置: CMake 3.25+ / C++20
> 构建状态: Windows Debug + Release 全量通过，D3D11 零 TDR 稳定
> 最近修复: 核心约束合规（c1e84c6）— 所有 Lua 绑定仅通过抽象接口访问 C++

---

## 0. 核心约束（强制）

> **所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> **禁止**: Lua/绑定层直接引用 BgfxRenderDevice、SDL3PlatformBackend 等具体类。
> **允许**: BackendRegistry.cpp、Engine.cpp（工厂/接线层）知晓具体类。
> **状态**: ✅ 已全面合规（2026-06-09 提交 c1e84c6 闭合所有违规）。

---

## 1. 项目规模

| 维度 | 数值 |
|------|------|
| C++ 文件 | 57 .cpp + 66 .h = 123 个编译单元 |
| C++ 代码量 | 16,275 行 |
| Lua 脚本 | 43 个文件 / 8,410 行 |
| C++ 单元测试 | 24 个文件 |
| KAG 命令 | 61 个（6 个子模块: layer/text/audio/system/transition/video） |
| 纯虚接口 | 8 个: IRenderDevice, IAudioBackend, IPlatformBackend, IAssetProvider, IAnimationBackend, ILive2DRenderPath, IMiniGameBackend, IVideoDecoder |
| 外部库 | 10 个全部静态编译 (bgfx/bimg/bx/SoLoud/Lua/FreeType/zstd/ed25519/pl_mpeg/cpp-httplib) |
| 模块 | 12 个: Core/Render/Audio/Scripting/System/Carc/Resource/Debug/Editor/MiniGame/Animation/Platform |

---

## 2. 架构总览

```
src/
├── Core/          ← Engine, BackendRegistry, InputRouter, DebugManager, ErrorUI
│                   JobSystem (TBB-style), SandboxQuota, TextureBudget, RpcServer
├── Render/        ← BgfxRenderDevice (IRenderDevice 实现), LayerManager, TextRenderer
│                   TextureManager, RTTManager, ShaderCache, ParticleSystem, GpuMonitor
│                   VideoPlayer (pl_mpeg)
├── Audio/         ← SoLoudAudioEngine + NullAudioBackend (BGM/VOICE/SE 三通道)
├── Scripting/     ← LuaManager, KAGBinding, RenderBinding, VFXBinding
│                   DevCoreBinding, DebugBinding, UnifiedBinding, GameState
├── System/        ← SaveManager (JSON 保存/加载 + 版本迁移)
├── Carc/          ← CryptoEngine (BCrypt), CARCReader/Writer, CRLManager
├── Resource/      ← AssetManager, AsyncLoader (多线程), ProviderChain, XP3Archive, ImageDecoder
├── Debug/         ← HotReload + DebugProtocol (Lua 断点调试)
├── Editor/        ← EditorServer (Electron IDE JSON-RPC 服务端)
├── MiniGame/      ← IMiniGameBackend + NullMiniGameBackend (3D 小游戏预留)
├── Animation/     ← IAnimationBackend + Live2D Backend (Cubism 5, CAESURA_HAS_LIVE2D)
│                   ILive2DRenderPath: D3D11Native ✅, OpenGLShared ⚠️, Metal ⚠️, OpenGLReadback ⚠️
└── Platform/      ← MobileAdapter (移动端生命周期存根)
```

**初始化链**: SDL3 → bgfx → SoLoud → Lua VM → SaveManager → HotReload
**主循环**: processEvents → engine_update (Lua) → beginFrame → engine_render (Lua) → MiniGame hook → endFrame → 音频检测
**关闭链**: 逆序

---

## 3. 模块详解

### 3.1 Core（引擎中枢）
**文件**: 10 .cpp + 12 .h

| 类 | 功能 | 依赖 |
|----|------|------|
| Engine | 主循环 + 生命周期管理，持有所有后端 unique_ptr | SDL3, bgfx, SoLoud, Lua |
| BackendRegistry | 单例工厂，注册/获取已创建的后端指针（缓存裸指针，不拥有） | 所有 I*Backend |
| InputRouter | KAG/GAME 输入焦点路由 | SDL3 事件 |
| DebugManager | ErrorUI 触发 + 重试逻辑 | bgfx, SDL |
| ErrorUI | 双级错误界面：Level1 (bgfx 存活) / Level2 (SDL MessageBox) | bgfx, SDL |
| JobSystem | TBB 风格多线程任务调度（粒子纹理/CARC/CPU 任务） | 无 |
| SandboxQuota | Lua 执行预算管理 | 无 |
| TextureBudget | 6 级自适应纹理内存预算（128MB~4GB） | 无 |
| RpcServer | JSON-RPC 服务端（Electron IDE 通信） | cpp-httplib |

**接口边界**: Engine 持有所有后端 unique_ptr，BackendRegistry 缓存裸指针供 Lua 层访问。

### 3.2 Render（渲染管线）
**文件**: 12 .cpp + 13 .h

| 类 | 功能 | 依赖 |
|----|------|------|
| BgfxRenderDevice | IRenderDevice 实现，D3D11/Metal/OpenGL 后端 | bgfx |
| LayerManager | 多图层透明合成（submit_batch 批量提交） | BgfxRenderDevice |
| TextRenderer | FreeType TTF 渲染 + CJK 静态图集 + MessageLayerCache 批缓存 | FreeType, bgfx |
| TextureManager | 纹理生命周期 + solid texture 创建 | bgfx |
| RTTManager | Render-to-Texture 视口管理 | bgfx |
| ShaderCache | 5 级着色器缓存（DXBC/GLSL/Metal/SPIRV/回退） | bgfx |
| ParticleSystem | 多线程粒子模拟 + GPU 提交（通过 IRenderDevice*） | JobSystem, bgfx |
| GpuMonitor | TDR 防护：每 500μs 让步 + 5ms 超时检测 | bgfx |
| VideoPlayer | pl_mpeg 视频解码播放 | pl_mpeg, bgfx |

**接口边界**: BgfxRenderDevice 实现 IRenderDevice 全部方法。Lua 层不感知 BgfxRenderDevice。

### 3.3 Audio（音频引擎）
**文件**: 2 .cpp + 2 .h

| 类 | 功能 |
|----|------|
| SoLoudAudioEngine | BGM/VOICE/SE 三通道独立音量 + 淡入淡出 |
| NullAudioBackend | 无音频环境静默回退 |

### 3.4 Scripting（Lua 绑定层）
**文件**: 8 .cpp + 8 .h

| 模块 | 功能 | C++ 访问方式 |
|------|------|-------------|
| LuaManager | Lua VM 生命周期 + 沙箱安全（require 白名单、I/O 禁用） | 直接 |
| KAGBinding | KAG 脚本调度器 + 61 命令 handler 表 | backend.lua → BackendRegistry |
| RenderBinding | 渲染命令: submit_batch/blend/transition/vfx/fill_viewport | BackendRegistry::getRenderDeviceFromLua |
| DevCoreBinding | 开发工具: 分辨率/全屏/输入焦点/日志/退出 | BackendRegistry::getPlatformBackendFromLua |
| VFXBinding | 视觉效果（旧版，已简化） | BackendRegistry |
| DebugBinding | 调试接口注册 | BackendRegistry |
| UnifiedBinding | 统一 Lua 模块注册入口 | 所有子绑定 |
| GameState | 全局游戏状态管理 | 无 |

**合规状态**: ✅ 所有绑定层仅使用 IRenderDevice*/IPlatformBackend* 抽象指针，通过 BackendRegistry 获取。

### 3.5 System（存档系统）
**文件**: 2 .cpp + 2 .h

| 类 | 功能 |
|----|------|
| SaveManager | JSON 存档 + 版本迁移（v1→v2→v3） |

### 3.6 Carc（加密资源包）
**文件**: 5 .cpp + 6 .h

| 组件 | 功能 | 多线程 |
|------|------|--------|
| CryptoEngine | AES-256-GCM 加密（BCrypt，跨平台需 OpenSSL） | ✅ JobSystem |
| CARCReader | .carc 包读取 + 解密 | ✅ AsyncLoader |
| CARCWriter | .carc 包创建工具 | 否 |
| CRLManager | 证书吊销列表 | 否 |

**技术债务**: BCrypt 仅在 Windows 可用（TD-12），Linux/macOS 需替换为 OpenSSL。

### 3.7 Resource（资源管线）
**文件**: 6 .cpp + 8 .h

| 组件 | 功能 | 多线程 |
|------|------|--------|
| AssetManager | 协调 ProviderChain 资源查找 | 否 |
| AsyncLoader | 异步加载队列（纹理/CARC/CPU 任务） | ✅ JobSystem |
| ProviderChain | DirAssetProvider → XP3Archive → CarcAssetProvider（优先级递减） | 否 |
| XP3Archive | KiriKiri .xp3 包兼容读取 | 否 |
| ImageDecoder | PNG/JPEG/BMP 解码 | stb_image |

### 3.8 Animation（动画后端）
**文件**: 6 .cpp + 9 .h

| 组件 | 功能 | 状态 |
|------|------|------|
| IAnimationBackend | 动画后端纯虚接口 | ✅ |
| ILive2DRenderPath | Live2D 渲染路径抽象（策略模式） | ✅ |
| D3D11NativeRenderPath | D3D11 原生渲染路径 | ✅ 完成 |
| OpenGLSharedRenderPath | 共享 bgfx GL 上下文渲染 | ⚠️ 待实现 |
| MetalNativeRenderPath | Metal 原生渲染 | ⚠️ macOS 待验证 |
| OpenGLReadbackRenderPath | OpenGL 读回渲染（回退路径） | ⚠️ 待实现 |
| Live2DBackend | Cubism 5 模型生命周期 + Lua 绑定（CAESURA_HAS_LIVE2D） | ✅ |
| Live2DUserModel | CubismUserModel 封装 + 动作/表情管理 | ✅ |

**法律合规**: Cubism SDK 不提交到 GitHub，`CAESURA_HAS_LIVE2D` 条件编译，用户自行下载 SDK。

### 3.9 MiniGame（3D 小游戏预留接口）
**文件**: 1 .cpp + 2 .h

| 组件 | 功能 | 状态 |
|------|------|------|
| IMiniGameBackend | 小游戏后端抽象接口 | ✅ 接口定义 |
| NullMiniGameBackend | 空实现（默认） | ✅ |

### 3.10 Debug（热重载）
**文件**: 2 .cpp + 2 .h

| 组件 | 功能 |
|------|------|
| HotReload | Lua 脚本文件监控 + 自动重载 |
| DebugProtocol | Lua 断点调试协议 |

### 3.11 Editor（IDE 集成）
**文件**: 1 .cpp + 1 .h

| 组件 | 功能 | 状态 |
|------|------|------|
| EditorServer | JSON-RPC 服务端，Electron IDE 通信 | ✅ 后端就绪 |
| Electron 前端 | 可视化编辑器 | ⚠️ 待开发 |

### 3.12 Platform（移动端）
**文件**: 1 .cpp + 1 .h

| 组件 | 功能 | 状态 |
|------|------|------|
| MobileAdapter | 移动端生命周期存根 | ⚠️ 2 个 TODO |

---

## 4. 技术债务追踪

| ID | 描述 | 严重度 | 状态 |
|----|------|--------|------|
| TD-01 | BackendRegistry 三重所有权 | 高 | ✅ 闭合 (Engine unique_ptr) |
| TD-02 | Lua 文件访问绕过沙箱 | 高 | ✅ 闭合 |
| TD-03 | Factory 方法返回 raw new（无 delete） | 高 | ✅ 闭合 |
| TD-04 | recursive_mutex → mutex 替换 | 中 | ✅ 闭合 |
| TD-05 | ParticleSystem 使用 BgfxRenderDevice* | 中 | ✅ 闭合 (IRenderDevice*) |
| TD-06 | Live2DBackend::setRenderDevice 使用 BgfxRenderDevice* | 中 | ✅ 闭合 (IRenderDevice*) |
| TD-07 | DevCoreBinding/RenderBinding static_cast 到具体类 | 高 | ✅ 闭合 |
| TD-08 | jsonGetRawValue 缺失嵌套提取 | 低 | ✅ 闭合 |
| TD-09 | HotReload 不必要 sleep | 低 | ✅ 闭合 |
| TD-10 | TextRenderer::init 使用 BgfxRenderDevice* | 中 | ✅ 闭合 (IRenderDevice*) |
| TD-11 | Live2D OpenGLSharedRenderPath 待实现 | 中 | ⚠️ 开放 |
| TD-12 | CryptoEngine BCrypt 仅 Windows，Linux/macOS 需 OpenSSL | 高 | ⚠️ 开放 |
| TD-13 | CJK 文本缓存 cacheIsCjk 标记 | 低 | ✅ 闭合 |
| TD-14 | CARC 差分更新（Delta CARC） | 低 | ⚠️ 未实现 |
| TD-15 | pl_mpeg → FFmpeg 视频解码升级 | 低 | ⚠️ 未实现 |
| TD-16 | CAESURA_DEBUG 默认值 | 低 | ✅ 闭合 |
| TD-17 | submit_batch RTT view ID → tex ID 键名不匹配 | 中 | ⚠️ 开放 |
| TD-18 | CRLManager 冗余 url 参数 | 低 | ✅ 闭合 |
| TD-19 | Live2D MetalNativeRenderPath macOS 待验证 | 中 | ⚠️ 开放 |
| TD-20 | MobileAdapter 移动端适配未实现 | 低 | ⚠️ 开放 |
| TD-21 | MiniGame 3D 后端未实现 | 低 | ⚠️ 开放 |
| TD-22 | 跨平台 CI（Linux/macOS 构建失败） | 高 | ⚠️ 开放（全局约束最后） |

**摘要**: 12/22 闭合，10 开放（其中 7 个为功能未实现而非 bug）。

---

## 5. 多线程架构

```
主线剧情（单线程顺序）:
  processEvents → engine_update (Lua) → beginFrame → engine_render → endFrame

辅助多线程（JobSystem）:
  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  ┌──────────────┐
  │ AsyncLoader │  │ ParticleSim  │  │ CARC Decrypt  │  │ CPU Tasks    │
  │ (纹理加载)   │  │ (粒子物理)    │  │ (资源解密)     │  │ (通用计算)    │
  └─────────────┘  └──────────────┘  └───────────────┘  └──────────────┘
```

- **主线不阻塞**: 剧情推进永远单线程顺序执行，保证流程确定性
- **多核用于辅助**: 后台加载/解密/解析/粒子推演，不参与游戏逻辑
- **JobSystem**: TBB 风格任务窃取，当前仅 Windows 线程池实现

---

## 6. 平台支持

| 平台 | 编译器 | 渲染后端 | 构建状态 | 备注 |
|------|--------|----------|----------|------|
| Windows | MSVC | D3D11 | ✅ 本地通过 | 零 TDR |
| Linux | GCC | OpenGL | ❌ CI 失败 | CryptoEngine BCrypt→OpenSSL |
| macOS | Clang | Metal | ❌ CI 失败 | CryptoEngine BCrypt→OpenSSL |

> **全局约束**: 跨平台 CI 修复排到最后。

---

## 7. Live2D 渲染路径矩阵

| 路径 | Windows D3D11 | Linux OpenGL | macOS Metal | iOS Metal | Android OpenGL |
|------|:---:|:---:|:---:|:---:|:---:|
| D3D11Native | ✅ | — | — | — | — |
| OpenGLShared | — | ⚠️ | — | — | ⚠️ |
| MetalNative | — | — | ⚠️ | ⚠️ | — |
| OpenGLReadback | — | ⚠️ | — | — | ⚠️ |

> Metal 在官网标记为仅 iOS；macOS 需用 OpenGL 或 Metal+OpenGL 并行方案。

---

## 8. 总体评估

### 优势
- 8 个纯虚接口全部支持多态替换，核心约束已全面合规
- BackendRegistry 单例解耦 C++ 内核与 Lua 脚本层
- 沙箱安全模型完善：Lua require 白名单 + I/O 禁用 + 指令预算
- 双级 ErrorUI + 智能重试错误恢复链
- 6 级自适应纹理预算 + 3 级 GPU 降级 + TDR 防护
- 多线程 JobSystem 覆盖粒子/CARC/纹理/CPU 任务
- 61 个 KAG 命令覆盖文本/音频/图层/过渡/VFX/视频/存档全流程
- MIT 许可 + Live2D 条件编译无法律风险

### 待推进
- 跨平台 CI (Linux/macOS) — 全局约束排最后
- Live2D OpenGLShared + Metal 渲染路径
- MiniGame 3D 小游戏后端实现
- pl_mpeg → FFmpeg 视频解码升级
- CARC 差分更新
- 移动端适配