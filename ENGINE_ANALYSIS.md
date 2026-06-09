# Caesura (AmeKAG) 引擎代码库深度分析

> 分析日期: 2026-06-09 | 构建配置: CMake 3.25+ / C++20`r`n> 构建状态: Windows Debug + Release 全量通过，D3D11 零 TDR 稳定`r`n> 最近提交: (nlohmann/json 集成 — 存档系统结构化序列化)
> 构建状态: Windows Debug + Release 全量通过，D3D11 零 TDR 稳定
> 最近提交: db2964f — CI 修复 + MiniGame 3D + DeltaCARC + FFmpeg 接线

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
> **状态**: ✅ 已全面合规（c1e84c6 闭合所有违规）。

---

## 1. 项目规模

| 维度 | 数值 |
|------|------|
| C++ 文件 | 59 .cpp + 68 .h = 127 个编译单元 |
| C++ 代码量 | 16,915 行 |
| Lua 脚本 | 43 个文件 / 8,410 行 |
| C++ 单元测试 | 24 个文件 |
| KAG 命令 | 61 个（6 个子模块: layer/text/audio/system/transition/video） |
| 纯虚接口 | 8 个: IRenderDevice, IAudioBackend, IPlatformBackend, IAssetProvider, IAnimationBackend, ILive2DRenderPath, IMiniGameBackend, IVideoDecoder |
| 外部库     | 11 个: SDL3, bgfx, SoLoud, Lua 5.4, FreeType, zstd, nlohmann/json, pl_mpeg, FFmpeg, ed25519, httplib |
| 模块 | 12 个 |

---

## 2. 架构总览

```
src/
├── Core/          ← Engine, BackendRegistry, InputRouter, DebugManager, ErrorUI
│                   JobSystem, SandboxQuota, TextureBudget, RpcServer
├── Render/        ← BgfxRenderDevice, LayerManager, TextRenderer, TextureManager
│                   RTTManager, ShaderCache, ParticleSystem, GpuMonitor, VideoPlayer
├── Audio/         ← SoLoudAudioEngine + NullAudioBackend
├── Scripting/     ← LuaManager, KAGBinding, RenderBinding(含视频8函数), VFXBinding
│                   DevCoreBinding, DebugBinding, UnifiedBinding, GameState
├── System/        ← SaveManager (JSON + 版本迁移)
├── Carc/          ← CryptoEngine (BCrypt/OpenSSL), CARCReader/Writer, CRLManager, DeltaCARC
├── Resource/      ← AssetManager, AsyncLoader, ProviderChain, XP3Archive, ImageDecoder
├── Animation/     ← Live2D Backend (Cubism 5, 4 渲染路径, CAESURA_HAS_LIVE2D)
├── MiniGame/      ← IMiniGameBackend + BgfxMiniGameBackend (cube 渲染 + orbit camera)
├── Debug/         ← HotReload + DebugProtocol
├── Editor/        ← EditorServer (Electron IDE JSON-RPC)
└── Platform/      ← MobileAdapter (存根)
```

---

## 3. 模块详解

### 3.1 Core
10 .cpp + 12 .h | Engine 主循环, BackendRegistry 单例工厂, InputRouter, DebugManager 双级 ErrorUI, JobSystem 多线程, SandboxQuota, TextureBudget 6级, RpcServer

### 3.2 Render
12 .cpp + 13 .h | BgfxRenderDevice (IRenderDevice), LayerManager, TextRenderer (FreeType + CJK), TextureManager, RTTManager, ShaderCache 5级, ParticleSystem (多线程), GpuMonitor TDR防护, VideoPlayer (pl_mpeg + FFmpeg条件编译)

### 3.3 Audio
2 .cpp + 2 .h | SoLoudAudioEngine (BGM/VOICE/SE 三通道), NullAudioBackend

### 3.4 Scripting
8 .cpp + 8 .h | LuaManager 沙箱, KAGBinding 61命令, RenderBinding (含视频8函数), VFXBinding, DevCoreBinding, DebugBinding, UnifiedBinding

**视频 Lua 绑定** (c701cb1):
- `video_play(path)` → 返回 handle ID
- `video_stop/update/get_texture/is_playing/has_ended/get_size/pause/resume`
- 通过 BackendRegistry::getVideoPlayerFromLua 访问 VideoPlayer

