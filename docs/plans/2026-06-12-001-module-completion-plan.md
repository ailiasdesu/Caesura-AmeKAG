# 模块完善计划 —— Caesura (AmeKAG)

**创建日期:** 2026-06-12
**状态:** active

## 目标

对 16 个引擎模块逐一评估当前状态，为每个模块制定具体的完善任务。完成后每个模块至少满足：接口定义在 `api/`、有单元测试、无 NullStub 残留（除非有意为之）。

## 模块分类总览

| 分类 | 模块数 | 模块 |
|---|---|---|
| 🔴 桩实现（需写真实代码） | 3 | live2d, minigame, steam |
| 🟡 部分完善（缺测试/接口/文档） | 10 | archive, di, entry, job, platform, render, rpc, script, storage, audio |
| 🟢 基本完善 | 3 | debug, input, resource |

---

## 🔴 第一优先：桩实现模块

### M1 — Steam（Steamworks 集成）
**当前状态:** `NullSteamBackend` 空壳，`ISteamBackend` 在 `steam/` 根目录，无 `api/`。
**目标:**
- [ ] 移动 `ISteamBackend.h` → `steam/api/ISteamBackend.h`
- [ ] 实现 `SteamBackend`（Steamworks SDK 集成：成就、统计、云存档）
- [ ] `steam/api/` 加入 CMakeLists
- [ ] 添加 `tests/cpp/test_steam.cpp`
**风险:** Steamworks SDK 仅 Windows/Linux，需条件编译。

### M2 — Live2D（动态立绘动画）
**当前状态:** `NullAnimationBackend` 空壳，`IAnimationBackend` 在 `live2d/` 根目录。有 `Live2DBackend` 但编译条件受 Cubism SDK 限制。
**目标:**
- [ ] 移动 `IAnimationBackend.h` → `live2d/api/IAnimationBackend.h`
- [ ] 确认 `Live2DBackend` 在 Cubism SDK 可用时能正常编译和运行
- [ ] 添加 `tests/cpp/test_live2d.cpp`（至少 Null 后端测试）
**风险:** Cubism SDK 是专有 SDK，需要 license。

### M3 — MiniGame（3D 小游戏）
**当前状态:** `NullMiniGameBackend` + 部分 `BgfxMiniGameBackend`。有 `IMiniGameBackend` 在 `minigame/api/`。有几何/碰撞代码但未完整集成。
**目标:**
- [ ] 完成 `BgfxMiniGameBackend` 的场景加载/渲染循环
- [ ] 添加 `tests/cpp/test_minigame.cpp`
**风险:** bgfx 依赖，渲染测试需要 GPU 上下文。

---

## 🟡 第二优先：缺接口/测试模块

### M4 — archive（CARC 归档格式）
**当前状态:** 6 cpp + 7 h，无 `api/`，无测试。实现了加密/压缩/读写但接口散落在具体类中。
**目标:**
- [ ] 提取 `IArchiveReader` / `IArchiveWriter` 接口到 `archive/api/`
- [ ] 添加 `tests/cpp/test_archive.cpp`
- [ ] `CryptoEngine` 提取 `ICryptoEngine` 接口

### M5 — storage（存档/读档）
**当前状态:** 4 cpp + 4 h，`ISaveProvider` 在模块根目录，无 `api/`。`SaveManager` 直接依赖 `CryptoEngine`（见架构评估）。
**目标:**
- [ ] 移动 `ISaveProvider.h` → `storage/api/ISaveProvider.h`
- [ ] `SaveManager` 通过接口访问加密，解除对 `archive/CryptoEngine.h` 的直接依赖
- [ ] 添加 `tests/cpp/test_storage.cpp`
- [ ] 添加 `ISaveManager` 接口

### M6 — script（Lua 脚本系统）
**当前状态:** 9 cpp + 9 h，核心模块但无 `api/`。`LuaManager` 被 Engine 直接依赖。
**目标:**
- [ ] 提取 `ILuaManager` 接口到 `script/api/ILuaManager.h`
- [ ] `LuaManager` 实现 `ILuaManager`
- [ ] 添加 `tests/cpp/test_kag_commands.cpp`（KAG 命令单元测试）
- [ ] 添加 `tests/cpp/test_script_bindings.cpp`（绑定集成测试）

### M7 — render（渲染系统）
**当前状态:** 20 cpp + 23 h，最大模块。有 5 个接口（`IRenderDevice`, `IGpuMonitor`, `IVideoPlayer`, `ITextureManager`, `IParticleSystem`, `ILayerManager`）。仅 1 个测试文件涵盖所有子模块。
**目标:**
- [ ] 拆分 `test_render.cpp` → `test_render_device.cpp`, `test_texture_manager.cpp`, `test_particle_system.cpp`, `test_layer_manager.cpp`
- [ ] `BgfxDraw.cpp`（513 行）进一步拆分
- [ ] 移除接口中的 bgfx 具体类型（`bgfx::TextureHandle` → 不透明句柄）

### M8 — entry（引擎入口）
**当前状态:** 2 cpp + 3 h，`Engine.cpp` 582 行，组合根耦合 14 模块。
**目标:**
- [ ] 拆分 `Engine::init()` 按子系统分组（渲染初始化、音频初始化、脚本初始化等）
- [ ] 提取初始化管线到独立函数/类
- [ ] 添加 `tests/cpp/test_engine_init.cpp`（分阶段初始化测试）

### M9 — platform（平台抽象）
**当前状态:** 2 cpp + 3 h，有 `IPlatformBackend` 在 `api/`。无测试。
**目标:**
- [ ] 添加 `tests/cpp/test_platform.cpp`
- [ ] `MobileAdapter` 完善或标记为实验性

### M10 — rpc（编辑器 RPC 服务器）
**当前状态:** 2 cpp + 2 h，无 `api/`，无测试。`EditorServer` 和 `RpcServer` 单例。
**目标:**
- [ ] 提取 `IRpcServer` 接口到 `rpc/api/IRpcServer.h`
- [ ] 添加 `tests/cpp/test_rpc.cpp`
- [ ] `EditorServer` 从 Engine 解耦（已在上一轮完成 include 修复）

### M11 — job（任务系统）
**当前状态:** 1 cpp + 1 h，无 `api/`，有测试。`JobSystem` 单例。
**目标:**
- [ ] 提取 `IJobSystem` 接口到 `job/api/IJobSystem.h`
- [ ] 注册到 `BackendRegistry`

### M12 — di（依赖注入）
**当前状态:** 3 cpp + 5 h，`BackendRegistry` 全接口化。有 `ITextureBudget` 在 `api/`。无测试。
**目标:**
- [ ] 添加 `tests/cpp/test_backend_registry.cpp`
- [ ] `SandboxQuota` 迁移到 `api/ISandboxQuota.h`

### M13 — audio（音频系统）
**当前状态:** 2 cpp + 3 h，有 `IAudioBackend` 在 `api/`。有测试。
**目标:**
- [ ] 补充边界测试（音频句柄泄漏、总线竞态）

---

## 执行顺序

```
Phase 1: M1(Steam) → M2(Live2D) → M3(MiniGame)     [消除桩实现]
Phase 2: M4(archive) → M5(storage) → M6(script)      [接口提取]
Phase 3: M7(render) → M8(entry) → M9(platform)       [测试+拆分]
Phase 4: M10(rpc) → M11(job) → M12(di) → M13(audio)  [收尾]
```

每个模块完成后要求：
- 构建零错误
- 149 基础测试 + 该模块新增测试全部通过
- 提交（一步一提交，方便回滚）