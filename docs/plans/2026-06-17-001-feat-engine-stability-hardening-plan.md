---
date: 2026-06-17
type: feat
origin: docs/brainstorms/2026-06-17-engine-stability-requirements.md
---

## Summary

逐模块加固引擎稳定性——从 L0 零依赖模块到 L4 组合根逐层修复 null 指针、资源生命周期和异常路径——然后用完整 galgame demo 本地验证端到端无崩溃。

## Problem Frame

引擎 16 模块 ~400 测试通过，但端到端 galgame 流程跑不通。调研发现大量具体问题：BackendRegistry 21 个 getter 全部返回原始裸指针无 null 检查，DebugBinding 10 处/VFXBinding 11 处/RenderBinding 8 处直接解引用未注册的后端；Engine::init() 有重复 `createGpuMonitor` 调用和双次 `miniGameBackend->init()`；init 失败无回滚；`m_shutdownComplete` 守卫仅 3 个文件使用。需求文档定义了 13 条要求 (R1-R13) 和 5 个验收示例。

## Requirements

### 防御性加固 (origin R1-R6)

- R1. BackendRegistry getter 返回 null 时调用方不崩溃——降级或跳过。
- R2. 每个模块 init/shutdown 幂等且可安全重入。
- R3. 文件 I/O 失败返回错误码，不抛异常穿透模块边界。
- R4. bgfx 初始化失败时引擎降级运行不崩溃。
- R5. Lua 脚本错误被 pcall 捕获，不导致引擎崩溃。
- R6. 异步加载失败时回调携带错误信息，调用方有超时兜底。

### 加固分层 (origin R7-R11)

- R7. L0 (platform/input/steam): SDL 失败 fallback，null input router 保护，Steam 条件编译隔离。
- R8. L1 (audio/job/debug/archive/storage): SoLoud fallback，线程池析构安全，ring buffer 溢出保护，文件损坏恢复。
- R9. L2 (resource/live2d/minigame/render/rpc): 图片解码失败占位，null backend 无副作用，RPC 端口占用降级。
- R10. L3 (di/script): BackendRegistry getter null 守卫，Lua 绑定层参数校验。
- R11. L4 (entry): Engine::init() 四阶段失败回滚，ErrorUI 安全渲染。

### Demo 验证 (origin R12-R13)

- R12. Demo 覆盖: title → ≥3 场景 → ≥2 角色立绘 → 选项分支 → 双结局 → 存档/读档 → CG 画廊 → 音乐室。
- R13. Demo 连续 5 次执行无崩溃。

## Key Technical Decisions

- **KTD1: 内联 null 守卫而非创建 Null*Backend 类。** 对已有 Null 后端的模块用 null backend 降级；对没有的模块（DebugManager、ParticleSystem、AsyncLoader 等）在每个调用点加 null 检查——比新增 8 个 Null 类更小改动面。(origin R1)
- **KTD2: 每层加固后本地构建+运行已有测试验证不回归。** 不修改测试逻辑，只确保已有 ~400 测试通过数不减少。(origin F1)
- **KTD3: `m_shutdownComplete` 守卫作为幂等性标准模式。** Engine/BgfxDeviceCore/BgfxRenderDevice 已有此模式——传播到 audio、job、minigame 等有状态的模块。模式: `shutdown()` 首行检查 `if (m_shutdownComplete) return;`，末行设置 `m_shutdownComplete = true`。(origin R2)
- **KTD4: 不从 Engine::init() 中移除任何已有逻辑——仅加保护和修复重复调用 bug。** `createGpuMonitor` 三行重复和 `m_miniGameBackend->init()` 双次调用属于明确的 bug 修复。其他逻辑路径保持不变。(origin 假设: P1-P3 命令逻辑正确)
- **KTD5: Demo 脚本和资源本地手写，不依赖外部资源管线。** 使用纯色占位图 + 静音音频 + 最小 KAG 场景脚本，确保 demo 可独立运行。(origin Q2)

## Implementation Units

### U1. L0 零依赖模块加固

