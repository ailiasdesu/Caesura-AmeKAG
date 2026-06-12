# Caesura (AmeKAG) 引擎架构拓扑图

> 生成日期: 2026-06-12 | 测试: 295/295 | 接口: 26

---

## 模块拓扑

```
                        ┌──────────────────────────────┐
                        │         main.cpp              │
                        │    (创建所有具体后端实例)        │
                        └──────────────┬───────────────┘
                                       │ EngineConfig
                                       ▼
                        ┌──────────────────────────────┐
                        │        entry (Engine)         │
                        │    组合根 · 4 阶段初始化       │
                        │    initPlatformPhase()         │
                        │    initScriptingPhase()        │
                        │    initAssetPhase()            │
                        │    initOptionalPhase()         │
                        └──────────────┬───────────────┘
                                       │ 注册所有后端
                                       ▼
                        ┌──────────────────────────────┐
                        │       di (BackendRegistry)     │
                        │   单一服务定位器 · 26 个 I*     │
                        └──┬──┬──┬──┬──┬──┬──┬──┬──┬──┘
                           │  │  │  │  │  │  │  │  │
          ┌────────────────┘  │  │  │  │  │  │  │  └────────────────┐
          ▼                   ▼  ▼  ▼  ▼  ▼  ▼  ▼                   ▼
   ┌──────────┐                                 ┌──────────┐
   │ platform │                                 │  script  │
   │ SDL3     │                                 │ Lua VM   │
   │ 窗口/事件 │                                 │ KAG 引擎 │
   └────┬─────┘                                 └────┬─────┘
        │                                            │
   ┌────┴─────┐                              ┌──────┴──────┐
   │  input   │                              │   storage   │
   │ 输入路由  │                              │  存档/读档   │
   └──────────┘                              └─────────────┘
                                                        
   ┌──────────┐   ┌──────────┐   ┌──────────┐
   │  render  │   │  audio   │   │ resource │
   │  bgfx    │   │ SoLoud   │   │ 资产管线  │
   │ 22 cpp   │   │ BGM/SE/  │   │ 异步加载  │
   │ 纹理/粒子 │   │ Voice    │   │ 图片解码  │
   │ 图层/Shader│  └──────────┘   └────┬─────┘
   │ 视频/RTT │                        │
   └──────────┘               ┌────────┴────────┐
                              │    archive      │
                              │ CARC 加密归档    │
                              │ AES-256 + zstd  │
                              └─────────────────┘

   ┌──────────┐   ┌──────────┐   ┌──────────┐
   │  live2d  │   │ minigame │   │  steam   │
   │ 动态立绘  │   │ 3D 小游戏 │   │ 成就/云存档│
   │ PNG 降级  │   │ 几何/碰撞 │   │ (条件编译) │
   └──────────┘   └──────────┘   └──────────┘

   ┌──────────┐   ┌──────────┐   ┌──────────┐
   │  debug   │   │   job    │   │   rpc    │
   │ 日志/性能 │   │ 任务系统  │   │ HTTP 编辑器│
   │ 热重载    │   │ 多线程   │   │ :9876    │
   └──────────┘   └──────────┘   └──────────┘
```

---

## 模块详细说明与数据流

### 1. entry — 引擎组合根 (Engine)
| 项目 | 说明 |
|------|------|
| 文件 | `Engine.cpp` (约 400 行), `Engine.h`, `EngineConfig.h` |
| 接口 | 无（组合根本身不暴露接口） |
| 职责 | 持有所有子系统的 `unique_ptr`，按 4 阶段初始化，生命周期管理 |
| 游戏中的作用 | 游戏启动时创建所有后端 → 主循环 → 游戏退出时销毁所有后端 |

**数据流：**
```
main.cpp → EngineConfig → Engine::init()
  → initPlatformPhase()   [SDL3 + bgfx + SoLoud + FreeType]
  → initScriptingPhase()  [Lua VM + KAG 模块 + HotReload]
  → initAssetPhase()      [JobSystem + AssetManager + TextureManager]
  → initOptionalPhase()   [Steam + Live2D + MiniGame + CryptoEngine]
→ 主循环: processEvents → update → render
```

---

### 2. di — 依赖注入 (BackendRegistry)
| 项目 | 说明 |
|------|------|
| 接口 | `ISandboxQuota`, `ITextureBudget` |
| 职责 | 存储 26 个 `I*` 接口指针，提供 `get*()` 全局访问 |
| 游戏中的作用 | 所有模块通过它发现彼此。脚本调用 `BackendRegistry:getRenderDevice()` 获取渲染器 |

**数据流：**
```
Engine::init() → BackendRegistry::setRenderDevice(ptr)
Lua 脚本 → BackendRegistry::getRenderDevice() → IRenderDevice*
任何模块 → BackendRegistry::getLuaState() → lua_State*
```

---

### 3. platform — 平台抽象层
| 项目 | 说明 |
|------|------|
| 接口 | `IPlatformBackend` |
| 实现 | `SDL3PlatformBackend`, `MobileAdapter` |
| 游戏中的作用 | 创建窗口、处理系统事件（最小化/恢复）、轮询输入 |

