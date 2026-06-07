# Caesura (AmeKAG) 引擎代码库深度分析

> 分析日期: 2026-06-07 | 构建配置: CMake 3.25+ / MSVC / C++20
> 构建状态: 全量成功构建（Release + Debug），无废弃代码残留
> 仅分析可构建模块，忽略已删除的 ce-compound 及 docs 文档层

---

## 1. 项目架构总览

```
Caesura(AmeKAG)/
├── src/
│   ├── Core/          ← 引擎中枢：主循环、后端注册表、输入路由、诊断、错误UI
│   ├── Render/        ← 渲染管线：bgfx设备、图层、字体、纹理、Shader缓存、粒子、视频
│   ├── Audio/         ← 音频后端：SoLoud 引擎 + NullAudioBackend 回退
│   ├── Scripting/     ← Lua 绑定层：KAG脚本、渲染、VFX、DevCore、Debug、Unified
│   ├── System/        ← 存档系统：JSON 序列化 + 版本迁移
│   ├── Carc/          ← 加密资源包：AES-256-GCM + Ed25519 + CRL
│   ├── Resource/      ← 资源提供者链：目录/XP3/CARC + 异步加载器
│   ├── Debug/         ← 开发工具：热重载 + Lua 断点调试协议
│   └── Platform/      ← 移动端适配存根（P2 预留）
├── external/          ← 6 个第三方库全静态编译（bgfx+bimg+bx、SoLoud、Lua、FreeType、zstd、ed25519）
├── shaders/           ← bgfx shader 源码（dx11/glsl/metal）
├── scripts/           ← Lua 游戏逻辑层（kag/ 子目录）
└── tests/             ← 单元测试（Catch2 + Sol2）
```

---

## 2. 模块详细分析

### 2.1 Core 模块 (`src/Core/`)

**构建文件**: 5 cpp + 6 h (+ 2 纯接口头文件)

| 组件 | 文件 | 功能 |
|------|------|------|
| Engine | `Engine.h/.cpp` | 顶层引擎类：初始化顺序链 (SDL3→bgfx→SoLoud→Lua→SaveManager→HotReload)、主循环 (processEvents→render→音频检测)、关闭逆序链。管理所有子系统 unique_ptr 所有权，通过 BackendRegistry 暴露引用。Lua 256MB 内存上限 + 自定义 alloc hook。CAESURA_ASSERT_MAIN_THREAD 调试宏。 |
| BackendRegistry | `BackendRegistry.h/.cpp` | 后端注册表单例：IRenderDevice / IAudioBackend / IPlatformBackend / InputRouter / VideoPlayer / TextureManager / LayerManager / TextureBudget 的 getter/setter + 工厂创建（按名称字符串）。ResourceHandle 代际追踪（安全校验 + 热重载失效）。暴露为 Lua Engine 表函数。 |
| IPlatformBackend | `IPlatformBackend.h` | 抽象平台后端接口（纯虚类）：init/shutdown/pollEvent/getTicksMs/getNativeWindowHandle/getWindowWidth/Height/setFullscreen/MouseState/getBackendName。 |
| SDL3PlatformBackend | `SDL3PlatformBackend.h/.cpp` | SDL3 具体实现：窗口创建、事件轮询、定时、全屏切换、原生句柄暴露。 |
| IAudioBackend | `IAudioBackend.h` | 抽象音频后端接口（24 纯虚函数）：BGM/VOICE/SE 三总线体系、3D 空间音频、全局/总线音量、波形缓存刷新、播放位置/长度查询、淡入淡出。 |
| InputRouter | `InputRouter.h/.cpp` | 输入路由器：KAG/GAME 焦点切换，按焦点分发 SDL 事件到回调向量。KAG 点击挂起检测（wait_click）。窗口 resize 回调通知。 |
| DebugManager | `DebugManager.h/.cpp` | 诊断单例：6 级日志 (Trace/Fatal) + 环形缓冲区 (1024 条) + 7 子系统错误计数 + 帧分析 (GPU提交/Lua GC/瞬态分配) + 子系统状态快照 (Render/Audio/Input) + PROFILE_SCOPE 宏。日志写文件 + 控制台双输出。 |
| ErrorUI | `ErrorUI.h/.cpp` | 双级错误屏幕：Level1 (bgfx 存活: 深红画布 + 白色调试文字) / Level2 (SDL MessageBox 回退)。Retry/Title/Quit 三按钮。智能重试：白名单检查 + 动画/过渡操作自动提级 + 同 token 3 次提级 + 3 次连续 title 失败→exit。 |
| TextureBudget | `TextureBudget.h/.cpp` | 6 级纹理内存预算：128MB→4GB，自动检测系统 RAM 分档，支持 config.lua 覆盖。 |