- **Goal:** platform/input/steam 在异常路径下不崩溃
- **Requirements:** R1, R7
- **Dependencies:** none
- **Files:**
  - `src/platform/SDL3PlatformBackend.cpp` — SDL init 失败返回 false，getNativeWindowHandle 失败返回 nullptr
  - `src/input/InputRouter.cpp` — processEvent 在被注入前丢弃事件不崩溃；添加 m_shutdownComplete 守卫
  - `src/steam/SteamBackend.cpp` — init/runCallbacks 失败不抛异常
  - `src/steam/NullSteamBackend.h` — 确认所有方法返回安全默认值
- **Approach:**
  1. SDL3PlatformBackend: 检查 `SDL_Init` 返回值，失败时记录错误不 abort
  2. InputRouter: 构造函数默认未注入，`processEvent` 和 `setFocus` 加 `if (!m_registered) return;`
  3. SteamBackend: 确认 `#ifdef CAESURA_HAS_STEAM` 隔离完整；NullSteamBackend 所有方法返回 false/0/nullptr 已验证（commit `1db7ab6e` 已修 Ed25519 偏移 bug）
- **Execution note:** 先加 null 守卫再改行为——这层改动最少，但建立后续层的模式
- **Patterns to follow:**
  - NullSteamBackend 已有完整安全默认值（参考 agent 调研）
  - `docs/solutions/architecture-patterns/header-only-to-instance-class.md` 模式用于 Null 类重构
- **Test scenarios:**
  - SDL init fails → `initPlatformPhase()` 返回 false → Engine::init() 返回 false
  - InputRouter 未注入时 `processEvent(SDL_QUIT)` → 不崩溃，事件丢弃
  - InputRouter 重复 init → 不泄漏，第二次 init 幂等
  - NullSteamBackend::init() → 返回 false，所有后续调用返回安全值
- **Verification:** `cmake --build build --config Debug` 零错误；`./CaesuraTests.exe -tc="*platform*,*input*,*steam*"` 全部通过

### U2. L1 基础服务模块加固

- **Goal:** audio/job/debug/archive/storage 在异常路径下不崩溃
- **Requirements:** R1, R2, R3, R8
- **Dependencies:** U1
- **Files:**
  - `src/audio/SoLoudAudioEngine.cpp` — init 失败后所有方法安全返回；添加 m_shutdownComplete
  - `src/audio/NullAudioBackend.h` — 确认 24 个虚方法全返回安全默认值
  - `src/job/JobSystem.cpp` — 析构函数等待所有 worker 退出 + m_shutdownComplete；submit 在 shutdown 后返回 0
  - `src/debug/DebugManager.cpp` — ring buffer 溢出截断旧数据（已有）；null DebugManager 时 DEBUG_* 宏不崩溃
  - `src/archive/CARCReader.cpp` — 文件损坏/CRC 失败/解密失败返回空结果不抛异常
  - `src/archive/CryptoEngine.cpp` — 加密/解密失败不抛异常
  - `src/storage/SaveManager.cpp` — 存档文件损坏返回错误码不崩溃；schema migration 失败保留原文件
- **Approach:**
  1. SoLoudAudioEngine: init 失败设置 `m_initialized = false`，所有 play/stop/volume 方法检查此标志
  2. JobSystem: `~JobSystem()` 调用 `shutdown()`（如果未调用）；shutdown 设置 `m_shutdownComplete`；`submit()` 在 shutdown 后返回 0（对齐 NullJobSystem 行为）
  3. DebugManager: `DEBUG_*` 宏调用 `DebugManager::instance()` 前检查是否已初始化——这是唯一的 BackendRegistry 例外（AGENTS.md 规则 7.4）
  4. Archive: CARCReader 在 CRC/解密失败时返回空 `vector<uint8_t>` + 记录 error；调用方检查 `empty()` 而不是假设成功
  5. Storage: SaveManager::load() 失败返回 null JSON + 错误码；schema migration 失败保留 `.bak` 备份
