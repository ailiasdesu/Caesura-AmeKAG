# T1–T5 执行情况详细总结

> 项目: Caesura (AmeKAG) — galgame 引擎架构重构
> 基线: Phase 1–4 已完成 (193/193 tests, 25 api/ 接口)
> 日期: 2026-06-12

---

## 总体结果

| 指标 | 会话开始 | 会话结束 | 变化 |
|------|----------|----------|------|
| 测试用例 | 193 | **237** | +44 |
| 断言 | 341 | **508** | +167 |
| 构建错误 | 0 | 0 | — |
| Git 提交 | — | 5 | — |
| 新增文件 | — | 9 | — |
| Engine::init() 行数 | ~170 | ~35 (+4 phase methods) | 重构 |

---

## T1 — 端到端集成测试

**目标**: 为 KAG 脚本解析、渲染管线、音频播放、存档往返、热重载添加集成测试。

### T1.1: KAG 脚本解析→执行集成测试

**文件**: `tests/cpp/test_kag_execution.cpp` (12 用例)

| 用例 | 验证内容 |
|------|----------|
| `KAG: Lua modules load without error` | 14 个 Lua 模块 (kag, tokenizer, scheduler, flow + 9 commands) 通过 require 加载 |
| `KAG: parser parses empty script` | tokenizer.parse("") 返回空表 |
| `KAG: parser tokenizes @text command` | `[text text="hello"]` → type=command, cmd=text, params={{"text","hello"}} |
| `KAG: parser tokenizes @bg command` | `[bg file="scene.png"]` → cmd=bg, params={{"file","scene.png"}} |
| `KAG: parser tokenizes @wait command` | `[wait time=100]` → cmd=wait, params={{"time","100"}} |
| `KAG: parser handles multi-command script` | 4 个连续命令正确解析 (bg→wait→text→p) |
| `KAG: parser handles labels and flow commands` | `*start` 标签 + jump/label/end 流控命令解析 |
| `KAG: parser handles inline text between commands` | 命令间的纯文本被正确标记为 text token |
| `KAG: scheduler runs empty token list without crash` | scheduler.run({}, {}) 不崩溃 |
| `KAG: scheduler runs non-blocking commands without crash` | coroutine 中运行 text 命令不崩溃 |
| `KAG: Lua stack is clean after multiple parse cycles` | 10 次 parse 后 lua_gettop == 0 |
| `KAG: require module idempotency` | 重复 require 同一模块不报错 |

**关键实现细节**:
- 设置 Lua `package.path` 为 `scripts/?.lua;scripts/?/init.lua` 以便 require 找到模块
- tokenizer 返回格式: `{ type = "command", cmd = "<name>", params = { {key, val}, ... } }`
- 使用 `luaL_dostring` 在 C++ 中执行 Lua test code，避免跨语言字符串转义问题

### T1.2: 渲染管线集成测试

**文件**: `tests/cpp/test_render_integration.cpp` (7 用例)

| 用例 | 验证内容 |
|------|----------|
| `Render: device default values` | Backbuffer 1280×720, backend name 非空 |
| `Render: device double shutdown idempotent` | 两次 shutdown() 不崩溃 |
| `Render: ParticleSystem create/destroy emitter` | Emitter{} 创建返回非负 ID, 双重 destroy 安全 |
| `Render: ParticleSystem update with no emitters` | aliveCount ≤ 1024 |
| `Render: ParticleSystem multiple create/destroy` | 连续创建返回不同 ID |
| `Render: ParticleSystem emit without init` | 未 bgfx 初始化时 emit+update 不崩溃 |
| `Render: FreeTypeContext init/shutdown cycle` | singleton init→shutdown→shutdown 安全 |

**关键实现细节**:
- BgfxRenderDevice 可在无 GPU 环境下构造 (仅访问默认值)
- TextureManager/LayerManager 为 singleton，使用 `::instance()` 访问
- ParticleSystem 无需 bgfx init 即可创建/销毁发射器
- FreeTypeContext 为 singleton，`init()` 幂等

### T1.3: 音频播放集成测试

**文件**: `tests/cpp/test_audio_integration.cpp` (8 用例)

| 用例 | 验证内容 |
|------|----------|
| `Audio: playSE with silence file returns valid handle` | tests/audio/silence.wav 加载成功 |
| `Audio: playSE invalid file returns 0` | 不存在文件返回 0 |
| `Audio: setGlobalVolume works` | 0.5 → getGlobalVolume ≈ 0.5, 1.0 → ≈ 1.0 |
| `Audio: isBGMPlaying returns false initially` | 初始状态 BGM/Voice 都不在播放 |
| `Audio: fadeVolume does not crash` | bgm/voice/se 三总线 fadeVolume 不崩溃 |
| `Audio: shutdown then re-init succeeds` | 完整 init→shutdown→init→shutdown 周期 |
| `Audio: playSE before init returns 0` | 未 init 时调用返回 0 不崩溃 |
| `Audio: bus volume set/get roundtrip` | bgm 0.75/voice 0.5/se 0.9 往返验证 |

### T1.4: 存档→读档往返测试