**公共接口边界**:
- `Engine` → 外部入口点，main.cpp 唯一调用
- `BackendRegistry` → Scripting 模块的 C++ 后端查询通道
- `IPlatformBackend` / `IAudioBackend` → Audio/Render 模块的多态抽象
- `InputRouter` → Engine + Scripting 的输入分发中心
- `DebugManager` → 全引擎的日志/诊断通道（通过宏）
- `ErrorUI` → Engine + KAGBinding 错误恢复路径

**依赖**: SDL3 (platform), bgfx (Render), SoLoud (Audio), Lua (Scripting)

**技术债务**:
- `BackendRegistry` 同时持有原始指针 (`m_renderDevice`) 和 `unique_ptr` (`m_ownedRenderDevice`) ，双轨管理容易产生悬垂指针
- `Engine::render()` 在每帧调用两次 Lua (engine_render + Engine_OnFrameRender) ，未合并
- `DebugManager` 使用 `recursive_mutex` 但日志宏调用路径始终在主线程，锁冗余
- `CAESURA_DEBUG` 宏默认强制 `#define CAESURA_DEBUG 1` ，无 Release 剥离能力

### 2.2 Render 模块 (`src/Render/`)

**构建文件**: 11 cpp + 13 h

| 组件 | 文件 | 功能 |
|------|------|------|
| IRenderDevice | `IRenderDevice.h` | 抽象渲染设备接口：init/shutdown/flushAllRTT/beginFrame/endFrame/commit_frame/视图管理/RT 创建销毁/blitViewport/blitTexture/stretchBlt/affineBlt/批量协议/调试标记/文字渲染/字体设置。VIEW_RTT=0, VIEW_MAIN=1, VIEW_DEBUG=2, VIEW_TRANSITION=99。 |
| BgfxRenderDevice | `BgfxRenderDevice.h/.cpp` | bgfx 具体实现：DX11/GL/Metal 多后端、视图顺序、嵌入式 Shader 加载、复合程序构建、纹理 blit（2D + stretch + affine）。所有 GPU 调用限定主线程。 |
| RTTManager | `RTTManager.h/.cpp` | 离屏渲染目标池管理器：2D/3D 分池、acquireCanvas/releaseCanvas、延迟销毁队列、resize 全清、截图快照。 |
| LayerManager | `LayerManager.h/.cpp` | KAG 三层 (BG/FG/MSG) 状态 + 脏矩形追踪（按层标记，帧末合并，>75% 画面则全帧重绘） + Z 序提交。单例，注册于 BackendRegistry。 |
| FontRenderer | `FontRenderer.h/.cpp` | FreeType 文字渲染器：动态图集 (2048x2048 RGBA8)、ASCII 预填充、CJK 懒加载 + 静态 CJK 图集回退 (G8-U5)、消息层批缓存 (DynamicVB/IB)、字形矩形提交。 |
| TextRenderer | `TextRenderer.h/.cpp` | 位图字体 + TTF 文字渲染器：Small/Large/TTF 三字体 ID、光标管理、ruby 注音渲染。与 FontRenderer 共享 FreeTypeContext。 |
| FreeTypeContext | `FreeTypeContext.h/.cpp` | 全局 FT_Library 单例共享实例，避免 FontRenderer/TextRenderer 各初始化一个。 |
| TextureManager | `TextureManager.h/.cpp` | 纹理生命周期管理器：路径/内存加载、纯色纹理工厂、placeholder 棋盘格（dev 模式）、LRU 淘汰、字节追踪 + 预算检查。 |
| ShaderCache | `ShaderCache.h/.cpp` | 28 种 Photoshop 复合 BlendMode 枚举 + CompositeShaderKey (blend+palette) → bgfx::ProgramHandle LRU 缓存 (64 条目)。预编译 10 种最常见组合。 |
| GpuMonitor | `GpuMonitor.h/.cpp` | GPU 自适应降级：3 帧超预算 (16.67ms)→降一级，10 帧良好→升一级。三档质量 (HIGH/MEDIUM/LOW)，分辨率缩放 + VFX 开关。 |
| ParticleSystem | `ParticleSystem.h/.cpp` | 粒子系统：1024 上限、发射器管理、速度/重力/生命/颜色、bgfx 四边形提交。 |
| VideoPlayer | `VideoPlayer.h/.cpp` | MPEG1 视频播放 (pl_mpeg)：多实例、帧解码/纹理更新、暂停/继续/seek、可选 FFmpeg 后端 (CAESURA_VIDEO_FFMPEG)。 |
| EmbeddedShaders | `EmbeddedShaders.h/.cpp` + `_SPIRV.cpp` | 内嵌 Shader 二进制（DX11/GLSL/Metal/SPIRV），作为 C++ uint8_t 数组编译进引擎。 |
| IVideoDecoder | `IVideoDecoder.h` | @Beta: FFmpeg 解码器抽象接口（纯虚），无实现。 |