**数据流：**
```
OS → SDL3 → SDL3PlatformBackend::pollEvent() → InputRouter
      → SDL3PlatformBackend::getNativeWindow() → BgfxRenderDevice::init()
```

---

### 4. input — 输入路由
| 项目 | 说明 |
|------|------|
| 接口 | `IInputRouter` |
| 实现 | `InputRouter` |
| 游戏中的作用 | SDL 事件 → KAG 模式（点击推进对话）/ GAME 模式（自定义回调）|

**数据流：**
```
SDL_Event → InputRouter::processEvent()
  ├─ KAG mode → KAG callback → Lua coroutine resume (推进脚本)
  └─ GAME mode → registered game callbacks
窗口 resize → InputRouter::notifyResize() → Lua layers.lua 重建图层
```

---

### 5. script — Lua 脚本引擎
| 项目 | 说明 |
|------|------|
| 接口 | `ILuaManager` |
| 子模块 | `vm/LuaManager`, `state/GameState`, `bindings/` (7 个绑定) |
| 游戏中的作用 | 执行 KAG 脚本，管理游戏状态，提供 Lua API |

**数据流：**
```
demo_story.ks → tokenizer.lua → parser.lua → conductor.lua
  → @text → KAGBinding::play_bgm() → BackendRegistry::getAudioBackend()
  → @bg   → KAGBinding::render_texture() → BackendRegistry::getRenderDevice()
  → @choice → scheduler 暂停 → 等待 InputRouter 点击 → 恢复协程
GameState: 全局跨脚本持久化表 (flags, scene_data 等)
```

---

### 6. render — 渲染系统（最大模块）
| 项目 | 说明 |
|------|------|
| 接口 | `IRenderDevice`, `IGpuMonitor`, `IVideoPlayer`, `ITextureManager`, `IParticleSystem`, `ILayerManager` |
| 核心类 | `BgfxRenderDevice`, `BgfxDeviceCore`, `BgfxDraw`, `BgfxShaderManager` |
| 游戏中的作用 | 所有画面输出：背景、立绘、文字、粒子、视频、过渡效果 |

**数据流（一帧）：**
```
Engine::render()
  → BgfxRenderDevice::beginFrame()
  → LayerManager::render(LayerType::BG, ...)   // 背景
  → LayerManager::render(LayerType::FG, ...)   // 立绘
  → ParticleSystem::render()                    // 粒子
  → TextRenderer::renderText()                  // 文字
  → LayerManager::render(LayerType::MSG, ...)   // 消息框
  → BgfxDraw::submitBlend/Transition/VFX()     // 后处理
  → BgfxRenderDevice::endFrame()
  → BgfxRenderDevice::commit_frame()
```

---

### 7. audio — 音频系统
| 项目 | 说明 |
|------|------|
| 接口 | `IAudioBackend` |
| 实现 | `SoLoudAudioEngine`, `NullAudioBackend` |
| 游戏中的作用 | BGM 播放/淡入淡出、音效播放、语音播放 |

**数据流：**
```
@bgm "bgm_01.mp3" → KAGBinding → BackendRegistry::getAudioBackend()
  → SoLoudAudioEngine::playBGM(path) → SoLoud 内部解码 → WASAPI/WINMM 输出
@se "click.wav" → playSE(path) → 音效总线 → 混音 → 输出
@fadeBGM → fadeVolume("bgm", target, duration) → 逐帧插值
```

---

### 8. resource — 资源管线
| 项目 | 说明 |
|------|------|
| 接口 | `IAssetProvider`, `IAsyncLoader` |
| 游戏中的作用 | 加载图片/音频/脚本，异步解码，纹理上传 |

**数据流：**
```
@bg "scene.png" → AssetManager::exists() → 查找
  ├─ 磁盘路径 → stb_image 解码 → TextureManager::loadTexture()
  └─ CARC 归档 → CarcAssetProvider::read() → stb_image → TextureManager

异步加载：
  AsyncLoader::enqueue(path) → JobSystem worker 线程解码
  → 完成事件 → 主线程 TextureManager::loadTextureFromRGBA()
```

---

### 9. archive — 归档与加密
| 项目 | 说明 |
|------|------|
| 接口 | `IArchiveReader`, `IArchiveWriter`, `ICryptoEngine` |
| 游戏中的作用 | 打包发布（CARC），加密存档 |

**数据流：**
```
发布：CARCWriter::create("game.carc") → addFile("scene.png") → finalize()
      → AES-256-GCM 加密 → zstd 压缩 → Ed25519 签名

运行时：CARCReader::open("game.carc") → readFile("scene.png")
         → 解密 → 解压 → 原始数据

存档加密：
SaveManager::save() → CryptoEngine::encrypt(data, key) → 写入磁盘
SaveManager::load() → CryptoEngine::decrypt(data, key) → JSON 解析
```

---

