---
title: 引擎稳定化：AI 辅助开发就绪
type: refactor
status: complete
date: 2026-06-07
origin: docs/brainstorms/2026-06-07-engine-stabilization-ai-ready-requirements.md
---

# 引擎稳定化：AI 辅助开发就绪

## Summary

修复 4 条硬阻断技术债：合并 UnifiedBinding/KAGBinding 重复 API、ParticleSystem 切抽象接口、BackendRegistry 所有权简化、沙盒规则外抽为独立 .lua 文件。让引擎暴露清晰一致的 API 面供外部 AI 生成正确的 KAG 脚本。

## Problem Frame

当前引擎有 18 条已标记技术债。在 IDE AI 辅助开发模式下，4 条直接阻断 AI 生成正确代码：重复 API 面让 AI 不知调哪个、粒子绑死具体类导致不可移植、三重所有权让 AI 生成的脚本跑在不确定状态下、沙盒规则内联 C++ 让 AI 无法预判安全边界。

## Requirements

### API 面统一

- R1. KAG 脚本层粒子操作通过 `IRenderDevice*` 抽象接口，非 `BgfxRenderDevice*`
- R2. ParticleSystem 不再接受具体渲染设备类型，通过 BackendRegistry 解析
- R3. UnifiedBinding 和 KAGBinding 重复便捷方法合并到 UnifiedBinding 作为唯一路径
- R4. KAGBinding 保留纯 KAG 语义方法；UnifiedBinding 保留后端分发 + 便捷方法
- R5. 合并后 Lua 侧单一 KAG 命令在 C++ 侧只有一条绑定路径

### 所有权与稳定性

- R6. BackendRegistry 只通过 `unique_ptr` 持有后端，移除原始指针成员
- R7. 所有后端访问返回引用，调用方不持有指针
- R8. Engine 直接拥有 unique_ptr，BackendRegistry 降级为纯查询工厂
- R9. 后端销毁顺序由 Engine::shutdown() 显式控制

### 沙盒安全可读

- R10. 沙盒规则从 `luaL_dostring` 内联代码迁移到 `scripts/sandbox.lua`
- R11. `sandbox.lua` 作为纯 Lua 模块，AI 可完整读取
- R12. 沙盒规则支持注释和分组标记
- R13. C++ 侧 `lockdownScriptEnv()` 改为单次 `luaL_dofile` 调用

## Key Technical Decisions

- KTD1. **unique_ptr 归 Engine，BackendRegistry 纯查询** — Engine::shutdown() 已控制销毁逆序链，BackendRegistry 持所有权会引入双源真值。移除 BackendRegistry 的 m_renderDevice/m_audioBackend/m_platformBackend 裸指针和 m_ownedRenderDevice/m_ownedAudioBackend/m_ownedPlatformBackend unique_ptr，getter 改为从 Engine 持有的实例返回。
- KTD2. **便捷方法归 UnifiedBinding** — show_text、show_image、clear_screen、wait_click 这些跨后端通用操作留在 UnifiedBinding（即 `_CAESURA_BACKEND` 表）。KAGBinding 只保留纯 KAG 叙事操作（场景跳转、分支、标记、选择支、存档调用）。两表不互相包含。
- KTD3. **ParticleSystem 通过 BackendRegistry 自解析后端** — 移除构造函数中 `BgfxRenderDevice*` 参数。`init()` 内调用 `BackendRegistry::instance().getRenderDevice()` 获取当前 IRenderDevice，不再存储设备指针。渲染操作通过 IRenderDevice 抽象执行。
- KTD4. **sandbox.lua 放 scripts/ 目录，与 init.lua 同级** — 路径 `scripts/sandbox.lua`，由 LuaManager::lockdownScriptEnv() 在现有 `scripts/` package.path 上下文中加载。不新增搜索路径。

## Implementation Units

### U1. BackendRegistry 所有权简化

- **目标:** 移除 BackendRegistry 中重复的所有权（裸指针 + unique_ptr 双轨），统一由 Engine 持有。
- **文件:** `src/Core/BackendRegistry.h`、`src/Core/BackendRegistry.cpp`、`src/Core/Engine.h`
- **要点:**
  - 删除 `m_renderDevice`、`m_audioBackend`、`m_platformBackend` 裸指针成员
  - 删除 `m_ownedRenderDevice`、`m_ownedAudioBackend`、`m_ownedPlatformBackend` unique_ptr 成员
  - getter 改为引用返回 `IRenderDevice&` / `IAudioBackend&` / `IPlatformBackend&`，由 Engine 侧注册
  - `setRenderDevice/setAudioBackend/setPlatformBackend` 改为只接收指针不持有
  - Engine.h 中对应访问器签名从 `→&` 更新
- **验证:** 编译通过；Engine::shutdown() 逆序链无变化；BackendRegistry 不参与后端销毁

### U2. ParticleSystem 切到抽象接口

- **目标:** ParticleSystem 不再依赖 `BgfxRenderDevice*`，改为通过 IRenderDevice 抽象操作。
- **文件:** `src/Render/ParticleSystem.h`、`src/Render/ParticleSystem.cpp`
- **要点:**
  - 移除 `class BgfxRenderDevice*` 前向声明和 `m_device` 成员
  - `init()` 参数从 `BgfxRenderDevice*` 改为无参，内部调用 BackendRegistry::getRenderDevice()
  - 所有 bgfx 调用改为通过 IRenderDevice 抽象（beginBatch/flushBatch/blitTexture）
  - 如果粒子渲染路径在 IRenderDevice 中缺少对应抽象方法，新增最小接口
- **验证:** 编译通过；粒子效果运行时行为不变

### U3. UnifiedBinding/KAGBinding API 面去重

- **目标:** 消除 show_text/show_image/clear_screen/wait_click 在两层绑定中的重复实现。
- **文件:** `src/Scripting/UnifiedBinding.h`、`src/Scripting/UnifiedBinding.cpp`、`src/Scripting/KAGBinding.h`、`src/Scripting/KAGBinding.cpp`
- **要点:**
  - 从 KAGBinding 中移除 show_text、show_image、clear_screen、wait_click 的 C 函数实现
  - 确认 UnifiedBinding 中这四个方法使用相同后端解析路径并功能完整
  - KAG 表中删除对应字段，或在 KAG 表中留 Lua 侧别名指向 _CAESURA_BACKEND 同名方法
  - 更新 `LuaManager::registerModules()` 中的日志输出（API 计数变化）
- **验证:** 编译通过；现有 KAG 脚本无需修改即可运行；Lua 侧 KAG.show_text 和 _CAESURA_BACKEND.show_text 行为一致

### U4. 沙盒安全规则外抽

- **目标:** 将 lockdownScriptEnv() 中的 luaL_dostring 内联代码迁移到 scripts/sandbox.lua。
- **文件:** `src/Scripting/LuaManager.cpp`、新建 `scripts/sandbox.lua`
- **要点:**
  - 创建 `scripts/sandbox.lua`，内容为当前 lockdownScriptEnv() 中所有 luaL_dostring 调用的 Lua 字符串替换为独立函数
  - 每条规则加注释说明目的（禁用原因、影响范围）
  - lockdownScriptEnv() 中移除 luaL_dostring 内联代码块
  - 改为 `luaL_dofile(m_L, "scripts/sandbox.lua")` 单次调用
  - 保留 C++ 侧对 os.execute/os.remove/io.open 的覆盖（需 lua_State 上下文）与 sandbox.lua 协作
- **验证:** 编译通过；sandbox.lua 语法正确；引擎启动后沙盒行为不变
