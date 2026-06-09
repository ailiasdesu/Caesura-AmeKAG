# Caesura (AmeKAG) 引擎代码库深度分析

> 分析日期: 2026-06-09 | 构建配置: CMake 3.25+ / C++20
> 构建状态: Windows Debug + Release 全量通过，D3D11 零 TDR 稳定
> 最近提交: bd22a86 — Electron 编辑器完善 (E1-E8)

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
| C++ 文件 | 63 .cpp + 73 .h = 136 个编译单元 |
| C++ 代码量 | ~17,000 行 |
| Lua 脚本 | 44 个文件 / ~8,500 行 |
| C++ 单元测试 | 24 个文件 |
| Electron 编辑器 | 11 组件 + 3 AI provider + 1 解析器 |
| KAG 命令 | 61 个（6 子模块: layer/text/audio/system/transition/video） |
| MiniGame API | 15 个 Lua 函数（几何体/材质/光照/碰撞/物理） |
| 纯虚接口 | 8 个: IRenderDevice, IAudioBackend, IPlatformBackend, IAssetProvider, IAnimationBackend, ILive2DRenderPath, IMiniGameBackend, IVideoDecoder |
| 外部库 | 13 个全部静态编译 |
| 模块 | 13 个（含 EditorServer） |

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
├── System/        ← SaveManager (JSON + AES-256-GCM + ISaveProvider + Schema v1→v5)
├── Carc/          ← CryptoEngine (BCrypt/OpenSSL), CARCReader/Writer, CRLManager, DeltaCARC
├── Resource/      ← AssetManager, AsyncLoader, ProviderChain, XP3Archive, ImageDecoder
├── Animation/     ← Live2D Backend (Cubism 5, 4 渲染路径, CAESURA_HAS_LIVE2D)
├── MiniGame/      ← IMiniGameBackend + BgfxMiniGameBackend (PBR-lite, 15 Lua API)
├── Debug/         ← HotReload + DebugProtocol
├── Editor/        ← EditorServer (JSON-RPC stdin/stdout, 7 methods)
└── Platform/      ← MobileAdapter (存根)