### 10. storage — 存档系统
| 项目 | 说明 |
|------|------|
| 接口 | `ISaveManager`, `ISaveProvider` |
| 游戏中的作用 | 保存/读取进度、存档列表、加密、云同步 |

**数据流：**
```
@save 1 → SaveManager::save(1, gameData)
  → JSON 序列化 → (可选) CryptoEngine 加密 → 写入 disk/cloud
@load 1 → SaveManager::load(1)
  → 读取 → (可选) 解密 → JSON 解析 → 恢复 GameState
```

---

### 11. live2d — 动态立绘
| 项目 | 说明 |
|------|------|
| 接口 | `IAnimationBackend` |
| 实现 | `Live2DBackend` (需 Cubism SDK), `NullAnimationBackend` (PNG 降级) |
| 游戏中的作用 | 角色立绘动画：呼吸、眨眼、表情切换、口型同步 |

**数据流：**
```
@fg "character.moc" → IAnimationBackend::loadModel()
  ├─ Cubism SDK 可用 → Live2DBackend → 动态形变渲染（60fps）
  └─ SDK 不可用   → NullAnimationBackend → 静态 PNG 渲染
每帧：update(dt) → render() → bgfx 提交到 FG 图层
```

---

### 12. minigame — 3D 小游戏
| 项目 | 说明 |
|------|------|
| 接口 | `IMiniGameBackend` |
| 实现 | `BgfxMiniGameBackend`, `NullMiniGameBackend` |
| 游戏中的作用 | galgame 中嵌入 3D 小游戏场景（如射击/解谜） |

**数据流：**
```
@minigame "shooter" → IMiniGameBackend::loadScene()
  → 加载 GLB 模型 → 几何/碰撞初始化
每帧：update(dt) → MiniCollision 检测 → render() → bgfx 提交到 MINIGAME 视图
```

---

### 13. steam — Steamworks 集成
| 项目 | 说明 |
|------|------|
| 接口 | `ISteamBackend` |
| 实现 | `SteamBackend` (需 SDK), `NullSteamBackend` |
| 游戏中的作用 | Steam 成就解锁、云存档同步、统计数据上报 |

---

### 14. debug — 调试与性能分析
| 项目 | 说明 |
|------|------|
| 接口 | `IDebugManager` |
| 游戏中的作用 | 日志记录、性能采样帧、断点调试、热重载脚本 |

**数据流：**
```
DEBUG_ERR(Engine, code, "msg") → DebugManager::log()
  → 环形缓冲区 → 写入日志文件 + 控制台输出
PROFILE_SCOPE("render") → beginProfile/endProfile → FrameProfile
HotReload: scripts/ 目录监控 → 文件变化 → 重新加载 Lua 模块
```

---

### 15. job — 多线程任务系统
| 项目 | 说明 |
|------|------|
| 接口 | `IJobSystem` |
| 游戏中的作用 | 异步资源解码、粒子并行更新、视频解码 |

**数据流：**
```
AsyncLoader → JobSystem::submit(decodeTask)
VideoPlayer → JobSystem::submit(decodeFrame)
ParticleSystem → JobSystem::submit(processSimBatch) → 多 worker 并行
```

---

### 16. rpc — HTTP 编辑器服务器
| 项目 | 说明 |
|------|------|
| 接口 | `IRpcServer`, `IEditorServer` |
| 游戏中的作用 | web-editor 通信桥梁：执行脚本、预览 Live2D、打包项目 |

**数据流：**
```
web-editor (React) → HTTP :9876 → EditorServer
  GET  /api/status        → 引擎状态
  POST /api/run           → 执行 Lua 脚本 → LuaManager
  GET  /api/live2d/models → 模型列表
  POST /api/live2d/load   → 加载模型 → IAnimationBackend
  POST /api/build         → 打包 CARC → CARCWriter
```

---

## 一帧完整数据流（Galgame 运行时）

```
1. platform: SDL3PlatformBackend::pollEvent()
     → mouse click / key press
2. input: InputRouter::processEvent(SDL_Event)
     → KAG mode → lua_resume(coroutine)
3. script: conductor.lua 执行下一行 KAG 命令
     → @text "hello" → TextRenderer 准备文字
     → @bg "scene.png" → AssetManager 加载纹理
     → @bgm "music.mp3" → SoLoudAudioEngine 播放
4. Engine::update(dt)
     → LuaManager::update() → 垃圾回收
     → ParticleSystem::update() → 粒子物理
     → AnimationBackend::update() → Live2D 形变
     → AsyncLoader::poll() → 完成回调
5. Engine::render()
     → BgfxRenderDevice::beginFrame()
     → LayerManager::render(BG)  → 背景纹理
     → LayerManager::render(FG)  → 立绘纹理
     → TextRenderer::render()    → 文字字形
     → ParticleSystem::render()  → 粒子效果
     → LayerManager::render(MSG) → 消息框
     → BgfxDraw::submitVFX()     → 后处理 (抖动/模糊)
     → BgfxRenderDevice::endFrame()
6. GpuMonitor::update() → 检查性能 → 必要时降级
7. 下一帧开始
```
