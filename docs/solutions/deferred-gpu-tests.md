# 待实际运行检查项

以下方法因缺乏 GPU / 窗口 / 外部服务上下文，当前无法在单元测试中覆盖。
待引擎完整可运行后（bgfx + SDL3 窗口初始化成功），逐项验证。

| 模块 | 方法 | 阻塞原因 | 验证方式 |
|------|------|---------|---------|
| storage | SaveManager::captureThumbnailPNG() | gfx::frame() 未初始化时 SIGSEGV | 引擎启动后调用 → 检查返回非空 PNG base64 |
| render | 所有 GPU 资源创建测试 | 无窗口环境 | AGENTS.md 第 8 条明确：渲染测试用默认构造+访问器 |

---

*记录日期：2026-06-16 · 分支 codex/archive-expanded-tests*
