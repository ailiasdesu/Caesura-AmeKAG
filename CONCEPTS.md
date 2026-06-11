# Caesura (AmeKAG) 核心概念

## 引擎名称
**Caesura** — 乐章中的停顿。代号 **AmeKAG** — あめ + KAG。

## 架构核心

### 后端注册表 (BackendRegistry)
Engine 持有 unique_ptr 所有权，BackendRegistry 缓存裸指针。
Lua 通过 `backend.lua → BackendRegistry → I*Backend` 访问 C++，不直接依赖具体类。

### 初始化链
```
SDL3 → bgfx → SoLoud → Lua VM → SaveManager → HotReload → MiniGame → Live2D
```

### 主循环
```
processEvents → engine_update (Lua) → beginFrame → engine_render → MiniGame render → endFrame → 音频检测
```

### 编辑器 RPC 模式 (--editor)
```
Electron main.cjs spawn --editor → Engine::runRpc() → RpcServer::run()
  stdin/stdout JSON-RPC 8 方法: ping/run/stop/eval/getFrame/getState/assets/logs
  stderr → main.cjs 转发 → IPC engine-log → React LogPanel
```

### 帧捕获流程 (getFrame)
```
RPC getFrame → Engine::captureFrameForRpc → engine_update (Lua) → render()
  → captureFrameBase64 → requestScreenShot → bgfx::frame() → 读PNG → base64
```

### KAG 脚本流程
```
.ks → tokenizer → parser → scheduler → kag.lua → backend.lua → BackendRegistry → I*Backend
```

## 8 个纯虚接口

| 接口 | 实现类 |
|------|--------|
| IRenderDevice | BgfxRenderDevice |
| IAudioBackend | SoLoudAudioEngine / NullAudioBackend |
| IPlatformBackend | SDL3PlatformBackend |
| IAssetProvider | DirAssetProvider / XP3Archive / CarcAssetProvider |
| IAnimationBackend | Live2DBackend |
| ILive2DRenderPath | D3D11Native / OpenGLShared / Metal / OpenGLReadback |
| IMiniGameBackend | BgfxMiniGameBackend / NullMiniGameBackend |
| IVideoDecoder | PlMpegDecoder / FFmpegDecoder |

## 关键模式

### 资源提供者链
```
DirAssetProvider → XP3Archive → CarcAssetProvider (优先级递减)
```

### 纹理内存预算
6 级: 128MB~4GB + 3 级 GPU 降级

### 错误恢复
ErrorUI Level1 (bgfx) → Level2 (SDL MessageBox) → Retry/Title/Quit
TDR 防护: 每 500μs 让步 + 5ms 超时

### 多线程模型
```
主线单线程: processEvents → engine_update → render
辅助多线程: AsyncLoader | ParticleSim | CARC Decrypt | CPU Tasks | Video Decode
```

### 差分更新
```
DeltaCARC: old.carc + new.carc → .cdelta (AES-256-GCM)
apply: old.carc + .cdelta → new.carc (SHA-256 验证)
```

### 3D 小游戏
```
BgfxMiniGameBackend: cube VB/IB → bgfx view 10 → orbit camera → PBR-lite shader
跨平台 shader: D3D11 DXBC / OpenGL GLSL / Metal MSL 运行时选择
Lua: mini_game.spawn_cube(x,y,z) / set_camera(...)
```

### Live2D 渲染路径
4 路径: D3D11Native (Win, ✅) / OpenGLShared (Linux, 移交) / Metal (macOS, 移交) / OpenGLReadback (FBO + GL状态, ✅)

## 目录结构 (10 模块)

| 模块 | 内容 |
|------|------|
| Core/ | Engine, BackendRegistry, InputRouter, JobSystem, RpcServer (JSON-RPC 8方法), MobileAdapter |
| Render/ | BgfxRenderDevice, LayerManager, ParticleSystem, GpuMonitor, VideoPlayer, 跨平台 shader |
| Audio/ | SoLoudAudioEngine (BGM/VOICE/SE) |
| Scripting/ | LuaManager, KAGBinding (61 cmd), RenderBinding, VFXBinding, DevCoreBinding |
| System/ | SaveManager (JSON + AES-256-GCM + Schema v1→v5) |
| CARC/ | CryptoEngine, CARC Reader/Writer, DeltaCARC |
| Resource/ | AssetManager, AsyncLoader, ProviderChain, XP3Archive |
| Live2D/ | Cubism 5 Backend (4 渲染路径, 条件编译) |
| MiniGame/ | BgfxMiniGameBackend (PBR-lite, 15 Lua API, 跨平台 shader) |
| Debug/ | DebugManager (std::put_time 日志), HotReload, DebugProtocol |

## 外部库

| 库 | 用途 |
|----|------|
| bgfx | 跨平台渲染 |
| SDL3 | 窗口/输入 |
| SoLoud | 音频引擎 |
| Lua 5.4 | 脚本引擎 |
| FreeType | 字体渲染 |
| zstd | 压缩 |
| ed25519 | 签名验证 |
| pl_mpeg | 视频解码 (默认) |
| FFmpeg | 视频解码 (条件编译) |
| Live2D Cubism 5 | 动态立绘 (条件编译，不提交SDK) |

## 知识库

`docs/solutions/` — 已记录的问题解决方案（bugs、最佳实践、工作流模式），按类别组织，含 YAML frontmatter。
`web-editor/` — Electron + React 编辑器 (12 组件, F5 运行, RPC 桥接, AI 面板, 一键打包)。

## CI 约束

- `workflow_dispatch` 手动触发，非 push 自动
- SDL3: Windows vcpkg / macOS brew / Linux 源码构建 → /usr/local
- FFmpeg: 非 Windows 平台 OFF（避免额外依赖）
- 路径大小写必须与 git 一致（Linux 区分大小写）
