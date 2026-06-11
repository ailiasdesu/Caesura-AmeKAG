---
title: Scripting 模块接口隔离专项计划
type: refactor
status: active
date: 2026-06-11
origin: docs/brainstorms/2026-06-11-interface-isolation-requirements.md
---

## Summary

为 BackendRegistry 补齐三个 getter（ParticleSystem 由 VFXBinding 自注册，DebugManager 和 AsyncLoader 由 Engine 注册），替换 7 个 binding 中 6 个具体跨模块 include。目标：Scripting 模块不再引用任何具体实现头文件。

## Problem Frame

Scripting 模块当前跨多个模块 include 具体实现头文件。其中 TextureManager/VideoPlayer 已有 BackendRegistry getter 但未统一使用，ParticleSystem/DebugManager/AsyncLoader 缺失 getter。补齐后统一通过 BackendRegistry 访问，消除 Scripting 对具体模块的直接依赖。

## Requirements

Traceable to origin R11.

### BackendRegistry Extension

- R1. BackendRegistry 新增 `setDebugManager(DebugManager*)` / `getDebugManager()`。
- R2. BackendRegistry 新增 `setAsyncLoader(AsyncLoader*)` / `getAsyncLoader()`。
- R3. BackendRegistry 新增 `setParticleSystem(ParticleSystem*)` / `getParticleSystem()`。

### Engine Wiring (安全：零新 include)

- R4. Engine::init() 注册 DebugManager 到 BackendRegistry（Engine 已 include Debug/DebugManager.h）。
- R5. Engine::init() 注册 AsyncLoader 到 BackendRegistry（Engine 已 include Resource/AsyncLoader.h）。
- R6. **VFXBinding 自注册。** registerVFXBinding() 内部调用 BackendRegistry::instance().setParticleSystem()。Engine 无需知道 ParticleSystem，避免 Core→Scripting 新依赖。

### Binding Cleanup

- R7. RenderBinding.cpp：TextureManager::instance() → BackendRegistry getter。移除 VideoPlayer.h、AsyncLoader.h。
- R8. VFXBinding.cpp：移除 ParticleSystem.h，改用 BackendRegistry getter。
- R9. DebugBinding.cpp：DebugManager::instance() → BackendRegistry getter。移除 DebugManager.h。
- R10. LuaManager.cpp：移除 Engine.h（vestigial，BackendRegistry.h 已提供所需声明）。
- R11. KAGBinding/DevCoreBinding：InputRouter.h → 前置声明 `class InputRouter;`。

### Verification

- R12. 编译通过，138 测试全绿。
- R13. Scripting 模块所有具体实现头文件 include 已消除。

## Key Technical Decisions

- **ParticleSystem 自注册（安全关键）。** Engine.cpp 不引入 Scripting/VFXBinding.h → 避免创建反向 Core→Scripting 依赖。VFXBinding 在被 LuaManager 加载时自行注册。
- **DebugManager/AsyncLoader 保持单例。** Engine 已持有引用，注册不增加新 include。
- **不在此范围：** TextureManager.h 的 include 在 RenderBinding.cpp 中仍需保留——替换的是调用方式（单例 → BackendRegistry），不是 include（方法调用需完整类定义）。

## Implementation Units

### U1. BackendRegistry extension

- **Goal:** BackendRegistry.h/.cpp 新增 DebugManager/AsyncLoader/ParticleSystem 的 setter/getter。
- **Files:** `src/Core/BackendRegistry.h`, `src/Core/BackendRegistry.cpp`
- **Pattern:** 照搬已有 setTextureManager/getTextureManager 模式。
- **Test Scenarios:** 编译通过。

### U2. Engine + VFXBinding wiring

- **Goal:** Engine 注册 DebugManager 和 AsyncLoader。VFXBinding 自注册 ParticleSystem。
- **Files:** `src/Core/Engine.cpp`, `src/Scripting/VFXBinding.cpp`
- **Pattern:** Engine::init() 加两行。registerVFXBinding() 加一行。
- **Safety check:** 零新 include。Engine 已有 DebugManager.h 和 AsyncLoader.h。VFXBinding 已有 ParticleSystem.h 和 BackendRegistry.h。
- **Test Scenarios:** getDebugManager()/getAsyncLoader()/getParticleSystem() 非空。

### U3. RenderBinding cleanup

- **Goal:** TextureManager 改用 BackendRegistry getter。移除 VideoPlayer.h、AsyncLoader.h。
- **Files:** `src/Scripting/RenderBinding.cpp`
- **Pattern:** TextureManager::instance().xxx() → BackendRegistry::instance().getTextureManager()->xxx()。AsyncLoader::instance() 同。
- **Test Scenarios:** Demo 纹理加载、视频、异步加载功能不变。

### U4. VFXBinding cleanup

- **Goal:** 移除 ParticleSystem.h，改用 BackendRegistry getter。
- **Files:** `src/Scripting/VFXBinding.cpp`, `src/Scripting/VFXBinding.h`
- **Pattern:** s_particleSystem → BackendRegistry::instance().getParticleSystem()。
- **Test Scenarios:** VFX 粒子效果功能不变。

### U5. DebugBinding cleanup

- **Goal:** 移除 DebugManager.h，改用 BackendRegistry getter。
- **Files:** `src/Scripting/DebugBinding.cpp`
- **Pattern:** DebugManager::instance() → BackendRegistry::instance().getDebugManager()。
- **Test Scenarios:** Debug API 功能不变。

### U6. LuaManager + InputRouter cleanup

- **Goal:** 移除 LuaManager.cpp 的 Engine.h。KAGBinding/DevCoreBinding 的 InputRouter.h → 前置声明。
- **Files:** `src/Scripting/LuaManager.cpp`, `src/Scripting/KAGBinding.cpp`, `src/Scripting/DevCoreBinding.cpp`
- **Pattern:** Engine.h → 直接删除。InputRouter.h → `class InputRouter;` 声明。
- **Test Scenarios:** Lua 引擎启动，Demo 运行，输入处理正常。

### U7. Verification

- **Goal:** 编译通过，测试全绿，验证 Scripting 模块无具体实现头文件 include。
- **Verification:** `rg '#include ".*/(TextureManager|VideoPlayer|ParticleSystem|DebugManager|Engine|AsyncLoader)\.h"' src/Scripting/` 返回空。

## Scope Boundaries

- 不改变 VFXBinding 的 ParticleSystem 所有权。
- 不改变 DebugManager/AsyncLoader 的单例模式。
- TextureManager.h 的 include 保留（方法调用需完整类定义），但调用路径改为 BackendRegistry。
- 不新建 Core→Scripting 反向依赖。

## Risks & Dependencies

- **已验证安全：** Engine.cpp 已 include DebugManager.h 和 AsyncLoader.h——注册不引入新依赖。
- **已验证安全：** VFXBinding.cpp 已 include ParticleSystem.h 和 BackendRegistry.h——自注册不引入新依赖。
- **风险：** VFXBinding 自注册时机。registerVFXBinding() 在 LuaManager::init() 中被调用，此时 BackendRegistry 已就绪。若 VFX binding 函数在注册前被调用，getParticleSystem() 返回 nullptr。缓解：Lua 脚本在 init 完成后才开始执行，不会在注册前调用。