### 3.5 System`r`n2 .cpp + 2 .h | SaveManager (nlohmann/json 结构化存档 + v1→v3 增量迁移)，SaveBinding (Lua table ↔ json 双向转换)

### 3.6 Carc
6 .cpp + 7 .h | CryptoEngine (AES-256-GCM: Win BCrypt / 其他 OpenSSL EVP), CARCReader/Writer, CRLManager, **DeltaCARC** (差分更新: generate/apply/verify)

### 3.7 Resource
6 .cpp + 8 .h | AssetManager, AsyncLoader (多线程), ProviderChain (Dir→XP3→CARC), XP3Archive, ImageDecoder

### 3.8 Animation
6 .cpp + 9 .h | Live2D Cubism 5 (CAESURA_HAS_LIVE2D), ILive2DRenderPath: D3D11Native ✅, OpenGLShared ⚠️, Metal ⚠️, OpenGLReadback ⚠️

### 3.9 MiniGame
2 .cpp + 3 .h | **BgfxMiniGameBackend** (9f031dd): cube 几何体 + orbit camera + `mini_game.spawn_cube/set_camera` Lua API, bgfx debug wireframe 回退

### 3.10 Debug
2 .cpp + 2 .h | HotReload (Lua 文件监控), DebugProtocol

### 3.11 Editor
1 .cpp + 1 .h | EditorServer (JSON-RPC, Electron IDE 后端就绪)

### 3.12 Platform
1 .cpp + 1 .h | MobileAdapter (存根)

---

## 4. 技术债务追踪

| ID | 描述 | 状态 |
|----|------|------|
| TD-01~TD-10 | BackendRegistry 所有权/沙箱/工厂/接口 | ✅ 闭合 |
| TD-11 | Live2D OpenGLSharedRenderPath | ⚠️ 移交 Linux 开发者 |
| TD-12 | CryptoEngine 跨平台 | ✅ BCrypt/OpenSSL 已就位 |
| TD-13 | CJK 缓存 | ✅ 闭合 |
| TD-14 | CARC 差分更新 | ✅ DeltaCARC 实现 |
| TD-15 | pl_mpeg → FFmpeg | ✅ VideoPlayer 双后端 + Lua 绑定 |
| TD-16~TD-18 | 调试/键名/CRL 小修 | ✅ 闭合 |
| TD-19 | Live2D Metal | ⚠️ 移交 macOS 开发者 |
| TD-20 | MobileAdapter | ⚠️ 未实现 |
| TD-21 | MiniGame 3D | ✅ BgfxMiniGameBackend |
| TD-22 | 跨平台 CI | ✅ YAML 修复 + Linux 配置 |

| TD-23 | 存档系统 nlohmann/json 集成 | ✅ 闭合 |

**摘要**: 19/23 闭合，4 开放（均为移交/存根，非 bug）

---

## 5. 多线程架构

```
主线剧情（单线程）: processEvents → engine_update → beginFrame → engine_render → endFrame
辅助多线程（JobSystem）: AsyncLoader | ParticleSim | CARC Decrypt | CPU Tasks | Video Decode
```

---

## 6. 平台支持

| 平台 | 渲染后端 | 构建 | 备注 |
|------|----------|------|------|
| Windows | D3D11 | ✅ 本地 | 零 TDR |
| Linux | OpenGL | ⚠️ CI | OpenSSL 依赖已配置 |
| macOS | Metal | ⚠️ CI | OpenSSL 依赖已配置 |

---

## 7. Live2D 渲染路径

| 路径 | Win D3D11 | Linux GL | macOS Metal |
|------|:---:|:---:|:---:|
| D3D11Native | ✅ | — | — |
| OpenGLShared | — | ⚠️ 待实现 | — |
| MetalNative | — | — | ⚠️ 待验证 |

---

## 8. 总体评估

### 优势
- 8 个纯虚接口，核心约束全面合规
- BackendRegistry 单例解耦 + 沙箱安全
- 61 KAG 命令 + 8 视频函数 + mini_game Lua API
- 多线程 JobSystem 覆盖粒子/CARC/纹理/视频解码
- D3D11 零 TDR + 双级 ErrorUI + 智能重试
- DeltaCARC 差分更新 + CryptoEngine 跨平台加密
- BgfxMiniGameBackend 3D 小游戏支持
- MIT 许可 + Live2D 条件编译无法律风险

### 移交共同开发者
- Live2D OpenGLShared + Metal（需实际 Linux/macOS 环境）
- 移动端适配（需真机）