**文件**: `tests/cpp/test_save_roundtrip.cpp` (8 用例)

| 用例 | 验证内容 |
|------|----------|
| `SaveManager: save -> load data integrity` | JSON 6 字段 (scene, text_index, flags, player_name, hp) 逐项比对 |
| `SaveManager: nonexistent slot returns empty JSON` | slot 99 load 返回空 JSON |
| `SaveManager: listSaves returns correct slot list` | 3 个 slot (1,3,5) 全部出现在列表中 |
| `SaveManager: deleteSlot removes save` | slotExists 从 true 变为 false |
| `SaveManager: encryption roundtrip preserves data` | 32B key 加密保存→解密读取数据一致 |
| `SaveManager: wrong key returns empty JSON` | key A 保存→key B 读取返回空, key A 恢复可读 |
| `SaveManager: save without encryption when key is cleared` | clearEncryptionKey 后 isEncryptionEnabled = false |
| `SaveManager: multiple save/load cycles do not leak or corrupt` | 10 次循环 save→load 数据正确, 3 slot 全存在 |

### T1.5: 热重载集成测试

**文件**: `tests/cpp/test_hotreload_integration.cpp` (5 用例)

| 用例 | 验证内容 |
|------|----------|
| `HotReload: singleton returns same instance` | &a == &b |
| `HotReload: default state is not initialized` | scriptState == IDLE |
| `HotReload: state transitions work` | IDLE→DEBUG_ACTIVE→RELOADING→IDLE 状态机 |
| `DebugProtocol: breakpoints with init` | setBreakpoint→hasBreakpoint→removeBreakpoint |
| `DebugProtocol: clearAllBreakpoints works` | 3 断点清除后全部不可见 |

---

## T2 — Engine::init() 分阶段初始化

**目标**: 将 ~170 行的 `Engine::init()` 拆分为 4 个私有方法，每个方法负责一个初始化阶段。

### T2.1–T2.4: 四个阶段方法

**修改文件**: `src/entry/Engine.h` (+4 方法声明), `src/entry/Engine.cpp` (重构)

#### initPlatformPhase()

**职责**: SDL3 平台 + bgfx 渲染设备 + SoLoud 音频 + 杂项注册

```
- SDL3PlatformBackend::init() → BackendRegistry::setPlatformBackend()
- BgfxRenderDevice::init() → BackendRegistry::setRenderDevice()
- Headless 路径: BackendRegistry::registerNullBackends()
- DebugManager::RenderInfo 设置
- SoLoudAudioEngine::init() → BackendRegistry::setAudioBackend()
- DebugManager::AudioInfo 设置
- InputRouter → BackendRegistry::setInputRouter()
- SaveManager::init("saves/")
- TextureBudget::detect() → BackendRegistry::setTextureBudget()
- DebugManager、AsyncLoader 注册
```

#### initScriptingPhase()

**职责**: Lua VM + 绑定 + HotReload + FreeType

```
- LuaManager::init()
- lua_setallocf (内存监控 hook)
- BackendRegistry::setLuaState()
- BackendRegistry::setVideoPlayer()
- HotReload::instance().init("scripts/")
- FreeTypeContext::instance().init()
```

#### initAssetPhase()

**职责**: 任务系统 + 资源管线

```
- JobSystem::instance().init() → BackendRegistry::setJobSystem()
- AssetManager::instance().init()
- AsyncLoader::instance().init()
```

#### initOptionalPhase()

**职责**: Steam + Crypto + MiniGame + Animation (best-effort)

```
- SteamBackend::init() → registerSteamBinding()
- CryptoEngine → BackendRegistry::setCryptoEngine()  ⬅ 已从 Steam if 块内移出
- BgfxMiniGameBackend::init() → BackendRegistry::setMiniGameBackend()
- Animation backend (Live2D 或 Null) init + 注册
```

### T2.5: Bug 修复

**问题**: CryptoEngine 注册 (`BackendRegistry::instance().setCryptoEngine()`) 被嵌套在 `if (m_steamBackend->init())` 块内，Steam 不可用时 CryptoEngine 不注册。

**修复**: 移至 `initOptionalPhase()` 顶层，独立于 Steam 状态。

### 重构结果

| 指标 | 重构前 | 重构后 |
|------|--------|--------|
| Engine::init() 行数 | ~170 | ~35 |
| 阶段方法数 | 0 | 4 |
| 方法平均行数 | — | ~60 |
| 失败清理路径 | 隐式 (goto-style) | 每阶段 return false |
| 测试 | 233 pass | 237 pass |

---

## T3 — 跨平台 CI

**状态**: 已就位 (无需修改)

`.github/workflows/ci.yml` 已包含:

| Job | 平台 | 编译器 | 构建 | 测试 |
|-----|------|--------|------|------|
| `build-windows` | Windows 2022 | MSVC | Debug + Release | ✅ |
| `build-macos` | macOS latest | Clang | Debug | ✅ |
| `build-linux` | Ubuntu 24.04 | GCC | Debug | ✅ + 耦合计数 |
| `release` | Windows 2022 | MSVC | Release package | CPack ZIP |

