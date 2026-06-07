---
date: 2026-06-07
topic: engine-stabilization-ai-ready
---

# 引擎稳定化：AI 辅助开发就绪

## Summary

清理由 ENGINE_ANALYSIS.md 标记的 4 条硬阻断技术债，让引擎暴露清晰一致的 API 面供外部 AI（Codex/Copilot 等 IDE 助手）理解和生成正确的 KAG 脚本。

## Problem Frame

当前引擎有 18 条已标记技术债。其中 4 条直接阻碍 AI 辅助开发：

- AI 面对两套功能重复的 API（UnifiedBinding 和 KAGBinding）无法判断该调用哪个
- AI 生成的粒子代码因 ParticleSystem 绑死具体类而非接口而不可移植
- 三层所有权模型（Engine + BackendRegistry raw ptr + BackendRegistry unique_ptr）让 AI 生成的脚本运行在不确定的引擎状态下
- 沙盒安全规则内联在 C++ 字符串中，AI 无法读取真实约束，生成的代码可能被沙盒拒绝却无法预判

其他 14 条债务（字体合并、跨平台加密等）不直接阻断 AI 辅助开发，后置处理。

## Requirements

### API 面统一

- R1. KAG 脚本层的粒子操作必须通过 `IRenderDevice*` 抽象接口，而非 `BgfxRenderDevice*` 具体类
- R2. ParticleSystem 的构造和初始化不再接受具体渲染设备类型，改用 BackendRegistry 解析当前活跃的 `IRenderDevice*`
- R3. UnifiedBinding 和 KAGBinding 中重复的便捷方法（show_text、show_image、clear_screen、wait_click）合并到 UnifiedBinding 作为唯一调用路径
- R4. KAGBinding 保留纯 KAG 语义方法（场景跳转、分支、标记、存档），UnifiedBinding 保留后端分发方法（render、audio、platform）和便捷方法
- R5. 合并后，Lua 侧单一 KAG 命令在 C++ 侧只有一条绑定路径——AI 和人类开发者不会遇到"同一个操作用两个函数都能做"的歧义

### 所有权与稳定性

- R6. BackendRegistry 只通过 `unique_ptr` 持有后端实例，移除原始指针成员（m_renderDevice、m_audioBackend、m_platformBackend）
- R7. 所有后端访问通过 `BackendRegistry::getRenderDevice()` 返回引用，调用方不持有指针
- R8. Engine 类不再通过 `BackendRegistry` 间接持有后端——Engine 直接拥有 unique_ptr，BackendRegistry 降级为纯查询/工厂单例（无所有权）
- R9. 后端销毁顺序由 Engine::shutdown() 显式控制，BackendRegistry 不参与生命周期管理

### 沙盒安全可读

- R10. 沙盒规则从 `LuaManager::lockdownScriptEnv()` 中的 `luaL_dostring` 内联代码迁移到 `scripts/sandbox.lua` 独立文件
- R11. `sandbox.lua` 作为纯 Lua 模块，AI 可以完整读取其内容理解安全边界
- R12. 沙盒规则支持注释和分组标记，标明每条禁用/替换的操作和原因
- R13. C++ 侧的 `lockdownScriptEnv()` 改为单次 `luaL_dofile("scripts/sandbox.lua")` 调用——规则变更只需编辑 .lua 文件，无需重编译引擎

## Scope Boundaries

**本次涵盖:** TD-03（所有权）、TD-05（沙盒外抽）、TD-06（粒子接口）、TD-15（API 面合并）

**本次不涵盖:** TD-01（render 双 Lua pcall 合并）、TD-02（字体渲染器合并）、TD-04（DebugManager 锁移除）、TD-07–TD-14（跨平台加密/JSON解析器/热重载轮询/移动端存根/VideoDecoder 接口）、TD-16–TD-18（Debug 宏/命名一致性/CRL 接口）

这些债务在后续迭代中单独处理，不与 AI 辅助开发形成硬依赖。
