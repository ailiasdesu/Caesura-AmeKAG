---
module: render
tags: [bgfx, gpu, headless, crash, guard]
problem_type: runtime-crash
---

# bgfx API 调用前的 GPU 初始化守卫模式

## 症状

headless 测试（无 GPU 上下文）中调用带 GPU 上传/创建逻辑的方法
（`TextRenderer::loadTTF`、`BgfxMiniGameBackend::ensureGpuResources`）
导致 SIGSEGV——`bgfx::createTexture2D`/`createShader` 在 `bgfx::init` 之前
调用是未定义行为。

## 错误尝试

用 `bgfx::getCaps()` 判空守卫——**getCaps() 本身在未 init 时也崩溃**
（内部访问 `s_ctx`）。

## 正确模式

每个子系统维护自己的 GPU 可用标志，由生命周期事件驱动：

- `TextRenderer`：`m_initialized`（`init()` 置 true；`shutdown()` 和
  `onDeviceLost()` 置 false）——`loadTTF` 开头 `if (!m_initialized) return false;`
- `BgfxMiniGameBackend`：`ensureGpuResources` 开头检查
  `m_renderDevice && m_renderDevice->isInitialized()`——设备是 GPU 可用性的
  权威来源。为此给 `IRenderDevice` 增加了 `virtual bool isInitialized() const = 0;`
  （`BgfxRenderDevice` 报告 `m_bgfxInitialized`，`NullRenderDevice` 恒 false）。

## 测试

- `test_render_pipeline.cpp`："loadTTF refuses without GPU context"——
  真实字体路径 + 未初始化 GPU → 优雅返回 false（修复前 SIGSEGV）。
- `test_mini_game.cpp`："enter(0) programmatic activation without GPU"——
  无设备/未初始化设备 → enter 安全 no-op。
