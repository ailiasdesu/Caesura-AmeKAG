---
title: Scripting 模块接口隔离专项计划
type: refactor
status: active
date: 2026-06-11
origin: docs/brainstorms/2026-06-11-interface-isolation-requirements.md
---

## Summary

为 BackendRegistry 补齐 ParticleSystem/DebugManager/AsyncLoader 的 getter，替换 Scripting 7 个 binding 中 6 个具体的跨模块 include——TextureManager→BackendRegistry（已存在）、VideoPlayer→BackendRegistry（已存在，可移除 include）、DebugManager→新 getter、ParticleSystem→新 getter、AsyncLoader→新 getter、Engine.h→移除。InputRouter 改为前置声明。

## Problem Frame

Scripting 模块当前跨 7 个模块 include：Core、Render、MiniGame、Resource、System、Steam、Debug。其中 3 个是抽象接口（IAudioBackend、IRenderDevice、IMiniGameBackend）可保留，其余 4 个是具体实现（TextureManager、VideoPlayer、ParticleSystem、DebugManager、AsyncLoader）。BackendRegistry 已有前两类的 getter，需要为后三类补全。

## Requirements

Traceable to origin R11.

### BackendRegistry Extension

- R1. BackendRegistry 新增 `setParticleSystem(ParticleSystem*)` / `getParticleSystem()`，存储非拥有 raw 指针。
- R2. BackendRegistry 新增 `setDebugManager(DebugManager*)` / `getDebugManager()`。
- R3. BackendRegistry 新增 `setAsyncLoader(AsyncLoader*)` / `getAsyncLoader()`。

### Engine Wiring

- R4. Engine::init() 中注册 ParticleSystem 指针到 BackendRegistry（ParticleSystem 是静态单例，由 VFXBinding 创建）。
- R5. Engine::init() 中注册 DebugManager 指针到 BackendRegistry（DebugManager 是全局单例）。
- R6. Engine::init() 中注册 AsyncLoader 指针到 BackendRegistry（AsyncLoader 是全局单例）。

### Binding Cleanup

- R7. RenderBinding.cpp：`TextureManager::instance()` → `BackendRegistry::instance().getTextureManager()`，移除 `VideoPlayer.h`（已通过 BackendRegistry 获得）。
- R8. VFXBinding.cpp：`s_particleSystem` → `BackendRegistry::instance().getParticleSystem()`，移除 `ParticleSystem.h`。
- R9. DebugBinding.cpp：`DebugManager::instance()` → `BackendRegistry::instance().getDebugManager()`，移除 `DebugManager.h`。
- R10. RenderBinding.cpp / UnifiedBinding.cpp：`AsyncLoader::instance()` → `BackendRegistry::instance().getAsyncLoader()`，移除 `AsyncLoader.h`。
- R11. LuaManager.cpp：移除 `Engine.h` include（仅用于 BackendRegistry::registerEngineBindings，已由 BackendRegistry.h 提供）。
- R12. KAGBinding.cpp / DevCoreBinding.cpp：`#include "../Core/InputRouter.h"` → 前置声明 `class InputRouter;`。

### Verification

- R13. 编译通过，138 测试全绿。
- R14. Scripting 模块跨模块 include 数从 7 降到 ≤3（仅抽象接口：IAudioBackend、IRenderDevice、IMiniGameBackend）。

## Key Technical Decisions

- **ParticleSystem 不改为 unique_ptr 持有。** 当前 VFXBinding 持有一个 `static ParticleSystem` 实例，生命周期由 VFXBinding 管理。BackendRegistry 仅存储非拥有 raw 指针，不改变所有权。

- **DebugManager 保持单例模式。** `DebugManager::instance()` 已在 Engine::init() 初始化。BackendRegistry 存储非拥有指针，binding 通过 BackendRegistry 访问。这比直接调用 `DebugManager::instance()` 多一层间接调用，但代价可忽略（Debug API 调用频率低）。

- **AsyncLoader 保持单例模式。** 同 DebugManager。

- **InputRouter 改为前置声明。** KAGBinding 和 DevCoreBinding 已通过 `BackendRegistry::getInputRouterFromLua()` 获取指针。只需要 `InputRouter*` 类型——前置声明即可。

- **不在此范围：** VFXBinding 的 `static ParticleSystem s_particleSystem` 声明仍保留在 VFXBinding.cpp——这属于 VFX 子系统内部状态，不构成跨模块依赖。