web-editor/
├── electron/      ← main.js (engine spawn + AI chat bridge) + preload.js (IPC)
├── src/           ← React 18 组件 + Context + 解析器 + AI providers
└── dist/          ← Vite 构建输出 (168KB JS + 6.7KB CSS)
```

---

## 3. 模块详解

### 3.1 Core（10 .cpp + 12 .h）
Engine 主循环、BackendRegistry 单例工厂、InputRouter 输入路由、DebugManager 双级 ErrorUI、JobSystem 多线程调度、SandboxQuota 指令预算、TextureBudget 6 级、RpcServer JSON-RPC (ping/run/eval/stop/assets/getState/logs)

### 3.2 Render（12 .cpp + 13 .h）
BgfxRenderDevice (IRenderDevice)、LayerManager 多层合成、TextRenderer (FreeType + CJK atlas)、TextureManager 生命周期、RTTManager 渲染到纹理、ShaderCache 5 级、ParticleSystem (多线程 JobSystem)、GpuMonitor TDR 防护、VideoPlayer (pl_mpeg + FFmpeg 条件编译)

### 3.3 Audio（2 .cpp + 2 .h）
SoLoudAudioEngine (BGM/VOICE/SE 三通道, fade, 3D positional)

### 3.4 Scripting（8 .cpp + 8 .h）
LuaManager 沙箱、KAGBinding 35 C 函数 + 61 KAG 命令、RenderBinding (31 函数 + 8 视频)、VFXBinding 粒子、DevCoreBinding、DebugBinding、UnifiedBinding (_CAESURA_BACKEND 统一入口)

### 3.5 System（2 .cpp + 2 .h）
SaveManager (JSON + AES-256-GCM 加密 + ISaveProvider 可替换后端 + v1→v5 Schema 迁移 + F5/F6 快速存档 + 自动存档)

### 3.6 Carc（6 .cpp + 7 .h）
CryptoEngine (AES-256-GCM: Win BCrypt / 其他 OpenSSL EVP, SHA-256, Ed25519)、CARCReader/Writer、CRLManager 证书撤销、**DeltaCARC** 差分更新 (generate/apply/verify)

### 3.7 Resource（6 .cpp + 8 .h）
AssetManager、AsyncLoader 多线程异步加载、ProviderChain (Dir → XP3 → CARC 优先级递减)、XP3Archive KiriKiri 兼容、ImageDecoder

### 3.8 Animation（6 .cpp + 9 .h）
Live2D Cubism 5 (CAESURA_HAS_LIVE2D 条件编译)、ILive2DRenderPath 4 路径：D3D11Native ✅ / OpenGLShared ⚠️ 移交 / MetalNative ⚠️ 移交 / OpenGLReadback ⚠️ 移交

### 3.9 MiniGame（2 .cpp + 3 .h）
BgfxMiniGameBackend (PBR-lite: roughness/metallic/specular + 3 光源类型 + 碰撞检测 + 物理)、15 Lua 函数、debug wireframe 回退

### 3.10 Debug（2 .cpp + 2 .h）
HotReload (Lua 文件监控)、DebugProtocol

### 3.11 Editor（1 .cpp + 1 .h）
EditorServer JSON-RPC (stdin/stdout)，7 个 RPC 方法，Electron IDE 后端完全就绪

### 3.12 Platform（1 .cpp + 1 .h）
MobileAdapter (存根，触屏事件注入已预埋)

### 3.13 Electron 编辑器（11 组件 + 3 providers + 1 parser）
舞台视图 (Canvas 2D + 拖入素材 + 网格)、时间线 (事件可视化 + 累计时间轴)、属性面板 (代码双向同步)、AI 面板 (@generate/@fix + 多后端)、素材浏览器、日志面板、设置对话框

---

## 4. 技术债务追踪

| ID | 描述 | 状态 |
|----|------|:---:|
| TD-01~TD-10 | BackendRegistry 所有权/沙箱/工厂/接口 | ✅ 闭合 |
| TD-11 | Live2D OpenGLSharedRenderPath | ⚠️ 移交 Linux 开发者 |
| TD-12 | CryptoEngine 跨平台 | ✅ BCrypt/OpenSSL 已就位 |
| TD-13 | CJK 缓存 | ✅ 闭合 |
| TD-14 | CARC 差分更新 | ✅ DeltaCARC 实现 |
| TD-15 | pl_mpeg → FFmpeg | ✅ VideoPlayer 双后端 + Lua 绑定 |
| TD-16~TD-18 | 调试/键名/CRL 小修 | ✅ 闭合 |
| TD-19 | Live2D Metal | ⚠️ 移交 macOS 开发者 |
| TD-20 | MobileAdapter | ⚠️ 存根（触屏事件已预埋） |
| TD-21 | MiniGame 3D | ✅ BgfxMiniGameBackend PBR-lite |
| TD-22 | 跨平台 CI | ✅ YAML 修复 + Linux 配置 |
| TD-23 | BgfxMiniGameBackend 测试链接 | ⚠️ 预存问题 |

**摘要**: 19/23 闭合，4 开放（移交/存根/预存）

---

## 5. 多线程架构

```
主线剧情（单线程）: processEvents → engine_update → beginFrame → engine_render → endFrame
辅助多线程（JobSystem）: AsyncLoader | ParticleSim | CARC Decrypt | CPU Tasks | Video Decode
```

---

## 6. 平台支持

| 平台 | 渲染后端 | 构建 | Live2D | 备注 |
|------|----------|:---:|:---:|------|
| Windows | D3D11 | ✅ | ✅ | 零 TDR |
| Linux | OpenGL | ⚠️ CI | ⚠️ 移交 | OpenSSL 依赖已配置 |
| macOS | Metal | ⚠️ CI | ⚠️ 移交 | OpenSSL 依赖已配置 |

---

## 7. Live2D 渲染路径

| 路径 | Win D3D11 | Linux GL | macOS Metal |
|------|:---:|:---:|:---:|
| D3D11Native | ✅ | — | — |
| OpenGLShared | — | ⚠️ 待实现 | — |
| MetalNative | — | — | ⚠️ 待验证 |
| OpenGLReadback | — | — | — |

---

## 8. Electron 编辑器

| 组件 | 文件 | 功能 |
|------|------|------|
| StageView | components/StageView.jsx | Canvas 2D 舞台 + 拖入素材 + 安全区 |
| AssetPanel | components/AssetPanel.jsx | 素材浏览 + 拖拽源 |
| Timeline | components/Timeline.jsx | 时间线事件可视化 |
| PropertyPanel | components/PropertyPanel.jsx | 属性编辑 + KAG 代码生成 |
| AIPanel | components/AIPanel.jsx | AI 对话 + @generate/@fix |
| SettingsDialog | components/SettingsDialog.jsx | AI 后端设置 |
| EditorContext | context/EditorContext.jsx | useReducer 全局状态 |
| kagParser | parser/kagParser.js | KAG Lua 正则解析 + 代码生成 |
| OpenAI/Codex/Custom | providers/ | AI 多后端 |
| Electron main | electron/main.js | 引擎 spawn + AI IPC |
| Electron preload | electron/preload.js | contextBridge IPC |

### RPC 方法
`ping` → 健康检查  
`run` → 执行 Lua 脚本  
`eval` → 评估表达式  
`stop` → 停止场景  
`assets` → 列出素材  
`getState` → 引擎状态  
`logs` → 日志缓冲区

---

## 9. 总体评估

### 完成度: ~90% (Alpha+)

### 优势
- 8 个纯虚接口，核心约束全面合规
- BackendRegistry 单例解耦 + 沙箱安全
- 61 KAG 命令 + 15 MiniGame API + 8 视频函数
- 多线程 JobSystem 覆盖粒子/CARC/纹理/视频解码
- D3D11 零 TDR + 双级 ErrorUI + 智能重试
- DeltaCARC 差分更新 + AES-256-GCM 存档加密
- BgfxMiniGameBackend PBR-lite 3D 小游戏
- Electron 可视化编辑器 + AI 辅助面板
- MIT 许可 + Live2D 条件编译无法律风险

### 移交共同开发者
- Live2D OpenGLShared + Metal（需实际 Linux/macOS 环境）
- 移动端适配（需真机）
- 跨平台 CI 测试启用（需非 GPU 测试桩）

### 下一阶段
- 编辑器 AI 上下文优化（token 预算调优）
- MiniGame shader 编译（替换 debug wireframe）
- FFmpeg 默认启用
- 可视化编辑器 → 引擎 RPC 帧预览