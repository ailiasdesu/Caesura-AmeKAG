---
name: Caesura Final-Phase Plan
created: 2026-06-12
status: active
---

# 最终阶段计划

## 基线

277/277 测试。架构完成。Engine 修复。RPC 端点就位。文档齐全。

## 剩余差距

| 差距 | 严重度 |
|------|--------|
| `test_di.cpp` / `test_minigame.cpp` 未创建 | 低 |
| Demo 不是真正可跑的 galgame | 中 |
| CI 三端未远端验证 | 中 |
| 前端 Live2D 面板/Build 按钮未完成 | 低 |

## 赛道

### F1: CI 三端验证（最高优先）

推送 master 触发 CI → 根据日志修复 → 三端全绿。这是发布前的硬性条件。

### F2: 测试最后补齐

`test_di.cpp`（BackendRegistry 完整测试）+ `test_minigame.cpp`（MiniGame 接口测试）

### F3: Demo 成为可跑 Galgame

扩展 demo_story.ks → 50 行完整脚本 → 端到端运行测试验证不崩溃

### F4: 编辑器前端收尾

Live2DPanel.jsx（模型列表+加载）、AssetPanel.jsx（Build 按钮）

---

## 非目标

- 新功能开发
- 性能优化
- 新平台支持