## Implementation Units

### U1. BackendRegistry extension — add ParticleSystem/DebugManager/AsyncLoader

- **Goal:** BackendRegistry.h/.cpp 新增三个 setter/getter 对。
- **Files:** `src/Core/BackendRegistry.h`, `src/Core/BackendRegistry.cpp`
- **Pattern:** 照搬已有的 `setTextureManager()`/`getTextureManager()` 模式——添加 `ParticleSystem*`、`DebugManager*`、`AsyncLoader*` 成员 + setter + getter。
- **Test Scenarios:** 编译通过。新 getter 在注册前返回 nullptr。

### U2. Engine wiring in init()

- **Goal:** Engine::init() 中将 ParticleSystem/DebugManager/AsyncLoader 的单例注册到 BackendRegistry。
- **Files:** `src/Core/Engine.cpp`
- **Pattern:** 在现有 BackendRegistry 注册区添加三行。ParticleSystem 通过 `VFXBinding::particleSystem()` 获取（需暴露静态方法）。
- **Test Scenarios:** 引擎启动后 BackendRegistry::getParticleSystem() 等非空。

### U3. RenderBinding cleanup

- **Goal:** 替换 TextureManager 单例调用，移除 VideoPlayer.h 和 AsyncLoader.h。
- **Files:** `src/Scripting/RenderBinding.cpp`
- **Pattern:** `TextureManager::instance().xxx()` → `BackendRegistry::instance().getTextureManager()->xxx()`，`AsyncLoader::instance()` → `BackendRegistry::instance().getAsyncLoader()`。
- **Test Scenarios:** Demo 纹理加载正常显示。VideoPlayer API 可用。

### U4. VFXBinding cleanup

- **Goal:** 替换 ParticleSystem 单例，移除 ParticleSystem.h include。
- **Files:** `src/Scripting/VFXBinding.cpp`, `src/Scripting/VFXBinding.h`
- **Pattern:** 暴露静态方法 `static ParticleSystem& particleSystem()` 供 Engine 注册。binding 内改用 `BackendRegistry::instance().getParticleSystem()`。
- **Test Scenarios:** VFX API（粒子效果）正常。ParticleSystem.h 不再出现在 VFXBinding.cpp 的 include 中。

### U5. DebugBinding cleanup

- **Goal:** 替换 DebugManager 单例调用，移除 DebugManager.h include。
- **Files:** `src/Scripting/DebugBinding.cpp`
- **Pattern:** `DebugManager::instance()` → `BackendRegistry::instance().getDebugManager()`。
- **Test Scenarios:** Debug API（日志级别、性能计数器）正常。

### U6. LuaManager + remaining includes cleanup

- **Goal:** 移除 LuaManager.cpp 的 Engine.h include，KAGBinding/DevCoreBinding 的 InputRouter.h 改为前置声明。
- **Files:** `src/Scripting/LuaManager.cpp`, `src/Scripting/KAGBinding.cpp`, `src/Scripting/DevCoreBinding.cpp`
- **Pattern:** Engine.h → 移除（BackendRegistry.h 已提供 registerEngineBindings）。InputRouter.h → `class InputRouter;` 前置声明。
- **Test Scenarios:** Lua 引擎正常启动运行 Demo。InputRouter API 功能不变。

### U7. Verification — coupling count check

- **Goal:** 运行耦合度计数脚本，确认 Scripting 跨模块 include 数 ≤3。
- **Files:** 已有计数脚本（U8 产物）。
- **Verification:** Scripting 跨模块 include 从 7 降到 ≤3。

## Scope Boundaries

- 不改变 VFXBinding 的 ParticleSystem 所有权（保持 static 单例）。
- 不改变 DebugManager 和 AsyncLoader 的单例模式。
- 不涉及 BackendRegistry 已有 getter 的重构（TextureManager、VideoPlayer、InputRouter 的 getter 已存在）。

## Risks & Dependencies

- **风险：ParticleSystem 暴露。** VFXBinding 当前持有 static 实例——暴露静态方法用于注册，Engine 需要 `#include "VFXBinding.h"`。替代方案：Engine 不需要知道 ParticleSystem 细节，由 VFXBinding 模块自己注册。
- **依赖：BackendRegistry 头文件**会被更多文件 include，编译时间可能微增。但 Scripting 模块原本就 include 这些具体头文件，替换后总体 include 量减少。