- **Execution note:** 每个文件改完后本地构建验证；改完所有文件后运行该模块已有测试
- **Patterns to follow:**
  - `m_shutdownComplete` 模式: Engine.h:73, BgfxDeviceCore.h:119-120, BgfxRenderDevice.h:81
  - NullAudioBackend 的 24 个无操作安全默认方法
  - Ring buffer 模式: DebugManager 已有 `m_ringBuffer[m_writeIndex % RING_SIZE]`
- **Test scenarios:**
  - SoLoud init 失败 → playBgm 返回 false，不崩溃
  - JobSystem::shutdown() 后 submit() → 返回 0（对齐 NullJobSystem 测试 `test_null_jobsystem.cpp:38`）
  - JobSystem 两次 shutdown → 第二次幂等，不崩溃
  - CARC 文件 CRC 错误 → read() 返回空 vector，error code 设置
  - SaveManager load 损坏文件 → 返回 nullptr，不崩溃
  - Schema migration v4→v5 失败 → 原始文件保留为 `.bak`
- **Verification:** `./CaesuraTests.exe -tc="*audio*,*job*,*debug*,*carc*,*save*"` 全部通过

### U3. L2 功能模块加固

- **Goal:** resource/live2d/minigame/render/rpc 异常降级不崩溃
- **Requirements:** R1, R3, R4, R6, R9
- **Dependencies:** U2
- **Files:**
  - `src/resource/ImageDecoder.cpp` — 添加空输入/超大尺寸/损坏文件保护（部分已有，审查完整性）
  - `src/resource/AsyncLoader.cpp` — 超时机制；enqueue 失败时不泄漏 CompletedLoad
  - `src/live2d/NullAnimationBackend.cpp` — 确认 PNG fallback 路径在无 TextureManager 时不崩溃
  - `src/minigame/NullMiniGameBackend.cpp` — 确认所有 no-op 方法安全；添加 m_shutdownComplete
  - `src/render/BgfxRenderDevice.cpp` — init 失败设置 m_shutdownComplete，所有 draw 方法检查；对齐已有 BgfxDeviceCore 守卫
  - `src/render/NullGpuMonitor.h` — 确认所有方法返回安全值
  - `src/rpc/EditorServer.cpp` — 端口占用时 start() 返回 false 不崩溃；已有 AnimationBackend null 检查（line 314-318），扩展模式到其他 getter
  - `src/rpc/RpcServer.cpp` — JSON 解析失败返回 error response 不崩溃
- **Approach:**
  1. ImageDecoder: 空输入（size=0/null data）→ 返回 `{.ok=false}`（已有 stb 路径，验证 bimg 路径）
  2. AsyncLoader: 添加加载超时（已有 m_shutdownComplete，复用模式）；postCompleteEvent 的 heap 分配在事件未消费时不泄漏（Engine::processEvents 保证消费）
  3. NullAnimationBackend: `getTextureManager()` 已检查 null（line 27），确认所有路径
  4. BgfxRenderDevice: init 失败后 `m_shutdownComplete = true`（对齐 BgfxDeviceCore::init 的 fallback 模式）
  5. EditorServer: 端口占用 → httplib 返回错误，start() 返回 false；所有 getter 调用前加 null 检查
  6. 确认 `docs/solutions/deferred-gpu-tests.md` 中记录的 `captureThumbnailPNG` 依赖可以继续延期
- **Patterns to follow:**
  - BgfxDeviceCore::init() 两级 fallback (line 64-76): 首选后端 → 自动选择
  - NullAnimationBackend PNG fallback 的 `getTextureManager()` null 检查模式
- **Test scenarios:**
  - 空 buffer 传给 ImageDecoder::decode() → 返回 `.ok=false`
  - AsyncLoader::enqueue("", "texture") → 返回 -1 (empty path rejection)
  - bgfx init 失败 → BgfxRenderDevice::draw() 返回 early，不崩溃
  - EditorServer::start() 端口已占用 → 返回 false，日志记录
  - NullAnimationBackend::draw() 无 TextureManager → 不崩溃，跳过渲染
- **Verification:** `./CaesuraTests.exe -tc="*image*,*async*,*live2d*,*minigame*,*render_device*,*rpc*"` 全部通过

### U4. L3 枢纽模块加固

