---
title: "MiniGame 3D 管线: 从 Debug Wireframe 到完整 PBR 小游戏后端"
date: 2026-06-09
category: docs/solutions/architecture-patterns
module: MiniGame
problem_type: architecture_pattern
component: development_workflow
severity: medium
applies_when:
  - "为视觉小说引擎扩展 3D 小游戏模块时"
  - "bgfx 嵌入式着色器 + 多阶段渐进实现"
  - "Lua API 层通过纯虚接口访问 C++ 后端时"
tags: [minigame, bgfx, pbr, collision, physics, lua-api, embedded-shader]
---

# MiniGame 3D 管线: 从 Debug Wireframe 到完整 PBR 小游戏后端

## Context

Caesura (AmeKAG) 需要 3D 小游戏能力以提升 galgame 交互体验。初始状态只有 debug wireframe cube + 基础 orbit camera。需要在 7 个实现单元内渐进构建完整的 3D 渲染管线，同时保持所有 Lua→C++ 调用通过 `IMiniGameBackend` 纯虚接口。

## Guidance

7 阶段渐进架构，每阶段独立可构建、可验证：

### IU-1: 嵌入式 Shader
```
shaders/dx11/minigame_vs.hlsl  → fxc.exe → .dxbc → EmbeddedShaders_MiniGame.cpp
```
使用 `bgfx::makeRef()` 从编译时嵌入的字节码创建 shader program。DXBC 优于运行时编译——零初始化延迟，无着色器缓存文件依赖。

### IU-2: PBR-lite 材质
`MiniMaterial` 结构体映射到着色器 uniform `u_material` (roughness/metallic/specStr)。`submitObject()` 优先查材质表，回退到 `MiniObject` 内联颜色。Lua 工厂模式 `create_material → set_material`。

### IU-3: 程序化几何体
`MiniGeometry` 静态工厂: `createCubeGeometry()`, `createSphereGeometry(16)`, `createPlaneGeometry()`, `createQuadGeometry()`。后端在 `initGeometryCache()` 中一次性创建全部 VB/IB，`submitObject()` 按 `MiniGeoType` 选择。

### IU-4: 多光源
着色器支持 1 方向光 + 最多 3 点光源（距离衰减 + smoothstep）。`setLightUniforms()` 每帧将 `MiniDirectionalLight` + `std::vector<MiniPointLight>` 序列化到 bgfx uniform。

### IU-5: AABB 碰撞
`MiniCollision::findCollisions()` — O(n²) 暴力检测（100 物体以内足够）。每 `update()` 自动运行。碰撞时调用 Lua 全局 `on_collision(objA, objB)`。

### IU-6: Euler 物理
`update()` 中逐物体积分: `accel→velocity→position`。全局重力 -9.8 m/s²。`set_velocity` / `set_gravity` 逐物体控制。

### IU-7: Demo + 文档
`demo_minigame.lua` 展示所有 15 个 Lua API 的完整用法。

## Why This Matters

- **独立 bgfx view #10**: 3D 渲染与 2D 层完全隔离，无深度冲突
- **纯虚接口**: `IMiniGameBackend` → `BgfxMiniGameBackend` / `NullMiniGameBackend`，遵循 `BackendRegistry` 工厂模式
- **编译时嵌入着色器**: 零外部文件依赖，发布包只需 .exe + .dll
- **渐进可验证**: 每个 IU 独立构建，可 bisect 问题

## When to Apply

- 扩展引擎添加新的渲染子系统时，遵循 `IBackend → ConcreteBackend → NullBackend` 三件套
- 着色器优先编译为 DXBC/SPIR-V/GLSL 并嵌入到 `EmbeddedShaders*.cpp`
- Lua API 通过 `luaCall(L, "method_name")` 字符串分发，或直接绑定 `luaL_Reg` 函数表

## Related

- `PLAN_MINIGAME_3D.md` — 完整实现计划
- `src/MiniGame/IMiniGameBackend.h` — 纯虚接口定义
- `scripts/demo_minigame.lua` — 演示脚本
- `ENGINE_ANALYSIS.md` §3.9 — 模块描述
