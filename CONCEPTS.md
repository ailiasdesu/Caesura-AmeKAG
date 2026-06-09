# Caesura (AmeKAG) 核心概念

> 更新: 2026-06-09 | 引擎完成度: ~90% (Alpha+)

## 引擎名称
**Caesura** — 乐章中的停顿。代号 **AmeKAG** — 雨 + KAG。

---

## 1. 架构核心

### 1.1 核心约束（强制）
```
Lua 脚本 → backend.lua → BackendRegistry → I*Backend 纯虚接口 → C++ 实现
```
**禁止**: Lua/绑定层直接引用 BgfxRenderDevice、SDL3PlatformBackend 等具体类。
**允许**: BackendRegistry.cpp、Engine.cpp（工厂/接线层）知晓具体类。

### 1.2 初始化链
```
SDL3 → bgfx → SoLoud → Lua VM → SaveManager → HotReload → MiniGame → Live2D → Sandbox
```

### 1.3 主循环
```
processEvents → engine_update (Lua) → beginFrame → engine_render → MiniGame render → endFrame → 音频检测
```

### 1.4 KAG 脚本流程
```
.ks → tokenizer → parser → scheduler → kag.lua → backend.lua → BackendRegistry → I*Backend
```

---

## 2. 8 个纯虚接口

| 接口 | 实现类 | 平台 |
|------|--------|------|
| `IRenderDevice` | BgfxRenderDevice | D3D11 / OpenGL / Metal |
| `IAudioBackend` | SoLoudAudioEngine / NullAudioBackend | 全平台 |
| `IPlatformBackend` | SDL3PlatformBackend | 桌面端 |
| `IAssetProvider` | DirAssetProvider / XP3Archive / CarcAssetProvider | 全平台 |
| `IAnimationBackend` | Live2DBackend (Cubism 5) | Windows ✅ |
| `ILive2DRenderPath` | D3D11Native / OpenGLShared / MetalNative / OpenGLReadback | Win ✅ / 其他 ⚠️ |
| `IMiniGameBackend` | BgfxMiniGameBackend (PBR-lite) | 全平台 |
| `IVideoDecoder` | PlMpegDecoder / FFmpegDecoder | 全平台 |

---

## 3. 关键模式

### 3.1 资源提供者链
```
DirAssetProvider → XP3Archive → CarcAssetProvider (优先级递减)
```

### 3.2 纹理内存预算
6 级: 128MB ~ 4GB + 3 级 GPU 降级

### 3.3 错误恢复
```
ErrorUI Level1 (bgfx debug text) → Level2 (SDL MessageBox) → Retry / Title / Quit
TDR 防护: 每 500μs 让步 + 5ms 超时 + 智能重试
```

### 3.4 多线程模型
```
主线剧情（单线程）: processEvents → engine_update → render
辅助多线程（JobSystem）: AsyncLoader | ParticleSim | CARC Decrypt | CPU Tasks | Video Decode
```

### 3.5 差分更新
```
DeltaCARC: old.carc + new.carc → .cdelta (AES-256-GCM)
apply: old.carc + .cdelta → new.carc (SHA-256 验证)
```

### 3.6 存档系统
```
SaveManager: JSON → AES-256-GCM ("CAES" magic) → 磁盘
Schema 迁移: v1→v5 (playtime / minigame / live2d / editor)
快速存档: F5 保存 / F6 读取 + 自动存档定时器
ISaveProvider: 可替换存储后端
```

### 3.7 沙箱安全
```
默认 DENY: loadfile, dofile, os.execute, io.open 全部禁用
require: 仅 preloaded 模块
Debug: 只读子集
AI strict 模式: Render/DevCore/Debug 白名单代理
```

### 3.8 3D 小游戏
```
BgfxMiniGameBackend: PBR-lite (roughness, metallic, specular)
3 类光源: 环境光 + 方向光 + 点光源 (最多 3 个)
几何体: Cube / Sphere / Plane + 碰撞检测 + 物理
15 个 Lua API: spawn_* / set_camera / material / lighting / collision
```

---

## 4. Electron 编辑器架构

```
┌────────────────────────────────────────────────┐
│  Electron Main Process                         │
│  main.js: engine spawn (headless)              │
│  preload.js: IPC bridge (contextBridge)        │
│  RPC: stdin/stdout JSON-RPC (7 methods)         │
└────────────┬───────────────────────────────────┘
             │ IPC
┌────────────▼───────────────────────────────────┐
│  React Renderer (App.jsx)                      │
│  ┌─────────┐ ┌──────────┐ ┌──────────────────┐│
│  │ Stage   │ │ Timeline │ │ Code Editor      ││
│  │ Canvas  │ │ Events   │ │ Lua textarea     ││
│  │ 2D      │ │ sorted   │ │ bidirectional    ││
│  └─────────┘ └──────────┘ └──────────────────┘│
│  ┌─────────┐ ┌──────────┐ ┌──────────────────┐│
│  │ Assets  │ │ Props    │ │ AI Panel         ││
│  │ Browser │ │ Editor   │ │ OpenAI/Codex     ││
│  │ drag    │ │ selected │ │ /Custom          ││
│  └─────────┘ └──────────┘ └──────────────────┘│
│  ┌──────────────────────────────────────────┐  │
│  │ Log Panel (real-time engine output)      │  │
│  └──────────────────────────────────────────┘  │
└────────────────────────────────────────────────┘
```

### RPC 方法
| 方法 | 功能 |
|------|------|
| `ping` | 引擎存活检测 |
| `run` | 执行 Lua 脚本 |
| `eval` | 评估 Lua 表达式 |
| `stop` | 停止当前场景 |
| `assets` | 列出素材 (bg/char/bgm/voice/se/scripts) |
| `getState` | 当前场景名 + 引擎状态 |
| `logs` | 日志缓冲区 |

### AI 后端
| Provider | 类型 | 配置 |
|----------|------|------|
| `openai` | OpenAI API | API key + model (gpt-4o/gpt-4o-mini) |
| `codex` | Codex 本地桥接 | 自动检测 IDE |
| `custom` | 自定义端点 | URL + 可选 key |

---

## 5. 外部库

| 库 | 版本 | 用途 |
|----|------|------|
| bgfx | vendored | 跨平台渲染 (D3D11/OpenGL/Metal) |
| SDL3 | 3.4.10 | 窗口 + 输入 |
| SoLoud | vendored | 音频引擎 (BGM/VOICE/SE 三通道) |
| Lua | 5.4 | 脚本引擎 |
| FreeType | vendored | 字体渲染 (含 CJK) |
| zstd | vendored | 压缩 |
| ed25519 | vendored | 数字签名验证 |
| pl_mpeg | vendored | 视频解码 (默认) |
| FFmpeg | 条件编译 | 视频解码 (可选) |
| nlohmann/json | v3.11.3 | 存档 JSON 序列化 |
| Live2D Cubism 5 | 不提交仓库 | 动态立绘 (条件编译) |
| Electron | 42 | 可视化编辑器框架 |
| React | 18 | 编辑器 UI |
| Vite | 5 | 前端构建 |

---

## 6. 知识库

`docs/solutions/` — 按类别组织的已解决问题：
- `architecture-patterns/` — 架构模式（如 MiniGame 3D 管线）
- `build-error/` — 构建错误修复记录
- `runtime-errors/` — 运行时错误诊断

每个条目含 YAML frontmatter 元数据：`status`, `type`, `severity`, `date`, `tags`。

通过 `/ce-compound` 记录新知识，`/ce-compound-refresh` 审计索引。