- **Goal:** BackendRegistry 所有 getter 含 null 守卫；Lua 绑定层参数校验完整
- **Requirements:** R1, R5, R10
- **Dependencies:** U3
- **Files:**
  - `src/di/BackendRegistry.cpp` — getVideoPlayerFromLua 和 getMiniGameBackendFromLua 加 null fallback
  - `src/script/bindings/DebugBinding.cpp` — 10 处 `getDebugManager()` 解引用全加 null 检查
  - `src/script/bindings/VFXBinding.cpp` — 11 处 `getParticleSystem()` 解引用全加 null 检查
  - `src/script/bindings/RenderBinding.cpp` — 8 处 `getTextureManager()`/`getAsyncLoader()` 加 null 检查
  - `src/script/bindings/UnifiedBinding.cpp` — 2 处 `getAsyncLoader()` 加 null 检查
  - `src/script/bindings/DevCoreBinding.cpp` — 确认已有 null 守卫完整性
  - `src/script/bindings/KAGBinding.cpp` — 确认已有 null 守卫完整性
- **Approach:**
  1. BackendRegistry: `getVideoPlayerFromLua` 加 fallback（对齐现有的 `getRenderDeviceFromLua` 模式: `BackendRegistry.cpp:281-287`——优先 Lua 注入，回退到 BackendRegistry）
  2. DebugBinding: 每个函数开头: `auto* dm = BackendRegistry::instance().getDebugManager(); if (!dm) return 0;`（10 处）
  3. VFXBinding: 每个函数开头: `auto* ps = BackendRegistry::instance().getParticleSystem(); if (!ps) return 0;`（11 处）
  4. RenderBinding: 每个使用 TextureManager/AsyncLoader 的函数开头加 null 守卫（8 处）
  5. UnifiedBinding: 2 处 AsyncLoader 加 null 守卫
  6. **不修改已有参数校验**——所有绑定文件的 `luaL_check*` 调用已完整（agent 调研确认）
- **Execution note:** 这是加固的核心——BackendRegistry null 守卫问题占比最大（agent 调研标记为 P0 级别），每个文件改动模式一致
- **Patterns to follow:**
  - `getRenderDeviceFromLua` 双 fallback 模式: `BackendRegistry.cpp:281-287`
  - DebugBinding null 守卫模板（未在此 U4 修改，仅应用）
- **Test scenarios:**
  - Covers AE4. Lua 调用未注入 ParticleSystem 的 emit → 返回 0，不 segfault
  - DebugManager 未注册时调用 `debug.errorCount()` → 返回 0
  - TextureManager 未注册时调用 `render.loadTexture("test.png")` → 返回 nil, error message
  - AsyncLoader 未注册时调用 `render.loadAsync("test.png", cb)` → 返回 -1, error message
  - VideoPlayer 未注入时 Lua 调用 → fallback 到 BackendRegistry，不崩溃
- **Verification:** `./CaesuraTests.exe -tc="*binding*,*kag*,*vfx*,*debug*,*render*"` 全部通过

### U5. L4 组合根修复

- **Goal:** Engine::init() 失败回滚；修复已知重复调用 bug；ErrorUI 安全渲染
- **Requirements:** R1, R2, R4, R11
- **Dependencies:** U4
- **Files:**
  - `src/entry/Engine.cpp` — 修复 createGpuMonitor 三行重复（line 197-201）；修复 m_miniGameBackend->init() 双次调用（line 278-279）；添加 init 失败回滚；Engine::renderDevice/audio/platform 访问器加 null 检查
  - `src/entry/Engine.h` — miniGame/animation/lua/inputRouter/gpuMonitor/videoPlayer 访问器加 null 检查（line 43-48）
  - `src/entry/Engine_Gpu.cpp` — 确认 NullGpuMonitor 在 headless 模式的选择逻辑
  - `src/render/BgfxRenderDevice.cpp` — 确认已有 m_shutdownComplete 守卫（commit `e26c15a5` 已添加）
  - `src/main.cpp` — getTextureManager() 调用处加 null 检查（line 244）