**公共接口边界**:
- `IRenderDevice` → Core (Engine/BackendRegistry) + Scripting (RenderBinding)
- `LayerManager` → Scripting (RenderBinding/Lua layers.lua)
- `FontRenderer` / `TextRenderer` → Scripting (文字渲染)
- `TextureManager` → Scripting (纹理加载/缓存)
- `ShaderCache` → BgfxRenderDevice (程序创建)
- `GpuMonitor` → Engine (帧循环)
- `ParticleSystem` / `VideoPlayer` → Scripting (VFXBinding / VideoPlayer)

**依赖**: bgfx + bimg + bx、FreeType、stb_image、bx::FileReader、pl_mpeg、(可选 FFmpeg)

**技术债务**:
- `FontRenderer.cpp:175` - `@Beta: Replace hardcoded 2048x2048 threshold with configurable atlas growth policy`
- `IVideoDecoder.h:2` - `@Beta: FFmpeg-based video decoder planned for Beta`
- `RTTManager.cpp` - 6 处 `// assert(isMainThread()); // @Beta` 被注释掉
- `BgfxRenderDevice.cpp:938` - `Spec [10.2.25]: @Beta — Pre-bake rule images into a LUT texture atlas`
- `TextRenderer` 和 `FontRenderer` 功能高度重叠（两者都有 TTF 渲染 + FreeType 集成），应合并
- `ParticleSystem` 依赖 `BgfxRenderDevice*` 裸指针（未通过抽象接口）

### 2.3 Audio 模块 (`src/Audio/`)

**构建文件**: 2 cpp + 2 h

| 组件 | 文件 | 功能 |
|------|------|------|
| SoLoudAudioEngine | `SoLoudAudioEngine.h/.cpp` | IAudioBackend 的 SoLoud 实现：BGM/VOICE/SE 三总线、波形文件 LRU 缓存 (O(1) touch)、3D 空间音频、SE 句柄追踪（批量停止）、总线音量持久化、淡入淡出。 |
| NullAudioBackend | `NullAudioBackend.h/.cpp` | 无操作音频回退：24 个纯虚方法全部返回安全默认值。用于无头模式/CI。 |

**公共接口边界**: 通过 `IAudioBackend` 抽象 → BackendRegistry → Scripting (UnifiedBinding/RenderBinding)

**依赖**: SoLoud

**技术债务**: 无明显 TODO/FIXME。LRU 缓存设计合理，SE 句柄数组式追踪 O(n) stopSE 在小规模下可接受。

### 2.4 Scripting 模块 (`src/Scripting/`)

**构建文件**: 9 cpp + 8 h

