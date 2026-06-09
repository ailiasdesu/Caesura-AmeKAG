# Caesura (AmeKAG) 核心概念

## 引擎名称
**Caesura** — 乐章中的停顿，呼应视觉小说的节奏感。
代号 **AmeKAG** — あめ (雨) + KAG，致敬 KiriKiri 的 KAG 脚本系统。

## 架构核心

### 后端注册表 (BackendRegistry)
解耦 C++ 与 Lua 的关键模式。Engine 持有 unique_ptr 所有权，BackendRegistry 缓存裸指针。
Lua 脚本通过 `backend.lua → BackendRegistry → I*Backend` 访问后端，不直接依赖 C++ 具体类。

### 初始化链
```
SDL3 平台 → bgfx 渲染 → SoLoud 音频 → Lua VM → SaveManager → HotReload
```

### 主循环
```
processEvents → engine_update (Lua) → beginFrame → engine_render (Lua) → MiniGame hook → endFrame → 音频检测
```

### 三层分离
- **C++ 层**: 性能关键路径 (渲染/音频/加密/解码)，`src/` 下 12 个模块
- **Lua 层**: 游戏逻辑 (KAG 解析/调度/命令执行)，`scripts/` 下 43 个文件
- **IDE 层**: Electron 编辑器 (RpcServer → EditorServer JSON-RPC)

### KAG 脚本流程
```
.ks 文件 → tokenizer.lua (词法) → parser.lua (语法) → scheduler.lua (调度) → kag.lua (命令分发) → backend.lua (C++ 代理) → BackendRegistry → I*Backend
```

## 8 个纯虚接口

| 接口 | 头文件 | 实现类 |
|------|--------|--------|
| IRenderDevice | src/Render/IRenderDevice.h | BgfxRenderDevice |
| IAudioBackend | src/Audio/IAudioBackend.h | SoLoudAudioEngine / NullAudioBackend |
| IPlatformBackend | src/Core/IPlatformBackend.h | SDL3PlatformBackend |
| IAssetProvider | src/Resource/IAssetProvider.h | DirAssetProvider / XP3Archive / CarcAssetProvider |
| IAnimationBackend | src/Animation/IAnimationBackend.h | Live2DBackend |
| ILive2DRenderPath | src/Animation/Live2D/ILive2DRenderPath.h | D3D11Native / OpenGLShared / Metal / OpenGLReadback |
| IMiniGameBackend | src/MiniGame/IMiniGameBackend.h | NullMiniGameBackend |
| IVideoDecoder | src/Render/IVideoDecoder.h | PlMpegDecoder |

## 关键模式

### 资源提供者链
```
ProviderChain: DirAssetProvider → XP3Archive → CarcAssetProvider (优先级递减)
```

### 纹理内存预算
```
6 级自动检测: 128MB / 256MB / 512MB / 1GB / 2GB / 4GB (基于系统 RAM)
3 级 GPU 降级: 全分辨率 → 半分辨率 → 纯色回退
```

### 错误恢复
```
ErrorUI Level1 (bgfx 存活) → Level2 (SDL MessageBox 回退) → Retry/Title/Quit
智能重试: 同 token 3 次提级 + 白名单检查
TDR 防护: 每 500μs 让步 + 5ms 超时检测
```

### 多线程模型
```
主线剧情（单线程顺序）:
  processEvents → engine_update (Lua) → beginFrame → engine_render → endFrame

辅助多线程（JobSystem）:
  AsyncLoader (纹理) | ParticleSim (粒子) | CARC Decrypt (解密) | CPU Tasks (通用)
```


## 知识库

`docs/solutions/` — 已记录的问题解决方案（bugs、最佳实践、工作流模式），按类别组织，含 YAML frontmatter (`module`, `tags`, `problem_type`)。在已记录领域实现或调试时参考。

## 外部库选择

| 库 | 用途 | 选择理由 |
|----|------|----------|
| bgfx | 跨平台渲染 | 单 API 多后端 (D3D11/Metal/OpenGL/Vulkan) |
| SDL3 | 窗口/输入 | 最广泛平台支持 |
| SoLoud | 音频引擎 | 轻量、多后端、3D 空间音频 |
| Lua 5.4 | 脚本引擎 | 轻量、可嵌入、沙箱能力强 |
| FreeType | 字体渲染 | TTF/OTF + CJK 支持 |
| zstd | 压缩 | 现代压缩比 + 速度 |
| ed25519 | 签名验证 | 公钥域、MIT 友好 |
| pl_mpeg | 视频解码 | 单头文件、无外部依赖 |
| stb | 图像解码 | 单头文件 |
| cpp-httplib | HTTP/RPC | 单头文件、Electron IDE 通信 |
| Live2D Cubism 5 | 动态立绘 | 条件编译 OFF，不提交 SDK，法律安全 |