- **Approach:**
  1. **Bug 修复**:
     - Line 197-201: 替换三行重复 `m_gpuMonitor = createGpuMonitor(m_config.headless)` 为单行
     - Line 278-279: 删除重复的 `m_miniGameBackend->init()` 调用
  2. **失败回滚**: init 函数保存已初始化阶段标记；任一阶段返回 false 时 ~Engine() 的 shutdown() 已处理清理（`Engine::~Engine()` line 86-88 调用 shutdown），但需确保 shutdown 幂等。添加 `m_initPhase` 枚举跟踪当前阶段，shutdown 只清理已初始化的阶段
  3. **访问器加固**:
     - `renderDevice()` 返回 `IRenderDevice*` 替代 `IRenderDevice&`（或加断言/检查）——这是 API 变更，保守做法: 维持返回引用但内部断言非 null
     - 实际最小改动: 在 `render()` 和 `run()` 的访问点加 null 检查，不改 API 签名
  4. **main.cpp**: getTextureManager() 解引用前加 null 检查
  5. **bgfx::getCaps()**: 在 line 166 和 549 的调用前加 `bgfx::isValid()` 守卫
- **Patterns to follow:**
  - m_shutdownComplete 守卫: Engine.h:73, Engine.cpp:691-692
  - Headless 路径: Engine.cpp:159-162, Engine_Gpu.cpp:13
  - `docs/solutions/architecture-patterns/engine-constructor-sigsegv-testing.md` 的 GpuMonitor 分离模式
- **Test scenarios:**
  - Engine::init() phase 1 失败 → ~Engine() shutdown 不崩溃（幂等）
  - Engine::init() phase 2 失败 → ~Engine() 只清理 phase 1 资源
  - headless=true 时 createGpuMonitor → 返回 NullGpuMonitor
  - Engine::renderDevice() 未注册时 → 返回 nullptr（断言失败但不崩溃）
  - Covers AE2. bgfx 未初始化 → 引擎降级 headless 模式运行
  - Covers AE3. 连续 5 次 init/shutdown 循环 → 不泄漏，不崩溃
- **Verification:** `./CaesuraTests.exe -tc="*engine*,*entry*"` 全部通过

### U6. Demo 脚本与端到端验证

- **Goal:** 编写并验证一个覆盖 8 个节点的完整 galgame demo
- **Requirements:** R12, R13
- **Dependencies:** U5
- **Files:**
  - `scripts/demo_stability.ks` (new) — 完整 galgame 流程 KAG 脚本
  - `assets/demo/bg_title.png` (new) — 标题占位图（纯色 1920x1080）
  - `assets/demo/bg_scene1.png` (new) — 场景占位图
  - `assets/demo/bg_scene2.png` (new) — 场景占位图
  - `assets/demo/ch_hero.png` (new) — 角色立绘占位图
  - `assets/demo/ch_heroine.png` (new) — 角色立绘占位图
  - `assets/demo/bgm_01.ogg` (new) — 静音音频占位
  - `assets/demo/voice_01.ogg` (new) — 静音音频占位
  - `config/music_room.lua` — 确认 demo BGM 条目
- **Approach:**
  1. 用外部工具生成占位资源: 纯色 PNG (bgfx 可渲染的最小有效图片) + 静音 OGG (SoLoud 可播放的最小有效音频)
  2. 编写 demo KAG 脚本覆盖 origin R12 全部节点:
     ```
     *start → [bg] → [ch] 双角色交替 → [p] 点击推进 →
     [jump] 场景切换 → [if] 选项分支 → 双结局 →
     [save]/[load] → 画廊模式 → 音乐室模式 → [end]
     ```
  3. 启动引擎: `./CaesuraAmeKAG.exe` → 加载 demo → 人工或脚本推进
  4. 记录崩溃点和异常行为，修复后重新跑至 5 次无崩溃
- **Test scenarios:**
  - Covers AE1. 不启动音频后端 → demo 仍可执行，文本推进正常
  - Covers AE5. Demo 连续 5 次从 title 到 ending → 无崩溃
  - 选项分支 → choice 1 到结局 A，重跑 choice 2 到结局 B
  - 存档后退出重启 → 读档回到存档点
  - 画廊模式 → 显示已解锁 CG 列表
  - 音乐室模式 → 播放/停止 BGM 预览
