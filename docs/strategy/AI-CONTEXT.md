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
| IMiniGameBackend | **BgfxMiniGameBackend** / NullMiniGameBackend |
| IVideoDecoder | PlMpegDecoder |

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
BgfxMiniGameBackend: cube VB/IB → bgfx view 10 → orbit camera → debug wireframe fallback
Lua: mini_game.spawn_cube(x,y,z) / set_camera(...)
```

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