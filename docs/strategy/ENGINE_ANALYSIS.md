# Caesura (AmeKAG) 引擎代码库深度分析

> 分析日期: 2026-06-09 | 构建配置: CMake 3.25+ / C++20
> 构建状态: Windows Debug + Release 全量通过，D3D11 零 TDR 稳定
> 最近提交: 527aae0 — Live2D GL FBO + MiniGame 着色器 + FFmpeg CMake + AI 面板真实 API

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
| C++ 文件 | 64 .cpp + 73 .h = 137 个编译单元 |
| C++ 代码量 | ~18,000 行 |
| Lua 脚本 | 44 个文件 / ~8,500 行 |
| C++ 单元测试 | 24 个文件 |
| Electron 编辑器 | 11 组件 + 3 AI provider + 1 解析器 |
| KAG 命令 | 61 个（7 子模块: layer/text/audio/system/transition/vfx/video） |
| MiniGame API | 15 个 Lua 函数（几何体/材质/光照/碰撞/物理） |
| 纯虚接口 | 8 个: IRenderDevice, IAudioBackend, IPlatformBackend, IAssetProvider, IAnimationBackend, ILive2DRenderPath, IMiniGameBackend, IVideoDecoder |
| 外部库 | 13 个全部静态编译 |
| 模块 | 10 个（精简重组） |

---

## 2. 架构总览

```
src/
├── Core/          ← Engine, BackendRegistry, InputRouter, RpcServer
│                   JobSystem, SandboxQuota, TextureBudget, MobileAdapter
├── Render/        ← BgfxRenderDevice, LayerManager, TextRenderer, TextureManager
│                   RTTManager, ShaderCache, ParticleSystem, GpuMonitor, VideoPlayer
│                   EmbeddedShaders_MiniGame (D3D11/GLSL/Metal 跨平台)
├── Audio/         ← SoLoudAudioEngine + NullAudioBackend
├── Scripting/     ← LuaManager, KAGBinding, RenderBinding(含视频8函数), VFXBinding
│                   DevCoreBinding, DebugBinding, UnifiedBinding, GameState
├── System/        ← SaveManager (JSON + AES-256-GCM + ISaveProvider + Schema v1→v5)
├── CARC/          ← CryptoEngine (BCrypt/OpenSSL), CARCReader/Writer, CRLManager, DeltaCARC
├── Resource/      ← AssetManager, AsyncLoader, ProviderChain, XP3Archive, ImageDecoder
├── Live2D/        ← Cubism 5 (CAESURA_HAS_LIVE2D), 4 渲染路径 + OpenGLReadback FBO
├── MiniGame/      ← IMiniGameBackend + BgfxMiniGameBackend (PBR-lite, 15 Lua API)
└── Debug/         ← HotReload + DebugProtocol + DebugManager (从 Core 迁入)

web-editor/
├── electron/      ← main.cjs (engine spawn + AI IPC bridge) + preload.cjs (IPC)
├── src/           ← React 18 组件 (11) + Context + 解析器 + AI providers (3)
└── dist/          ← Vite 构建输出
```

---

## 3. 模块详解

### 3.1 Core（11 .cpp + 13 .h）
Engine 主循环、BackendRegistry 单例工厂、InputRouter 输入路由、JobSystem 多线程调度、SandboxQuota 指令预算、TextureBudget 6 级、RpcServer JSON-RPC (ping/run/eval/stop/assets/getState/getFrame/logs)、MobileAdapter（存根，触屏事件预埋）

### 3.2 Render（15 .cpp + 13 .h）
BgfxRenderDevice (IRenderDevice)、LayerManager 多层合成、TextRenderer (FreeType + CJK atlas)、TextureManager 生命周期、RTTManager 渲染到纹理、ShaderCache 5 级、ParticleSystem (多线程 JobSystem)、GpuMonitor TDR 防护、VideoPlayer (pl_mpeg + FFmpeg 条件编译)、EmbeddedShaders_MiniGame_GLSL/Metal 跨平台着色器

### 3.3 Audio（2 .cpp + 2 .h）
SoLoudAudioEngine (BGM/VOICE/SE 三通道, fade, 3D positional)

### 3.4 Scripting（8 .cpp + 8 .h）
LuaManager 沙箱、KAGBinding 35 C 函数 + 61 KAG 命令、RenderBinding (31 函数 + 8 视频)、VFXBinding 粒子、DevCoreBinding、DebugBinding、UnifiedBinding (_CAESURA_BACKEND 统一入口)

### 3.5 System（3 .cpp + 3 .h）
SaveManager (JSON + AES-256-GCM 加密 + ISaveProvider 可替换后端 + v1→v5 Schema 迁移 + F5/F6 快速存档 + 自动存档)

### 3.6 CARC（6 .cpp + 7 .h）
CryptoEngine (AES-256-GCM: Win BCrypt / 其他 OpenSSL EVP, SHA-256, Ed25519)、CARCReader/Writer、CRLManager 证书撤销、**DeltaCARC** 差分更新 (generate/apply/verify)

### 3.7 Resource（6 .cpp + 8 .h）
AssetManager、AsyncLoader 多线程异步加载、ProviderChain (Dir → XP3 → CARC 优先级递减)、XP3Archive KiriKiri 兼容、ImageDecoder