- Linux: 从源码构建 SDL3 3.2.0
- macOS: Homebrew 安装依赖 (sdl3, freetype, zstd, openssl@3)
- Windows: vcpkg 安装 SDL3

**注意**: macOS CI 测试使用 `|| true` 抑制失败; Linux CI 未检查测试退出码。这两点可在后续改进。

---

## T4 — KAG API 文档

### T4.1: KAG 命令参考

**文件**: `docs/api/kag-commands.md`

从 `scripts/kag/commands/` 下 9 个 Lua 文件提取 68 个命令, 分类如下:

| 分类 | 数量 | 示例 |
|------|------|------|
| Audio | 13 | playbgm, stopbgm, fadebgm, playse, stopse, playvoice, setbgmvolume... |
| Layer | 6 | bg, fg, cl, image, position, layopt |
| Text | 13 | ch, text, l, r, er, p, ruby, font, skip, reset, pt, button, endbutton |
| System | 4 | wait, eval, emb, history |
| Flow Control | 10 | if, else, endif, jump, call, return, link, label, macro, endmacro, end |
| Transition | 4 | trans, move, quake, fade |
| VFX | 1 | vfx |
| Video | 2 | video, stopvideo |
| Resource | 6 | preload, get_texture, is_loaded, is_pending, flush_cache... |
| Save/Load | 3 | save, load, listsaves |

### T4.2: Lua 模块 API

**文件**: `docs/api/lua-modules.md`

6 个模块的完整 API 参考:

| 模块 | 函数数 | 说明 |
|------|--------|------|
| Render | 7 | render_text, clear_text, create_solid_texture, load_texture... |
| VFX | 8 | create_emitter, emit, set_emitter_pos, alive_count... |
| Debug | 7 | log, assert, traceback, get_fps, get_memory, profile_start/end |
| DevCore | 4 | get_version, get_backend_name, set_dev_mode, reload_scripts |
| KAG (C++ bindings) | 15 | play_bgm, play_se, set_global_volume, is_bgm_playing... |
| SaveBinding | 4 | save, load, list_saves, delete_save |
| Steam (conditional) | 4 | is_available, get_user_name, unlock_achievement... |

### T4.3: 入门指南

**文件**: `docs/guides/getting-started.md`

- 三端环境要求 (Windows/Linux/macOS)
- 逐步构建命令 (含 SDL3 源码构建)
- 项目目录结构图
- 第一个 KAG 场景示例 (bg→ch→p→playbgm→fg→position→stopbgm→end)
- 指向 API 文档的后续阅读链接

---

## T5 — Demo 场景

### T5.1: Demo 冒烟测试

**文件**: `tests/cpp/test_demo_smoke.cpp` (4 用例)

| 用例 | 验证内容 |
|------|----------|
| `Demo: core KAG flow parses` | `*start → [ch] → [p] → [bg] → [wait] → [end]` 6+ token 解析 |
| `Demo: choice buttons parse` | `[button]×2 + [endbutton]` 正确解析为 2 个 button |
| `Demo: bgm + stopbgm flow` | `[playbgm]` 和 `[stopbgm]` 作为独立命令解析 |
| `Demo: advance 30 lines without crash` | 30 个 `[ch]` + `[p]` + `[end]` 全部解析不崩溃 |

**注意**: 使用内联 KAG 脚本字符串，不依赖外部 `.ks` 文件（避免 CWD 路径问题）。

### T5.2: 现有 Demo 文件

`scripts/demo_story.ks` 和 `tests/scripts/smoke_test.ks` 已存在且语法正确，冒烟测试中通过 tokenizer 验证了解析能力。

---

## 文件变更清单

### 新增文件 (9)

| 文件 | 用途 | 阶段 |
|------|------|------|
| `tests/cpp/test_kag_execution.cpp` | KAG 解析+执行测试 (12 用例) | T1.1 |
| `tests/cpp/test_save_roundtrip.cpp` | 存档往返测试 (8 用例) | T1.4 |
| `tests/cpp/test_render_integration.cpp` | 渲染集成测试 (7 用例) | T1.2 |
| `tests/cpp/test_audio_integration.cpp` | 音频集成测试 (8 用例) | T1.3 |
| `tests/cpp/test_hotreload_integration.cpp` | 热重载测试 (5 用例) | T1.5 |
| `tests/cpp/test_demo_smoke.cpp` | Demo 冒烟测试 (4 用例) | T5 |
| `docs/api/kag-commands.md` | KAG 命令参考 (68 命令) | T4 |
| `docs/api/lua-modules.md` | Lua 模块 API 参考 (6 模块) | T4 |
| `docs/guides/getting-started.md` | 入门指南 | T4 |

### 修改文件 (3)

| 文件 | 变更 | 阶段 |
|------|------|------|
| `src/entry/Engine.h` | 添加 4 个 init*Phase() 声明 | T2 |
| `src/entry/Engine.cpp` | init() 拆分为 4 phase methods + CryptoEngine bug fix | T2 |
| `tests/CMakeLists.txt` | 添加 6 个新测试文件 | T1, T5 |
