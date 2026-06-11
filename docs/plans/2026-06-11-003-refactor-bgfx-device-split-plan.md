---
title: BgfxRenderDevice careful split
type: refactor
status: active
date: 2026-06-11
---

## Summary

将 BgfxRenderDevice 的 1169 行拆为 4 个编译单元。只提取零交叉引用的三个独立模块，绘制代码保留在原文件。每步独立提交并编译验证。

## Problem Frame

上次失败的根因：Draw 提取将 batch/blit/effect 函数抽出到 BgfxDraw，但这些函数引用 m_width/m_height/m_posTexLayout/m_rttMap/m_textRenderer 等成员——导致大量交叉引用需要在两文件间传递。本次只提取三个真正的独立模块。

## Key Technical Decisions

- **Draw 代码不提取。** batch/blit/effect/stretch/affine 函数留在 BgfxRenderDevice.cpp，直接访问成员。等本次三个模块稳定、测试覆盖完善后再考虑提取。
- **每步编译验证。** 提取一个文件 → cmake --build → git commit。不跨步组合。
- **提取顺序：** DebugCallback（最简单）→ ShaderManager → DeviceCore。每次提取后原文件的 include 和成员同步更新。

## Implementation Units

### U1. BgfxDebugCallback

- **提取内容：** BgfxDebugCallback 类（~40行）+ s_debugCallback 实例 + setBgfxShuttingDown 函数。
- **新文件：** `src/render/BgfxDebugCallback.h`, `src/render/BgfxDebugCallback.cpp`
- **原文件改动：** 移除 class 定义和 static 实例，添加 `#include "BgfxDebugCallback.h"`，替换 `s_debugCallback` → `g_bgfxDebugCallback`（新名称避免匿名命名空间冲突）。
- **CMakeLists：** 添加新 .cpp 到 ENGINE_SOURCES，新 .h 到 ENGINE_HEADERS。
- **验证：** `cmake --build build_nol2d --config Debug` 编译通过。

### U2. BgfxShaderManager

- **提取内容：** buildBgfxShader (~48行) + initEmbeddedShaders (~150行) + 所有 ProgramHandle/UniformHandle 成员变量（m_fallbackProgram, m_texSampler, m_blendProgram, m_transitionProgram, m_vfxProgram, m_stretchProgram, m_affineProgram, m_u_blendParams, m_u_transParams, m_u_vfxParams, m_u_stretchParams, m_u_affineParams）+ 所有对应的 getter 方法。
- **新文件：** `src/render/BgfxShaderManager.h`, `src/render/BgfxShaderManager.cpp`
- **原文件改动：**
  - 头文件：include EmbeddedShaders.h → include BgfxShaderManager.h；移除所有 shader 成员 + getter → 替换为 `std::unique_ptr<BgfxShaderManager> m_shaders;` + 委托 getter。
  - cpp 文件：移除 buildBgfxShader + initEmbeddedShaders 函数体；init() 中 `m_shaders = std::make_unique<BgfxShaderManager>();` + `m_shaders->initEmbeddedShaders();`；shutdown() 中 `m_shaders.reset()` 替换逐个 destroy。
  - 所有 m_fallbackProgram → m_shaders->getFallbackProgram() 等成员访问改为委托。
- **注意：** BgfxShaderManager dtor 逐项 isValid+destroy，默认初始化全部 BGFX_INVALID_HANDLE。
- **验证：** 编译通过。Demo 运行所有 shader 正常加载（Blend/Transition/VFX/StretchBlt/AffineBlt）。

### U3. BgfxDeviceCore

- **提取内容：** init/resize/shutdown/beginFrame/endFrame/commit_frame + setupDefaultViews/setViewRect/setViewClear/touch/setDebugName + createRenderTarget/destroyRenderTarget/getViewportTexture + blitViewport + setPreferredBackend/getBackendName。
- **新文件：** `src/render/BgfxDeviceCore.h`, `src/render/BgfxDeviceCore.cpp`
- **Dependencies:** BgfxShaderManager（init 中调用 initEmbeddedShaders），bx/math.h（mtxOrtho）。
- **原文件改动：**
  - 头文件：添加 `#include "BgfxDeviceCore.h"`，添加 `std::unique_ptr<BgfxDeviceCore> m_deviceCore;`。移除 m_width/m_height/m_bgfxInitialized/m_rttMap/m_nextHandle/RTTEntry → 委托 getBackbufferWidth/Height 到 m_deviceCore->getWidth/Height。
  - cpp 文件：init() 中 `m_deviceCore = std::make_unique<BgfxDeviceCore>();` + 委托 init；resize/shutdown/frame/views 全部委托；RTT 函数委托。
  - 保持：m_posTexLayout, m_textRenderer, m_batchQuads, m_batching 留在 BgfxRenderDevice（画图用）。
- **验证：** 编译通过。Demo 运行正常。

### U4. 最终验证

- **验证：** 编译通过，引擎启动成功，所有子系统正常，耦合度计数 PASS。
- **清点：** 1169 行 → 4 文件（~300+200+250+200），总计约 950 行（头文件开销抵消部分减少）。

## Scope Boundaries

- 不提取 Draw 代码（batch/blitTexture/stretchBlt/affineBlt/submitBlend/submitTransition/submitVFX/fillViewport/submitFullscreenQuad）
- 不提取 TextRenderer 相关（renderText/renderRuby/setFont/textLineHeight）
- 不提取 m_posTexLayout/m_batchQuads/m_batching

## Risks

- **BgfxShaderManager dtor 时机：** 必须在 bgfx::shutdown 之前销毁（程序在 DeviceCore::shutdown 中调用）。已确保 m_shaders.reset() 在 m_deviceCore->shutdown() 之前执行。
- **setPreferredBackend 静态变量：** 保留在 BgfxDeviceCore.cpp 中（在 init 之前调用）。