### 3.8 Live2D（6 .cpp + 9 .h）
Cubism 5 (CAESURA_HAS_LIVE2D 条件编译)、ILive2DRenderPath 4 路径：D3D11Native ✅ / OpenGLShared ⚠️ 移交 / MetalNative ⚠️ 移交 / OpenGLReadback ✅ (FBO + GL状态保存恢复)

### 3.9 MiniGame（4 .cpp + 7 .h）
BgfxMiniGameBackend (PBR-lite: roughness/metallic/specular + 3 光源类型 + 碰撞检测 + 物理)、15 Lua 函数、跨平台 shader (D3D11 DXBC / OpenGL GLSL / Metal MSL 运行时选择)

### 3.10 Debug（3 .cpp + 3 .h）
DebugManager 双级 ErrorUI + TDR 防护监控、HotReload (Lua 文件监控)、DebugProtocol

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
| TD-21 | MiniGame 3D | ✅ BgfxMiniGameBackend PBR-lite + 跨平台 shader |
| TD-22 | 跨平台 CI | ✅ YAML 修复 + Linux 配置 |
| TD-23 | BgfxMiniGameBackend 测试链接 | ⚠️ 预存问题 |
| TD-24 | Live2D OpenGLReadback | ✅ FBO + GL 状态保存恢复 |

**摘要**: 20/24 闭合，4 开放（移交/存根/预存）

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
| Windows | D3D11 | ✅ | ✅ | 零 TDR, D3D11Native ✅ |
| Linux | OpenGL | ⚠️ CI | ⚠️ 移交 | OpenSSL 依赖已配置 |
| macOS | Metal | ⚠️ CI | ⚠️ 移交 | OpenSSL 依赖已配置 |

---

## 7. Live2D 渲染路径

| 路径 | Win D3D11 | Linux GL | macOS Metal |
|------|:---:|:---:|:---:|
| D3D11Native | ✅ | — | — |
| OpenGLShared | — | ⚠️ 待实现 | — |
| MetalNative | — | — | ⚠️ 待验证 |
| OpenGLReadback | ✅ FBO | — | — |

---

## 8. Electron 编辑器

| 组件 | 文件 | 功能 |
|------|------|------|
| StageView | components/StageView.jsx | Canvas 2D 舞台 + 拖入素材 + Live Preview |
| SceneList | components/SceneList.jsx | 场景拖拽排序、模板选择 |
| CodeEditor | components/CodeEditor.jsx | 语法高亮 + 自动补全 + 错误行标记 |
| AssetPanel | components/AssetPanel.jsx | 场景列表 + 素材树上下分区 |
| PropertyPanel | components/PropertyPanel.jsx | 素材/Live2D/MiniGame 三种上下文切换 |
| AIPanel | components/AIPanel.jsx | AI 对话 + 多后端 + 流式输出 + @指令 |
| Live2DPanel | components/Live2DPanel.jsx | 模型选择、表情/动作、参数调试 |
| MiniGamePanel | components/MiniGamePanel.jsx | 物体列表、PBR材质、光照编辑 |
| LogPanel | components/LogPanel.jsx | 分级筛选、搜索、连接状态指示器 |
| SaveManager | components/SaveManager.jsx | 8档位网格、缩略图、右键菜单 |
| SettingsDialog | components/SettingsDialog.jsx | 标签页分组设置 |
| EditorContext | context/EditorContext.jsx | useReducer 全局状态 |
| kagParser | parser/kagParser.js | KAG Lua 正则解析 + 代码生成 |
| OpenAI/Codex/Custom | providers/ | AI 多后端 (真实 IPC 桥接) |
| Electron main | electron/main.cjs | 引擎 spawn + AI chat bridge |
| Electron preload | electron/preload.cjs | contextBridge IPC |

### RPC 方法
`ping` → 健康检查  
`run` → 执行 Lua 脚本  
`eval` → 评估表达式  
`stop` → 停止场景  
`assets` → 列出素材  
`getState` → 引擎状态  
`getFrame` → 引擎帧截图 (base64 PNG)  
`logs` → 日志缓冲区

---

## 9. 总体评估

### 完成度: ~93% (Beta)

### 优势
- 8 个纯虚接口，核心约束全面合规
- BackendRegistry 单例解耦 + 沙箱安全
- 61 KAG 命令 + 15 MiniGame API + 8 视频函数
- 多线程 JobSystem 覆盖粒子/CARC/纹理/视频解码
- D3D11 零 TDR + 双级 ErrorUI + 智能重试
- DeltaCARC 差分更新 + AES-256-GCM 存档加密
- BgfxMiniGameBackend PBR-lite 3D 小游戏 + 跨平台 shader
- Live2D OpenGLReadback FBO 实现
- Electron 可视化编辑器 (11 组件 + 3 后端) + AI 真实 IPC
- MIT 许可 + Live2D 条件编译无法律风险
- 目录结构精简重组: 10 模块 (13→10)

### 移交共同开发者
- Live2D OpenGLShared + Metal（需实际 Linux/macOS 环境）
- 移动端适配（需真机）
- 跨平台 CI 测试启用（需非 GPU 测试桩）

### 下一阶段
- Beta 冲刺: FFmpeg 默认启用 + MiniGame shader 最终化
- Demo 场景: Live2D + MiniGame + 视频 完整演示
- 编辑器: AI 上下文优化 + Electron 打包试运行