- **Verification:** Demo 在本地 Windows 环境连续 5 次从头到尾执行无崩溃；所有 8 个流程节点可手动到达

## Scope Boundaries

### Deferred for later (carried from origin)
- CI 修复与 CI 上跑 demo 测试
- KAG 逻辑 bug 修复
- 性能优化
- 新功能开发 (P3 3D 小游戏等)
- 缩略图 `captureThumbnailPNG`

### Outside this product's identity (carried from origin)
- 更换渲染后端
- 跨平台 demo 验证（本次仅 Windows 本地）

### Deferred to Follow-Up Work
- 为 8 个缺失 Null 后端的模块（DebugManager、ParticleSystem、AsyncLoader、TextureManager、InputRouter、VideoPlayer、LayerManager、TextureBudget）创建正式 Null* 类——当前用内联 null 检查覆盖，长期应创建正式 Null 类对齐现有 NullAudioBackend 模式
- 将 `docs/design/engine-safety-and-qa-mechanisms.md` 从 AGENTS.md 移除引用或创建该文档（agent 调研发现该文件不存在但 AGENTS.md 引用了它）

## Risks & Dependencies

| Risk | Severity | Mitigation |
|---|---|---|
| U4 绑定层大面积加 null 守卫可能意外改变 Lua 脚本行为 | Medium | 每个守卫返回安全默认值（0、false、nil），与 Null*Backend 行为一致 |
| Init 回滚实现可能引入新的资源泄漏 | Medium | U5 只添加阶段标记，依赖已有 ~Engine() shutdown 路径；不改动 shutdown 逻辑本身 |
| Demo 占位资源可能触发 bgfx/SoLoud 边界条件 bug | Low | 使用最小有效格式（纯色 PNG、静音 OGG），已知兼容 |
| 改动面大（~30 文件），可能引入编译错误 | Low | 每层加固后立即本地构建验证 |

## Open Questions

- Q1 (origin, deferred to implementation): bgfx init 失败时 headless 模式当前支持到什么程度？U5 修复后在本地验证；agent 调研确认 Engine 已有 headless 路径，`registerNullBackends()` 注册 NullRenderDevice/NullPlatformBackend，但 NullGpuMonitor 是唯一 render null ——当前 headless 可运行 Lua 但不可渲染。
- Q2 (origin, deferred to implementation): Demo 资源用 ImageMagick/ffmpeg 生成纯色 PNG 和静音 OGG——脚本自动化避免手动资产准备。

## Sources / Research

- Origin requirements: `docs/brainstorms/2026-06-17-engine-stability-requirements.md`
- Engine init code: `src/entry/Engine.cpp` (lines 94-300, 530-549)
- BackendRegistry: `src/di/BackendRegistry.h` (lines 37-159), `src/di/BackendRegistry.cpp` (lines 17-55 for Null backends, 281-322 for FromLua wrappers)
- Null backends: `src/audio/NullAudioBackend.h`, `src/live2d/NullAnimationBackend.cpp`, `src/minigame/NullMiniGameBackend.cpp`, `src/render/NullGpuMonitor.h`, `src/steam/NullSteamBackend.h`
- Shutdown guard pattern: `src/entry/Engine.h:73`, `src/render/BgfxDeviceCore.h:119-120`, `src/render/BgfxRenderDevice.h:81`
- Crash fix patterns (git): `e26c15a5` (double bgfx shutdown), `465baaf2` (null TextureManager), `195a3fcd` (double delete steam), `843d953f` (RTT handle overwrite)
- Binding null check patterns: `src/script/bindings/KAGBinding.cpp` (positive), `src/script/bindings/DebugBinding.cpp` (missing — 10 sites P0), `src/script/bindings/VFXBinding.cpp` (missing — 11 sites P0)
- Existing solutions: `docs/solutions/architecture-patterns/engine-constructor-sigsegv-testing.md`, `docs/solutions/deferred-gpu-tests.md`