| 组件 | 文件 | 功能 |
|------|------|------|
| LuaManager | `LuaManager.h/.cpp` | Lua VM 生命周期：luaL_newstate→openlibs→GameState::create→registerModules (8 个绑定模块)→lockdownScriptEnv (沙盒：禁用 loadfile/dofile/os.execute/io.open/io.popen、替换 require 为只返回已加载模块、debug 库只读子集)。 |
| KAGBinding | `KAGBinding.h/.cpp` | 32 个 KAG C API：show_text/show_image/clear_screen/wait_click/场景跳转/标记/分支/音乐/音效/语音/视频/存档/动画/过渡/选择支等。全部通过 BackendRegistry 解析后端。 |
| RenderBinding | `RenderBinding.h/.cpp` | 渲染 Lua 绑定：纹理加载/销毁/查询、纯色纹理、图层提交 (BG/FG/MSG)、文字渲染、批量协议。 |
| VFXBinding | `VFXBinding.h/.cpp` | 粒子系统 Lua 绑定：发射器创建/销毁/发射 + CAESURA_BACKEND 兼容路由。 |
| DevCoreBinding | `DevCoreBinding.h/.cpp` | 开发/核心 Lua 绑定：InputRouter 相关控制。 |
| DebugBinding | `DebugBinding.h/.cpp` | 10 个诊断 API：帧时间、GPU 状态、纹理统计、日志查询等。 |
| UnifiedBinding | `UnifiedBinding.h/.cpp` | 统一后端代理表 `_CAESURA_BACKEND`：render()/audio()/platform() 分发 + show_text/show_image/clear_screen/wait_click 便捷方法。 |
| GameState | `GameState.h/.cpp` | KAG 全局上下文表：协程、token、调用栈、f/sf/tf 标志表、图层引用、backlog、CancelToken 操作追踪。存储在 Lua 注册表 (caesura_ctx)。 |

**公共接口边界**:
- `LuaManager` → Engine (init/shutdown/loadScript) + main.cpp
- KAGBinding / RenderBinding / VFXBinding / DevCoreBinding / DebugBinding / UnifiedBinding → LuaManager::registerModules
- SaveBinding → 归入 System 概念但注册在此
- GameState → KAGBinding (上下文管理)

**依赖**: Lua 5.4、BackendRegistry、IRenderDevice、IAudioBackend、InputRouter

**技术债务**:
- `LuaManager.cpp` 里 `lockdownScriptEnv()` 用 `luaL_dostring` 内联大量 Lua 代码段，应抽到独立 .lua 沙盒模块文件
- `UnifiedBinding` 与各独立 Binding 存在分层重复（show_text 在 Unified 和 KAGBinding 各实现一次）
- `RenderBinding_Shutdown()` 和 `VFXBinding_Shutdown()` 是手动调用，非 RAII

### 2.5 System 模块 (`src/System/`)

**构建文件**: 2 cpp + 2 h

| 组件 | 文件 | 功能 |
|------|------|------|
| SaveManager | `SaveManager.h/.cpp` | 存档系统单例：JSON 序列化 (纯字符串拼接，无第三方库)、元数据包装 (slot/timestamp/sceneName/thumbnail/tokenIndex/schemaVersion)、版本迁移链 (fromVersion→toVersion 映射)、文件 I/O (readFile/writeFile)。内置 jsonEscape/jsonBuildObj/jsonGetString/jsonGetInt 静态工具。 |
| SaveBinding | `SaveBinding.h/.cpp` | KAG 存档 Lua 绑定：KAG.save_game/load_game/list_saves/delete_save。 |

**公共接口边界**: SaveManager → Engine (init) + SaveBinding → LuaManager (registerModules)

**依赖**: 无外部依赖（纯 STD + 文件 I/O）

**技术债务**:
- JSON 解析器为手工实现的正则/字符扫描，不支持嵌套对象/数组，未来数据复杂化时会成为瓶颈
- 迁移链是线性版本递增 (1→2→3...) ，若中间断链则无法恢复

### 2.6 Carc 模块 (`src/Carc/`)

**构建文件**: 5 cpp + 6 h

