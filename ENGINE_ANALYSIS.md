# Caesura (AmeKAG) 引擎代码库深度分析

> 分析日期: 2026-06-10 | 构建配置: CMake 3.25+ / C++20
> 构建状态: 上次全量通过 (D3D11 零 TDR)，当前编辑器迭代中
> 最近提交: 965589f — 光标卡grabbing修复

---

## 0. 核心约束（强制）

> **所有 Lua 层对 C++ 的访问，必须通过抽象接口 + BackendRegistry 工厂。**
>
> ```
> Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
> ```
>
> **状态**: ✅ 全面合规 — 0 处违规（Scripting 层零直接引用具体类）

---

## 1. 项目规模

| 维度 | 数值 |
|------|------|
| C++ 文件 | 65 .cpp + 73 .h = 138 编译单元 |
| C++ 代码量 | ~18,500 行 |
| Lua 脚本 | 45 文件 (scripts/ 31 + kag/commands/ 9 + demo/ 1 + dev/ 1) |
| C++ 单元测试 | 24 文件 |
| Electron 编辑器 | 12 组件 + 3 AI provider + 1 解析器 |
| KAG 绑定 | 95 C 函数 (31 KAG + 28 Render + 10 VFX + 10 Debug + 8 DevCore + 8 Unified) |
| Lua KAG 命令 | 14 函数 (9 子模块: audio/layer/resource/save/system/text/transition/vfx/video) |
| MiniGame API | 15 Lua 函数 |
| 纯虚接口 | 8 个 ✅ |
| 外部库 | 11 个全部静态编译 |

---

## 2. 架构总览

```
src/
├── Core/         ← Engine, BackendRegistry, InputRouter, RpcServer
│                   JobSystem, SandboxQuota, TextureBudget, MobileAdapter
├── Render/       ← BgfxRenderDevice, LayerManager, TextRenderer, TextureManager
│                   RTTManager, ShaderCache, ParticleSystem, GpuMonitor, VideoPlayer
├── Audio/        ← SoLoudAudioEngine + NullAudioBackend
├── Scripting/    ← LuaManager, KAGBinding(31), RenderBinding(28+8视频), VFXBinding(10)
│                   DevCoreBinding(8), DebugBinding(10), UnifiedBinding(8), GameState
├── System/       ← SaveManager (JSON+AES-256-GCM, v1→v5 Schema, F5/F6快速存档)
├── CARC/         ← CryptoEngine (BCrypt/OpenSSL), CARCReader/Writer, DeltaCARC
├── Resource/     ← AssetManager, AsyncLoader, ProviderChain, XP3Archive
├── Live2D/       ← Cubism 5 条件编译, 4 渲染路径, OpenGLReadback FBO ✅
├── MiniGame/     ← BgfxMiniGameBackend PBR-lite, 15 Lua API, 跨平台shader
└── Debug/        ← HotReload, DebugProtocol, DebugManager (双级ErrorUI+TDR)

web-editor/
├── electron/     ← main.cjs (engine spawn+AI IPC) + preload.cjs
├── src/          ← 12 React组件 + Context + kagParser + 3 AI providers
└── dist/         ← Vite 构建输出
```

---

## 3. 技术债务追踪

| ID | 描述 | 状态 |
|----|------|:---:|
| TD-01~TD-10 | BackendRegistry 所有权/沙箱/工厂/接口 | ✅ |
| TD-11 | Live2D OpenGLSharedRenderPath | ⚠️ 移交 Linux |
| TD-12 | CryptoEngine 跨平台 (BCrypt/OpenSSL) | ✅ |
| TD-13 | CJK 缓存 | ✅ |
| TD-14 | CARC 差分更新 (DeltaCARC) | ✅ |
| TD-15 | pl_mpeg → FFmpeg 双后端 | ✅ |
| TD-16~TD-18 | 调试/键名/CRL 小修 | ✅ |
| TD-19 | Live2D Metal | ⚠️ 移交 macOS |
| TD-20 | MobileAdapter 存根 | ⚠️ P2 预留 |
| TD-21 | MiniGame 3D PBR-lite | ✅ |
| TD-22 | 跨平台 CI | ✅ |
| TD-23 | BgfxMiniGameBackend 测试链接 | ⚠️ 预存 |
| TD-24 | Live2D OpenGLReadback FBO | ✅ |

**摘要**: 20/24 闭合，4 开放（移交/存根/预存，均非阻塞）

### 存根/TODO 扫描 (2026-06-10)

| 文件 | 级别 | 说明 |
|------|:---:|------|
| MobileAdapter.cpp | P2 | 完整移动端存根，触屏事件预埋 |
| MetalNativeRenderPath.cpp | 移交 | macOS 开发者验证 |
| Live2DBackend.cpp:227 | TODO | 纹理加载到 Cubism 渲染器 |
| SaveManager.cpp:359 | 存根 | 缩略图截图 (SU-4) |
| ISaveProvider.cpp:36 | 存根 | 模式文件列表 |
| CRLManager.cpp:192 | 预期 | 离线/嵌入式使用 |
| NullAudioBackend.cpp | 预期 | SE 空操作存根 |

---

## 4. 编辑器状态

| 组件 | 状态 |
|------|:---:|
| StageView — Canvas 2D + 缩放 + 网格 + 安全区 | ✅ |
| StageView — 鼠标拖拽平移 (中键/Alt+左键) | ✅ |
| StageView — 素材拖入预览 | ✅ |
| StageView — 分辨率切换 | ✅ |
| CodeEditor — CodeMirror 6 + Lua 高亮 + KAG 补全 | ✅ |
| SceneList — 场景拖拽排序 | ✅ |
| AssetPanel — 场景列表 + 素材树 | ✅ |
| PropertyPanel — 三种上下文切换 | ✅ |
| AIPanel — 多后端 + 流式输出 + @指令 | ✅ |
| Live2DPanel — 模型/表情/动作 | ✅ |
| MiniGamePanel — 物体/PBR/光照 | ✅ |
| LogPanel — 分级筛选 + 连接状态 | ✅ |
| SaveManager — 8 档位 Modal | ✅ |
| SettingsPage — AI 配置可操作 | ✅ |
| CommandPalette | ✅ |
| PackagePanel — 一键打包 (Win/Mac/Linux) | ✅ |

---

## 5. 总体评估

### 完成度: ~94% (Beta+)

**引擎核心** (95%): 8 接口 ✅, 95 绑定函数, 45 Lua 脚本, 零 TDR, JobSystem 多线程

**编辑器** (90%): 12 组件全部可用, 缩放/平移/补全/打包就绪, RPC 桥接引擎

**跨平台** (80%): Windows ✅, Linux/macOS CI 配置就绪, Live2D 部分移交

**已知缺口**: Live2D Linux/macOS 渲染路径(移交), 移动端适配(P2存根), 缩略图截图(存根)

### 下一优先级
1. 编辑器 ↔ 引擎 RPC 全面连通测试 (run/eval/getFrame)
2. FFmpeg 默认启用 + 视频播放验证
3. Electron 打包 → 完整安装包测试