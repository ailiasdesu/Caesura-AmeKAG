# Caesura (AmeKAG) — C++ API Interface Reference

> **30 个纯虚接口，16 个模块，统一通过 `BackendRegistry` 访问**
> 最后更新: 2026-06-12

---

## 目录

1. [archive — 加密归档](#1-archive--加密归档)
2. [audio — 音频后端](#2-audio--音频后端)
3. [debug — 日志与性能分析](#3-debug--日志与性能分析)
4. [di — 依赖注入与资源配额](#4-di--依赖注入与资源配额)
5. [entry — 引擎组合根](#5-entry--引擎组合根)
6. [input — 输入路由](#6-input--输入路由)
7. [job — 多线程任务系统](#7-job--多线程任务系统)
8. [live2d — 动画后端](#8-live2d--动画后端)
9. [minigame — 3D 小游戏](#9-minigame--3d-小游戏)
10. [platform — 平台抽象](#10-platform--平台抽象)
11. [render — 渲染 (6 个接口)](#11-render--6-个接口)
12. [resource — 资源管理](#12-resource--资源管理)
13. [rpc — HTTP/JSON-RPC 服务器](#13-rpc--httpjson-rpc-服务器)
14. [script — Lua 虚拟机](#14-script--lua-虚拟机)
15. [steam — Steamworks 集成](#15-steam--steamworks-集成)
16. [storage — 存档/读档](#16-storage--存档读档)

---

## 访问规则

所有后端通过 `BackendRegistry::instance()` 访问：

```cpp
auto* renderer = BackendRegistry::instance().getRenderDevice();
auto* audio    = BackendRegistry::instance().getAudioBackend();
auto* lua      = BackendRegistry::instance().getLuaState();
```

- BackendRegistry 存储非拥有指针（`I*`），Engine 持有 `unique_ptr` 所有权。
- 子系统通过 `set*()` 注册，通过 `get*()` 访问。
- 禁止绕过 BackendRegistry 直接访问单例（DEBUG_* 宏除外）。

---

## 1. archive — 加密归档

**命名空间**: `Caesura::carc`
**实现**: CARCReader, CARCWriter, CryptoEngine
**用途**: 将游戏资源打包为单个加密归档文件（.carc），支持 Ed25519 签名验证。

### 1.1 IArchiveReader

```cpp
class IArchiveReader {
public:
    virtual ~IArchiveReader() = default;

    virtual bool open(const std::string& path,
                      const std::string& pubKeyPath = "") = 0;
    virtual void close() = 0;
    virtual std::vector<uint8_t> readFile(const std::string& relativePath) = 0;
    virtual bool hasFile(const std::string& relativePath) const = 0;
    virtual size_t numFiles() const = 0;
    virtual bool isOpen() const = 0;
};
```

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `open` | `path`: .carc 文件路径；`pubKeyPath`: Ed25519 公钥路径（可选） | `bool` | 打开归档，可选签名验证 |
| `close` | — | — | 关闭归档，释放资源 |
| `readFile` | `relativePath`: 归档内相对路径 | `vector<uint8_t>` | 读取文件内容，未找到返回空 |
| `hasFile` | `relativePath` | `bool` | 检查文件是否存在 |
| `numFiles` | — | `size_t` | 归档内文件总数 |
| `isOpen` | — | `bool` | 归档是否已打开 |

### 1.2 IArchiveWriter

```cpp
class IArchiveWriter {
public:
    virtual ~IArchiveWriter() = default;

    virtual bool create(const std::string& outputPath,
                        const std::string& privateKeyPath = "",
                        const std::string& publicKeyPath = "") = 0;
    virtual bool addFile(const std::string& relativePath,
                         const uint8_t* data, size_t size) = 0;
    virtual bool finalize() = 0;
};
```

| 方法 | 说明 |
|------|------|
| `create` | 创建输出归档，可选 Ed25519 签名密钥 |
| `addFile` | 添加文件到归档（`relativePath` 为内部路径） |
| `finalize` | 写入索引和签名，关闭归档 |

### 1.3 ICryptoEngine

```cpp
class ICryptoEngine {
public:
    virtual ~ICryptoEngine() = default;

    // AES-256-GCM 加解密
    virtual std::vector<uint8_t> encrypt(
        const uint8_t* plaintext, size_t plaintextLen,
        const uint8_t* key, size_t keyLen,
        uint8_t* nonce, size_t nonceLen,
        uint8_t* tag, size_t tagLen) = 0;

    virtual std::vector<uint8_t> decrypt(
        const uint8_t* ciphertext, size_t ciphertextLen,
        const uint8_t* key, size_t keyLen,
        const uint8_t* nonce, size_t nonceLen,
        const uint8_t* tag, size_t tagLen) = 0;

    // SHA-256
    virtual void sha256(const uint8_t* data, size_t len,
                        uint8_t* hash, size_t hashLen) = 0;

    // Ed25519 签名/验证
    virtual bool sign(const uint8_t* data, size_t len,
                      const uint8_t* privateKey, size_t privateKeyLen,
                      uint8_t* signature, size_t signatureLen) = 0;
    virtual bool verify(const uint8_t* data, size_t len,
                        const uint8_t* publicKey, size_t publicKeyLen,
                        const uint8_t* signature, size_t signatureLen) = 0;

    // 随机数生成
    virtual void generateKey(uint8_t* key, size_t keyLen) = 0;
    virtual void generateNonce(uint8_t* nonce, size_t nonceLen) = 0;
    virtual void generateKeyPair(uint8_t* publicKey, size_t publicKeyLen,
                                 uint8_t* privateKey, size_t privateKeyLen) = 0;

    // 密钥文件 I/O
    virtual bool readPublicKey(const std::string& path,
                               uint8_t* key, size_t keyLen) = 0;
    virtual bool readPrivateKey(const std::string& path,
                                uint8_t* key, size_t keyLen) = 0;
    virtual bool writePublicKey(const std::string& path,
                                const uint8_t* key, size_t keyLen) = 0;
    virtual bool writePrivateKey(const std::string& path,
                                 const uint8_t* key, size_t keyLen) = 0;
};
```

---

## 2. audio — 音频后端

**实现**: SoLoudAudioEngine
**用途**: BGM / Voice / SE 三总线音频播放，支持 3D 空间音效。

### IAudioBackend

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `init` | — | `bool` | 初始化音频引擎 |
| `shutdown` | — | — | 关闭并释放所有资源 |
| `update` | `deltaTime` | — | 每帧更新音频系统 |
| `playBGM` | `file`, `fadeTime=1.0f` | `uint` | 播放背景音乐（支持淡入） |
| `stopBGM` | `fadeTime=1.0f` | — | 停止 BGM（支持淡出） |
| `playVoice` | `file` | `uint` | 播放语音（绝对中断前一条） |
| `stopVoice` | — | — | 停止语音 |
| `playSE` | `file` | `uint` | 播放 2D 音效 |
| `playSE3D` | `file`, `x, y, z` | `uint` | 播放 3D 空间音效 |
| `stopSE` | — | — | 停止所有音效 |
| `setSEVolume` | `handle`, `volume` | — | 设置单个音效句柄的音量 |
| `getSEVolume` | `handle` | `float` | 获取单个音效句柄的音量 |
| `stopSEHandle` | `handle` | — | 停止指定句柄的音效 |
| `update3dListener` | `posX/Y/Z, atX/Y/Z, upX, upY, upZ` | — | 更新 3D 听者位置 |
| `setGlobalVolume` | `volume` | — | 设置全局音量 (0.0–1.0) |
| `getGlobalVolume` | — | `float` | 获取全局音量 |
| `setBusVolume` | `bus`, `volume` | — | 按总线设置音量（"bgm"/"voice"/"se"） |
| `getBusVolume` | `bus` | `float` | 按总线获取音量 |
| `flushWaveCache` | — | — | 清空波形缓存 |
| `isVoicePlaying` | — | `bool` | 语音是否播放中 |
| `isBGMPlaying` | — | `bool` | BGM 是否播放中 |
| `isSEPlaying` | — | `bool` | 是否有音效播放中 |
| `activeVoiceCount` | — | `int` | 当前活跃语音数 |
| `getPosition` | `bus` | `float` | 总线当前播放位置（秒） |
| `getLength` | `bus` | `float` | 总线当前曲目总长度（秒） |
| `fadeVolume` | `bus`, `targetVolume`, `fadeTime` | — | 平滑过渡到目标音量 |
| `getBackendName` | — | `const char*` | 后端名称 |

---

## 3. debug — 日志与性能分析

**实现**: DebugManager
**访问方式**: `BackendRegistry::instance().getDebugManager()` 或直接通过 `DEBUG_*` 宏（零开销路径）

### 3.1 枚举类型

| 枚举 | 值 | 说明 |
|------|-----|------|
| `DbgLevel` | Trace=0, Debug=1, Info=2, Warn=3, Err=4, Fatal=5 | 日志级别 |
| `SubSys` | Render=0, Audio=1, Scripting=2, Input=3, Platform=4, Engine=5, Dbg=6 | 子系统标识 |
| `ErrCode` | 见头文件 `IDebugManager.h` | 错误码（按子系统分段：1xxx=Platform, 2xxx=Render, 3xxx=Audio, 4xxx=Script, 6xxx=Engine, 9xxx=Internal） |

### 3.2 IDebugManager

| 方法 | 说明 |
|------|------|
| `init(logDir)` | 初始化日志系统，指定日志目录 |
| `shutdown` | 关闭，刷新缓冲区 |
| `log(level, subsystem, code, fmt, ...)` | 记录带错误码的日志 |
| `log(level, subsystem, fmt, ...)` | 记录无错误码的日志 |
| `lastError` | 返回最近一条错误日志 |
| `errorCount` | 错误总数 |
| `entryCount` | 日志条目总数 |
| `subsystemErrorCount(s)` | 按子系统统计错误数 |
| `ringBuffer` | 返回日志环形缓冲区 |
| `getSubsystemStats(s)` | 返回子系统的 `SubsystemStats` |
| `dumpFullReport` | 生成完整报告字符串 |
| `beginProfile(label)` | 开始性能采样 |
| `endProfile(label)` | 结束性能采样 |
| `recordGpuSubmit(count)` | 记录 GPU 提交数 |
| `recordTransientAlloc(count, bytes)` | 记录瞬态分配 |
| `recordLuaGc(ms)` | 记录 Lua GC 耗时 |
| `getFrameProfile` | 返回 `FrameProfile`（含 totalMs, gpuSubmitMs, luaGcMs） |
| `endFrameProfile` | 结束当前帧的性能采样 |
| `getRenderInfo` / `setRenderInfo` | 渲染子系统信息 |
| `getAudioInfo` / `setAudioInfo` | 音频子系统信息 |
| `getInputInfo` / `setInputInfo` | 输入子系统信息 |

---

## 4. di — 依赖注入与资源配额

### 4.1 ISandboxQuota

Lua 沙箱资源配额管理，防止脚本资源泄漏。

| 方法 | 说明 |
|------|------|
| `tryAlloc(kind)` | 尝试分配资源（如 "textures", "particles_emitters"） |
| `release(kind)` | 释放资源计数 |
| `count(kind)` | 当前使用计数 |
| `maxLimit(kind)` | 最大限制 |

### 4.2 ITextureBudget

纹理内存预算自动检测和分级。

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `detect` | — | 自动检测系统内存，确定预算等级 |
| `setTier(tier)` | — | 手动设置 0–5 级 |
| `getTier` | `int` | 当前预算等级 (0=Low/128MB … 5=DevOverride/4GB) |
| `getBudgetMB` | `uint32_t` | 预算（MB） |
| `getBudgetBytes` | `uint64_t` | 预算（字节） |
| `getTierName` | `const char*` | 预算等级名称 |
| `isAutoDetected` | `bool` | 是否自动检测（false=手动设置） |

---

## 5. entry — 引擎组合根

**无独立接口**（`entry/` 是唯一可以 include 具体实现头文件来创建对象的模块）。

核心类型：
- `EngineConfig` — 聚合结构体，承载所有后端配置
- `Engine` — 四阶段初始化：`initPlatformPhase()` → `initScriptingPhase()` → `initAssetPhase()` → `initOptionalPhase()`

参见 [Engine.cpp](/src/entry/Engine.cpp) 和 [AGENTS.md](/AGENTS.md) 第 4 节。

---

## 6. input — 输入路由

**实现**: InputRouter
**注册**: `BackendRegistry::instance().setInputRouter()`

### IInputRouter

```cpp
enum class InputFocus { KAG, GAME };
using GameInputCallback = std::function<void(const SDL_Event&)>;
```

| 方法 | 说明 |
|------|------|
| `processEvent(event)` | 处理 SDL 事件，路由到当前焦点的回调 |
| `setFocus(focus)` | 切换输入焦点（KAG ↔ GAME） |
| `getFocus` | 返回当前焦点 |
| `registerGameCallback(cb)` | 注册 GAME 模式的输入回调 |
| `registerKAGCallback(cb)` | 注册 KAG 模式的输入回调 |
| `registerFocusChangeCallback(cb)` | 注册焦点切换回调 |
| `registerResizeCallback(cb)` | 注册窗口大小变化回调 |
| `notifyResize(w, h)` | 通知所有 resize 回调 |
| `hasKAGClick` | KAG 是否有点击待处理 |
| `isClickPending` | 同 `hasKAGClick` |
| `consumeKAGClick` | 消耗 KAG 点击事件 |

---

## 7. job — 多线程任务系统

**实现**: JobSystem（真实多线程）、NullJobSystem（测试用同步模拟）
**Mock**: `tests/mocks/NullJobSystem.h` — 所有任务在主线程同步执行

### IJobSystem

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `init` | — | — | 启动工作线程池 |
| `shutdown` | — | — | 等待并关闭所有线程 |
| `submit` | `work`, `priority=Normal`, `onComplete=nullptr` | `uint64_t` | 提交异步任务，返回 job ID |
| `pollMainThreadJobs` | — | — | 执行已完成任务的 `onComplete` 回调 |
| `waitIdle` | — | — | 阻塞直到所有任务完成 |
| `workerCount` | — | `int` | 工作线程数 |
| `pendingJobs` | — | `int` | 待处理任务数 |
| `isRunning` | — | `bool` | 任务系统是否运行中 |

**线程安全约束**：
- `submit` 可将 CPU 工作（解码、物理）分发到 Worker 线程
- `onComplete` 回调在主线程 `pollMainThreadJobs` 中执行
- Worker 线程**绝不执行 Lua 代码**
- 内存分配器必须线程安全

---

## 8. live2d — 动画后端

**实现**: Live2DBackend（需 Cubism SDK）、NullAnimationBackend（PNG 降级）

### IAnimationBackend

| 方法 | 说明 |
|------|------|
| `init` | 初始化动画系统 |
| `shutdown` | 释放所有资源 |
| `loadModel(path, name)` | 加载模型，返回 handle（0=失败） |
| `unloadModel(handle)` | 卸载模型 |
| `isLoaded(handle)` | 模型是否已加载 |
| `showModel(handle, x, y, scale)` | 显示模型在指定位置 |
| `hideModel(handle)` | 隐藏模型 |
| `setOpacity(handle, opacity)` | 设置透明度 (0.0–1.0) |
| `render(dt)` | 渲染所有可见模型 |
| `playMotion(handle, name)` | 播放指定动作 |
| `setExpression(handle, name)` | 设置表情 |
| `setParameter(handle, param, value)` | 设置模型参数 |
| `name` | 返回后端名称 |

**降级行为**：无 Cubism SDK 时，NullAnimationBackend 支持 PNG/JPG/BMP 静态图片作为立绘，通过 TextureManager 加载。

---

## 9. minigame — 3D 小游戏

**实现**: BgfxMiniGameBackend（预留）
**状态**: 接口完整，3D 渲染实现待后续完成

### IMiniGameBackend

| 方法 | 说明 |
|------|------|
| `init` | 初始化，分配 GPU 资源 |
| `shutdown` | 释放所有 GPU/CPU 资源 |
| `loadScene(path)` | 加载 3D 场景（glTF/OBJ），返回 scene handle |
| `unloadScene(handle)` | 卸载场景 |
| `enter(handle)` | 进入小游戏模式（隐藏 KAG 层，激活 3D 相机） |
| `leave` | 退出小游戏模式（恢复 KAG 渲染） |
| `isActive` | 是否处于小游戏模式 |
| `update(dt)` | CPU 更新（物理、动画），可在线程池执行 |
| `render` | GPU 提交，仅主线程 |
| `processEvent(sdlEvent)` | 输入事件路由，返回 true=已消费 |
| `luaCall(L, method)` | Lua 桥接调用 |
| `setRenderDevice(dev)` | 注入渲染设备 |
| `getBackendName` | 后端名称 |

**生命周期**：KAG 场景 → `mini_game:enter` → active loop (update+render) → `mini_game:leave` → KAG 场景

---

## 10. platform — 平台抽象

**实现**: SDL3PlatformBackend

### IPlatformBackend

| 方法 | 说明 |
|------|------|
| `init(title, width, height)` | 创建窗口，初始化 SDL3 |
| `shutdown` | 销毁窗口，关闭 SDL3 |
| `pollEvent` | 轮询事件，派发到回调。返回 true=有事件 |
| `getMouseState` | 返回 `MouseState{x, y, leftDown}` |
| `getTicksMs` | 返回启动后毫秒数 |
| `getNativeWindowHandle` | 返回原生窗口句柄（给 bgfx） |
| `getWindowWidth` / `getWindowHeight` | 窗口尺寸 |
| `setFullscreen` | 切换全屏 |
| `resizeWindow(w, h)` | 调整窗口大小 |
| `getBackendName` | 后端名称 |

---

## 11. render — 6 个接口

渲染是接口最多的模块，覆盖设备管理、纹理、图层、粒子、GPU 监控、视频播放。

### 11.1 IRenderDevice

**核心渲染设备抽象**。不在 `api/` 目录（直接位于 `src/render/`），因为它是最核心的渲染契约。

**视图 ID 常量**：
| 常量 | 值 | 用途 |
|------|-----|------|
| `VIEW_RTT` | 0 | 离屏渲染到纹理 |
| `VIEW_MAIN` | 1 | 主合成管线（KAG UI） |
| `VIEW_DEBUG` | 2 | 调试覆盖层 |
| `VIEW_TRANSITION` | 99 | 过渡效果合成 |

| 分类 | 方法 | 说明 |
|------|------|------|
| **生命周期** | `init(hwnd, w, h)` | 初始化 bgfx 渲染设备 |
| | `shutdown` | 先 `flushAllRTT` 再 `bgfx::shutdown` |
| | `flushAllRTT` | 释放所有 RTT framebuffer（GPU 上下文仍存） |
| | `resize(w, h)` | 窗口大小变化时重建 backbuffer |
| **帧管理** | `beginFrame` | 开始帧 |
| | `endFrame` | 结束帧 |
| | `commit_frame` | 提交帧到 GPU |
| **视图** | `setViewRect(viewId, x, y, w, h)` | 设置视图矩形 |
| | `setViewClear(viewId, flags, rgba, depth, stencil)` | 设置视图清除 |
| | `touch(viewId)` | 标记视图为活跃 |
| **离屏渲染** | `createRenderTarget(w, h)` | 创建 RTT，返回 `ViewportHandle` |
| | `destroyRenderTarget(handle)` | 销毁 RTT |
| | `blitViewport(handle, viewId, x, y, w, h)` | 将 RTT 纹理绘制到目标视图 |
| | `getViewportTexture(handle)` | 获取 RTT 的 bgfx 纹理（bgfx::TextureHandle） |
| | `fillViewport(handle, r, g, b, a)` | 纯色填充 RTT |
| **纹理 Blit** | `blitTexture(viewId, texId, x, y, w, h, opacity)` | 将纹理（TM ID）绘制到视图 |
| | `stretchBlt(viewId, dstTexId, dx, dy, dw, dh, srcTexId, sx, sy, sw, sh, filter)` | 缩放 Blit |
| | `affineBlt(viewId, dstTexId, ..., matrix[6])` | 仿射变换 Blit |
| **批量协议** | `beginBatch` | 开始批量提交 |
| | `flushBatch` | 刷新批量提交 |
| **文字渲染** | `renderText(viewId, text, x, y, r, g, b, a)` | 渲染文字 |
| | `renderRuby(viewId, text, ruby, x, y, r, g, b, a)` | 渲染注音文字 |
| | `setFont(fontId)` | 设置字体 |
| | `textLineHeight` | 行高 |
| **特效** | `submitBlend(viewId, baseTex, blendTex, mode, baseAlpha, blendAlpha, globalAlpha)` | 混合特效 |
| | `submitTransition(viewId, fromTex, toTex, ruleTex, method, progress)` | 转场特效 |
| | `submitVFX(viewId, srcTex, effect, ...)` | 视觉特效 |
| **调试** | `setDebugName(viewId, name)` | 设置视图调试名称 |
| **着色器** | `getDefaultSampler` / `getFallbackProgram` | 着色器/采样器访问 |

### 11.2 ILayerManager

三层合成管理器（BG=背景, FG=前景, MSG=消息）。

| 方法 | 说明 |
|------|------|
| `init` / `shutdown` | 生命周期 |
| `setTexture(t, texId)` | 设置图层纹理（bgfx idx） |
| `setVisible(t, visible)` | 设置图层可见性 |
| `setOpacity(t, opacity)` | 设置图层不透明度 |
| `setPosition(t, x, y)` | 设置图层位置 |
| `setScale(t, sx, sy)` | 设置图层缩放 |
| `setBlendMode(t, blend)` | 设置混合模式 |
| `clear(t)` | 清除单个图层 |
| `clearAll` | 清除所有图层 |
| `markAllDirty` | 标记所有图层为脏 |
| `markDirty(t, x, y, w, h)` | 标记脏矩形 |
| `markDirtyWithTransparency(t, x, y, w, h)` | 标记脏矩形（递归标记下层） |
| `updateDirtyRegions(screenW, screenH)` | 合并脏矩形，设置 scissor |
| `clearDirtyRects` | 清除脏矩形 |
| `render(viewId, screenW, screenH, programId)` | 按 BG→FG→MSG 顺序提交图层 |

### 11.3 ITextureManager

纹理生命周期管理。

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `initialize` | `bool` | 初始化纹理管理器 |
| `shutdown` | — | 释放所有纹理 |
| `setDevMode(bool)` | — | Dev=true 显示棋盘格占位纹理 |
| `loadTexture(path)` | `uint32_t` | 从文件加载，返回 TM ID |
| `loadTextureFromMemory(data, size, cacheKey)` | `uint32_t` | 从内存加载 |
| `loadTextureFromRGBA(rgba, w, h, cacheKey)` | `uint32_t` | 从 RGBA 像素数据加载 |
| `createSolidTexture(r, g, b, a)` | `uint32_t` | 创建 1×1 纯色纹理，返回 TM ID |
| `getPlaceholderTexture` | `uint32_t` | 占位纹理 bgfx idx |
| `destroyTexture(id)` | — | 销毁纹理 |
| `getTextureHandle(id)` | `uint32_t` | TM ID → bgfx idx（0=无效） |
| `getTextureSizeById(id, w, h)` | — | 查询纹理尺寸 |
| `isValid(id)` | `bool` | 纹理 ID 是否有效 |
| `totalTextureBytes` | `uint64_t` | 总纹理内存占用 |
| `checkBudget(id, w, h)` | — | 预算检查，超限时 LRU 驱逐 |
| `trackTexture(id, bytes)` | — | 跟踪纹理 |
| `untrackTexture(id)` | — | 取消跟踪 |

### 11.4 IParticleSystem

2D 粒子特效系统。

| 方法 | 说明 |
|------|------|
| `init` | 初始化粒子系统 |
| `shutdown` | 释放所有粒子 |
| `createEmitter(cfg)` | 创建发射器（`Emitter` 结构体），返回 ID |
| `destroyEmitter(id)` | 销毁发射器 |
| `emit(emitterId, count)` | 发射指定数量粒子 |
| `update(dt, screenW, screenH)` | 更新粒子物理 |
| `render(viewId)` | 渲染所有活跃粒子 |
| `aliveCount` | 活跃粒子总数 |

**参数校验**（VFXBinding 层）：`rate`, `lifeMin/Max`, `speedMin/Max`, `sizeMin/Max` 不能为负；`lifeMin > lifeMax` 时自动 clamp。

### 11.5 IGpuMonitor

GPU 性能监控和自适应降级。

```cpp
enum class GpuQuality : uint8_t { HIGH = 0, MEDIUM = 1, LOW = 2 };

struct FrameMetrics {
    double gpuTimeMs, cpuTimeMs, rollingAvgMs;
    uint32_t frameCount, overloadFrames;
    bool degraded;
    GpuQuality quality;
};
```

| 方法 | 说明 |
|------|------|
| `update(dt)` | 更新 GPU 性能数据，返回当前质量等级 |
| `metrics` | 返回 `FrameMetrics` 引用 |
| `currentQuality` | 当前质量等级 |
| `isDegraded` | 是否已降级 |
| `resolutionScale` | 当前分辨率缩放因子 |
| `vfxEnabled` | VFX 是否启用（低性能时自动关闭） |
| `reset` | 重置统计数据 |

### 11.6 IVideoPlayer

MPEG-1/FFmpeg 视频播放。

```cpp
struct VideoHandle { uint32_t id = 0; explicit operator bool() const; };
```

| 方法 | 说明 |
|------|------|
| `open(path)` | 打开视频文件，返回 `VideoHandle` |
| `close(handle)` | 关闭视频 |
| `update(handle, dt)` | 解码下一帧到 bgfx 纹理 |
| `getTexture(handle)` | 返回当前帧 bgfx 纹理 idx |
| `isPlaying(handle)` | 是否播放中 |
| `hasEnded(handle)` | 是否播放完毕 |
| `width(handle)` / `height(handle)` | 视频尺寸 |
| `duration(handle)` | 总时长（秒） |
| `currentTime(handle)` | 当前播放位置（秒） |
| `pause(handle)` / `resume(handle)` | 暂停/继续 |
| `seek(handle, time)` | 跳转到指定时间 |
| `shutdown` | 停止所有视频 |
| `activeCount` | 活跃视频数 |

---

## 12. resource — 资源管理

### 12.1 IAssetProvider

抽象资产源（文件系统、CARC 归档等）。

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `read(path)` | `vector<uint8_t>` | 读取文件，不存在返回空 |
| `exists(path)` | `bool` | 文件是否存在 |
| `getSource` | `string` | 人类可读的来源名称 |
| `priority` | `int` | 优先级（高=先查）。CARC=10, Dir=5, Patch=8 |
| `verify` | `bool` | 完整性校验（CARC 验证 Ed25519 签名） |

### 12.2 IAsyncLoader

异步资源加载管线。

```cpp
struct CompletedLoad {
    int id; std::string path, type; bool success;
    std::vector<uint8_t> rgba; uint16_t width, height;
    std::vector<uint8_t> data;
};
```

| 方法 | 说明 |
|------|------|
| `init` / `shutdown` | 生命周期 |
| `enqueue(path, type)` | 入队加载请求，返回 load ID |
| `cancelAll` | 取消所有待处理加载 |
| `poll` | 轮询完成结果（需每帧调用） |
| `pendingCount` | 待处理加载数 |
| `isRunning` | 加载器是否运行中 |

---

## 13. rpc — HTTP/JSON-RPC 服务器

### 13.1 IEditorServer

Web 编辑器 HTTP 服务器。

| 方法 | 说明 |
|------|------|
| `start(port=9876)` | 启动 HTTP 服务器 |
| `stop` | 停止服务器 |
| `isRunning` | 服务器是否运行中 |
| `port` | 当前端口号 |
| `pushLog(level, message)` | 推送日志到编辑器前端 |
| `setLuaState(L)` | 注入 Lua 状态机 |
| `setWebRoot(path)` | 设置 Web 前端静态文件根目录 |

**已注册端点**：
| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/ping` | 健康检查 → `{"status":"ok"}` |
| GET | `/api/status` | 引擎状态（Lua 可用性、端口） |
| GET | `/api/assets` | 列出项目资源（支持 `?type=image/audio/script` 过滤） |
| POST | `/api/run` | 执行 Lua 脚本 |
| POST | `/api/stop` | 停止执行 |
| GET | `/api/logs` | 近期日志 |

### 13.2 IRpcServer

JSON-RPC 服务器接口。

| 方法 | 说明 |
|------|------|
| `run` | 启动 RPC 服务器 |
| `stop` | 停止 |
| `isRunning` | 是否运行中 |
| `setFrameCaptureCallback(cb)` | 设置帧截图回调 |
| `pushLog(level, message)` | 推送日志 |

---

## 14. script — Lua 虚拟机

**实现**: LuaManager
**注册**: `BackendRegistry::instance().setLuaState()`

### ILuaManager

| 方法 | 说明 |
|------|------|
| `init` | 初始化 Lua VM，注册绑定，执行 `scripts/init.lua` |
| `shutdown` | 关闭 Lua VM |
| `update(dt)` | 每帧驱动协程 |
| `loadScript(path)` | 加载并执行 Lua 脚本 |
| `resumeKAGCoroutine` | 恢复 KAG 协程（点击后调用） |
| `lockdownScriptEnv` | 锁定脚本环境（沙箱） |
| `registerModules` | 注册 C++ 绑定模块（KAG, Render, VFX, Debug, DevCore） |
| `state` | 返回 `lua_State*` |
| `setInstructionBudget(n)` | 设置每帧最大指令数（防死循环） |
| `getInstructionBudget` | 当前指令预算 |
| `isInstructionBudgetExceeded` | 是否超出指令预算 |
| `resetInstructionBudget` | 重置指令预算计数器 |

**线程安全约束**：Lua VM 仅在主线程操作。Worker 线程绝不执行 Lua。多 VM 隔离（如 LuaLanes）可用于高级场景。

---

## 15. steam — Steamworks 集成

**实现**: SteamBackend（条件编译 `#ifdef CAESURA_HAS_STEAM`）

### ISteamBackend

| 分类 | 方法 | 说明 |
|------|------|------|
| **生命周期** | `init` | 初始化 Steam API |
| | `shutdown` | 关闭 |
| | `runCallbacks` | 每帧调用 `SteamAPI_RunCallbacks` |
| | `isOverlayActive` | Steam 覆盖层是否激活（暂停输入） |
| **成就** | `unlockAchievement(id)` | 解锁成就 |
| | `isAchievementUnlocked(id)` | 查询成就状态 |
| | `resetAchievement(id)` | 重置单个成就 |
| | `resetAllAchievements` | 重置所有成就 |
| **统计** | `setStatInt(name, value)` / `getStatInt(name)` | 整数统计 |
| | `setStatFloat(name, value)` / `getStatFloat(name)` | 浮点统计 |
| | `storeStats` | 刷新到 Steam 服务器 |
| **云存档** | `cloudWrite(fileName, data, size)` | 写入云存档 |
| | `cloudRead(fileName, buffer, maxSize)` | 读取云存档 |
| | `cloudFileSize(fileName)` | 云文件大小 |
| | `cloudFileExists(fileName)` | 云文件是否存在 |
| | `cloudDelete(fileName)` | 删除云文件 |
| | `cloudQuotaTotal` / `cloudQuotaUsed` | 云存储配额 |

---

## 16. storage — 存档/读档

### 16.1 ISaveManager

JSON 存档管理，支持加密和 schema 迁移。

| 方法 | 说明 |
|------|------|
| `init(saveDir)` | 初始化，指定存档目录 |
| `save(slot, data, sceneName, tokenIndex, thumbnailPng)` | 保存到指定槽位 |
| `load(slot, outMeta)` | 加载槽位，可选返回元数据 |
| `listSaves` | 列出所有存档槽位及元数据 |
| `slotExists(slot)` | 槽位是否存在 |
| `deleteSlot(slot)` | 删除槽位 |
| `setEncryptionKey(key[32])` | 设置 AES-256 加密密钥 |
| `clearEncryptionKey` | 清除密钥（存档不加密） |
| `isEncryptionEnabled` | 是否启用加密 |
| `setSaveProvider(provider)` | 注入自定义存储后端 |
| `getSaveProvider` | 获取当前存储后端 |
| `currentSchemaVersion` | 当前存档格式版本号 |
| `registerMigration(fromVer, toVer, fn)` | 注册 schema 迁移函数 |

### 16.2 ISaveProvider

抽象存储后端（本地文件、云同步等）。

| 方法 | 说明 |
|------|------|
| `readFile(path)` | 读取原始字节 |
| `writeFile(path, content)` | 写入原始字节 |
| `deleteFile(path)` | 删除文件 |
| `listFiles(pattern)` | 列出匹配文件 |
| `pushToCloud(slotPath)` | 推送存档到云端（默认 no-op） |
| `pullFromCloud(slotPath)` | 从云端拉取存档（默认 no-op） |
| `supportsCloudSync` | 是否支持云同步（默认 false） |

**默认实现**：`LocalFileSaveProvider` — 使用 `std::ifstream/std::ofstream`

---

## 附录 A: BackendRegistry 完整 getter 列表

```cpp
class BackendRegistry {
    IRenderDevice*       getRenderDevice();
    IPlatformBackend*    getPlatformBackend();
    IAudioBackend*       getAudioBackend();
    ILuaManager*         getLuaState();          // alias: getLuaManager()
    IInputRouter*        getInputRouter();
    ITextureManager*     getTextureManager();
    ILayerManager*       getLayerManager();
    IParticleSystem*     getParticleSystem();
    IGpuMonitor*         getGpuMonitor();
    IVideoPlayer*        getVideoPlayer();
    IDebugManager*       getDebugManager();
    IJobSystem*          getJobSystem();
    IAsyncLoader*        getAsyncLoader();
    IAnimationBackend*   getAnimationBackend();
    IMiniGameBackend*    getMiniGameBackend();
    ISaveManager*        getSaveManager();
    ISaveProvider*       getSaveProvider();
    IEditorServer*       getEditorServer();
    ITextureBudget*      getTextureBudget();
    ISteamBackend*       getSteamBackend();
    ISandboxQuota*       getSandboxQuota();
    // archive
    ICryptoEngine*       getCryptoEngine();
    IArchiveReader*      getArchiveReader();
    IArchiveWriter*      getArchiveWriter();

    // Lua helpers
    static IRenderDevice*   getRenderDeviceFromLua(lua_State* L);
    static IVideoPlayer*    getVideoPlayerFromLua(lua_State* L);
};
```

## 附录 B: 接口命名约定

- 接口文件：`src/<module>/api/I<ModuleName>.h`
- 接口类名：`I` + PascalCase（`IRenderDevice`, `IAudioBackend`）
- 纯虚类：所有方法 `= 0`，不包含数据成员
- 类型定义（枚举、结构体）如果被接口方法使用，放在接口头文件中
- 命名空间：所有公共类型在 `Caesura::` 下（archive 在 `Caesura::carc::`）