| 组件 | 文件 | 功能 |
|------|------|------|
| CARCFormat | `CARCFormat.h` | CARC 容器格式定义：CARC_MAGIC/CARC_VERSION、CARCHeader (64B packed)、FileEntry (116B packed)、AES-256-GCM 密钥/Nonce/Tag 常量。 |
| CryptoEngine | `CryptoEngine.h/.cpp` | Windows BCrypt 加密引擎：AES-256-GCM 加解密、SHA-256、Ed25519 签名/验证 (orlp 库)、随机密钥生成、密钥对生成 + 文件 I/O。 |
| CARCReader | `CARCReader.h/.cpp` | CARC 读取器：Header 解析→索引解密→文件查找 (SHA-256 路径哈希)→解密解压→Ed25519 签名验证→链信任验证 (CRL)。 |
| CARCWriter | `CARCWriter.h/.cpp` | CARC 写入器：目录→CARC 打包、AES-256-GCM 加密 + Ed25519 签名 + miniz 压缩。 |
| CarcAssetProvider | `CarcAssetProvider.h/.cpp` | IAssetProvider 适配器：包装 CARCReader 为统一资源接口，priority=10。 |
| CRLManager | `CRLManager.h/.cpp` | 证书吊销列表管理器：Offline/Online/Hybrid 三模式、SHA-256 指纹集合、CRL JSON 解析/生成（含签名验证）、本地缓存。 |

**公共接口边界**:
- CARCReader/CARCWriter → main.cpp (启动验证) + CarcAssetProvider → ProviderChain
- CryptoEngine → CARCReader/CARCWriter (内部依赖)
- CRLManager → CARCReader (链信任验证)

**依赖**: BCrypt (Windows)、ed25519 (orlp)、miniz、zstd

**技术债务**: 
- `CryptoEngine` 全静态方法类，无状态，但 Windows-only (BCrypt)
- CRL 的 `fetchOnline()` 方法签名有 URL 参数但内部使用 `m_onlineURL`，不一致

### 2.7 Resource 模块 (`src/Resource/`)

**构建文件**: 4 cpp + 5 h

| 组件 | 文件 | 功能 |
|------|------|------|
| IAssetProvider | `IAssetProvider.h` | 抽象资源提供者接口：read/exists/getSource/priority/verify。 |
| DirAssetProvider | `DirAssetProvider.h/.cpp` | 文件系统目录提供者：priority=5。 |
| ProviderChain | `ProviderChain.h/.cpp` | 优先级排序的提供者链：addProvider 自动排序、read 依次尝试、exists 短路。 |
| XP3Archive | `XP3Archive.h/.cpp` | Kirikiri XP3 打包/解包器：zlib 压缩段、UTF-16LE 文件名索引、pack/unpack/list 静态方法。 |
| AsyncLoader | `AsyncLoader.h/.cpp` | 异步加载器：单后台线程 + SDL 事件通知、16 请求上限队列、取消支持、完成回调轮询。 |

**公共接口边界**:
- ProviderChain → Engine + main.cpp (资源加载)
- IAssetProvider → CarcAssetProvider (Carc) + DirAssetProvider
- AsyncLoader → Engine (processEvents 轮询)

**依赖**: zstd、miniz、SDL3 (事件)

**技术债务**: 无明显标记

### 2.8 Debug 模块 (`src/Debug/`)

**构建文件**: 2 cpp + 2 h

| 组件 | 文件 | 功能 |
|------|------|------|
| HotReload | `HotReload.h/.cpp` | 文件监视热重载：扫描 .ks/.lua 文件、记录 last_write_time、帧级检测→取消操作→coroutine.close()→GameState 重置→重载脚本→显示警告覆盖。 |
| DebugProtocol | `DebugProtocol.h/.cpp` | Lua 断点调试协议：lua_sethook + LUA_MASKLINE、断点集合 ("file:line")、stepOver/stepInto/stepOut/continue、局部/全局变量 inspect、阻塞等待循环。 |

**公共接口边界**: HotReload → Engine (init/checkAndReload) + ErrorUI (requestReload)；DebugProtocol → Scripting (DebugBinding)

**依赖**: Lua (lua_sethook/lua_Debug)、filesystem

**技术债务**:
- HotReload 的 `waitForCommand()` 使用 sleep 50ms 轮询而非条件变量
- DebugProtocol 无网络/远程调试透传（仅本地）

### 2.9 Platform 模块 (`src/Platform/`)

**构建文件**: 1 cpp + 1 h

| 组件 | 文件 | 功能 |
|------|------|------|
| MobileAdapter | `MobileAdapter.h/.cpp` | P2 预留移动端适配存根：生命周期回调 (onPause/onResume)、触摸→鼠标映射 (onFingerDown/Motion/Up)、手势输入 (onPinch/onLongPress)、DP缩放。全部方法为占位实现。 |

**公共接口边界**: 预留，当前无调用方

**依赖**: Lua (回调)

**技术债务**:
- 6 处 `// TODO:` 注释，全部为 "未连接" 的占位实现
- namespace 为 `caesura::platform` 而非 `Caesura`，与其他模块不一致

---

## 3. 依赖关系图谱

```
                    ┌─────────────┐
                    │   main.cpp  │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │   Engine    │ (Core)
                    └──┬──┬──┬──┬─┘
          ┌────────────┤  │  │  ├────────────┐
          ▼            ▼  │  ▼               ▼
  ┌────────────┐ ┌──────────┐ ┌──────┐ ┌───────────┐
  │SDL3Platform│ │BgfxRender│ │SoLoud│ │LuaManager │
  │  Backend   │ │  Device  │ │Audio │ │(Scripting)│
  └──────┬─────┘ └────┬─────┘ └──┬───┘ └─────┬─────┘
         │            │           │           │
         │    ┌───────┼───────────┼───────────┼────────┐
         └────► BackendRegistry ◄─┘           │        │
              └───────┼───────────┬───────────┼────────┘
                      │           │           │
        ┌─────────────┼───────┬───┴───────┬───┼─────────────┐
        ▼             ▼       ▼           ▼   ▼             ▼
  InputRouter   VideoPlayer  GpuMonitor  SaveManager   ProviderChain
                                     │                  │
                               SaveBinding    ┌──────┐ │ ┌────────┐
                                              │ Carc │ │ │DirAsset│
                                              │AssetPr│ │ │Provider│
                                              └──────┘ │ └────────┘
                                                   ▲   │
                                              CARCReader│
                                                   ▲   │
                                              CryptoEng│
                                                   ▲   │
                                              ed25519 │
```

**外部依赖 (全静态编译)**:
| 库 | 用途 | 链接方式 |
|----|------|---------|
| SDL3 | 窗口+事件+定时 | 动态 DLL (find_package) |
| bgfx + bimg + bx | GPU渲染 | 静态库 |
| SoLoud | 音频引擎 | 静态库 |
| Lua 5.4 | 脚本VM | 静态库 |
| FreeType 2 | 字体光栅化 | 静态库 (禁用 PNG/HarfBuzz/Brotli) |
| zstd | CARC 解压 | 静态库 |
| ed25519 | 数字签名 | 静态库 |
| miniz | 压缩 (来自 bimg) | 静态库 |
| pl_mpeg | MPEG1 视频解码 | 单头文件内嵌 |
| stb_image | 图像加载 | 单头文件内嵌 |
| BCrypt | Windows 加密 | 系统库 |
| D3D11/DXGI/D3DCompiler | Windows 图形 | 系统库 |

---

## 4. 技术债务汇总

### 4.1 高优先级

| ID | 位置 | 类别 | 描述 |
|----|------|------|------|
| TD-01 | `Core/Engine.cpp:render()` | 架构 | 每帧两次独立 Lua pcall (engine_render + Engine_OnFrameRender)，应合并为单次 dispatch |
| TD-02 | `Render/TextRenderer` ↔ `Render/FontRenderer` | 重复 | 两个类都集成 FreeType + TTF，功能高度重叠；TextRenderer 有 cursor/newline 管理但 FontRenderer 有 batch cache + CJK 回退，应合并 |
| TD-03 | `Core/BackendRegistry` | 生命周期 | 同时管理 raw ptr + unique_ptr，Engine 也持有 unique_ptr，罕见的三重所有权 |
| TD-04 | `Core/DebugManager` | 冗余 | `recursive_mutex` 但所有调用路径在主线程，锁开销无意义 |
| TD-05 | `Scripting/LuaManager.cpp` | 可维护性 | `lockdownScriptEnv()` 内联大量 Lua 代码字符串，应抽到独立 sandbox.lua 文件 |
| TD-06 | `Render/ParticleSystem` | 耦合 | 依赖 `BgfxRenderDevice*` 裸指针而非 `IRenderDevice*` 抽象 |
| TD-07 | `Carc/CryptoEngine` | 可移植性 | Windows-only (BCrypt)，无 Linux/macOS 回退路径 |
| TD-08 | `System/SaveManager` | 健壮性 | 手写 JSON 解析器仅支持扁平键值对，不支持嵌套 |

### 4.2 中优先级

| ID | 位置 | 类别 | 描述 |
|----|------|------|------|
| TD-09 | `Debug/HotReload` | 性能 | `waitForCommand()` 使用 sleep(50ms) 轮询而非条件变量 |
| TD-10 | `Render/RTTManager.cpp` | 安全性 | 6 处 `assert(isMainThread())` 被注释为 `@Beta` |
| TD-11 | `Render/FontRenderer.cpp:175` | 配置化 | `@Beta: Replace hardcoded 2048x2048 atlas threshold` |
| TD-12 | `Render/IVideoDecoder.h` | 未完成 | `@Beta: FFmpeg-based video decoder planned for Beta`，仅接口无实现 |
| TD-13 | `Render/BgfxRenderDevice.cpp:938` | 优化 | `@Beta — Pre-bake rule images into a LUT texture atlas` |
| TD-14 | `Platform/MobileAdapter` | 占位 | 6 处 TODO，全部为移动端存根 |
| TD-15 | `Scripting/UnifiedBinding` ↔ `KAGBinding` | 重复 | show_text 等便捷方法在两层各实现一次 |

### 4.3 低优先级

| ID | 位置 | 类别 | 描述 |
|----|------|------|------|
| TD-16 | `Core/DebugManager.h` | 配置 | `CAESURA_DEBUG` 始终定义为 1，无 Release 模式剥离 |
| TD-17 | `Platform/MobileAdapter` | 一致性 | namespace `caesura::platform` 与其他模块的 `Caesura` 不一致 |
| TD-18 | `Carc/CRLManager` | 接口 | `fetchOnline(url)` 参数冗余（内部使用 m_onlineURL） |

---

## 5. 构建配置分析

### 5.1 CMake 构建体系

- **项目**: `CaesuraAmeKAG` v1.0.0, C++20, MSVC (Visual Studio)
- **源码**: 64 个 cpp/h 文件（含 2 个纯接口头），编译为单可执行文件
- **预定义宏**: `SDL_MAIN_HANDLED`, `_CRT_SECURE_NO_WARNINGS`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `BX_CONFIG_DEBUG`
- **MSVC 编译选项**: /W3 /wd4100 /wd4189 /wd4244 /Zc:__cplusplus /Zc:preprocessor /utf-8 /FS
- **Windows 链接**: winmm, ws2_32, psapi, ole32, d3d11, dxgi, d3dcompiler, bcrypt
- **Apple 链接** (预留): Cocoa, IOKit, Metal, QuartzCore
- **Linux 链接** (预留): pthread, dl, X11

### 5.2 可选功能

| 选项 | 默认值 | 用途 |
|------|--------|------|
| `CAESURA_VIDEO_FFMPEG` | OFF | FFmpeg 视频解码后端 |
| `CAESURA_DEBUG` | 1 (强制) | 调试日志宏启用 |

### 5.3 子构建

- `tools/carc_pack` → CARC 打包命令行工具
- `tests/` → 单元测试（Catch2 + Sol2）

---

## 6. 总体评估

### 优势
- 接口抽象设计良好：IPlatformBackend / IAudioBackend / IRenderDevice / IAssetProvider 均支持多态替换
- 后端注册表模式解耦了 C++ 内核与 Lua 脚本层
- 沙盒安全模型完善：Lua 环境锁定 + 文件 I/O 禁用 + require 白名单
- 错误恢复链设计精巧：双级 ErrorUI + 智能重试
- 6 级自适应纹理预算 + 3 级 GPU 降级策略

### 待改进
- TextRenderer / FontRenderer 功能重叠需合并
- BackendRegistry 三层所有权模型需简化
- 6 个 @Beta 标记 + 6 个 TODO 占位需推进
- 手写 JSON 解析器未来可能成为约束
- Windows-only 加密路径需跨平台抽象
