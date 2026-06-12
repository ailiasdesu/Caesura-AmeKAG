---
name: Caesura Launch-Ready Plan
created: 2026-06-12
status: active
---

# Caesura (AmeKAG) 发布就绪计划

## 当前基线

257/257 测试全绿，26 api/ 接口，Engine 拆分完成，Live2D PNG 降级可用，基础文档就位。

**剩余障碍：**
- Demo 只是一个冒烟测试，不是真正的 galgame
- 4 个模块无测试（di/minigame/platform/storage）
- CI 三端未验证
- Engine 在测试环境构造就崩
- 编辑器集成不完整

## 赛道

---

### S1: Demo 成为真正的 Galgame

> demo_story.ks 只被解析，从未被执行出画面和声音。

**目标：** Demo 成为可运行的最小完整 galgame——有对话、有背景、有 BGM、有选项、有结局。

**IU S1.1:** Demo KAG 脚本完善
- `scripts/demo_story.ks` 当前只有解析测试
- 扩展为完整流程：标题画面 → 场景1（教室）→ 选项分支 → 场景2A/2B → 结局
- 使用占位符资源（纯色背景、静音音频）

**IU S1.2:** Demo 端到端运行测试
- 创建 `tests/cpp/test_demo_e2e.cpp`
- 完整运行 Demo 30 秒，验证不崩溃
- 验证至少执行了 @text、@bg、@choice、@jump 命令

**IU S1.3:** Demo 资源生成脚本
- 创建 `scripts/generate_demo_assets.lua`
- 生成 Demo 所需的最小资源（纯色 PNG、静音 WAV）
- 使 Demo 可在全新 checkout 后直接运行

---

### S2: 剩余模块测试补齐

**IU S2.1:** `tests/cpp/test_di.cpp` — BackendRegistry 完整测试
**IU S2.2:** `tests/cpp/test_minigame.cpp` — MiniGame 接口完整性
**IU S2.3:** `tests/cpp/test_platform.cpp` — SDL3PlatformBackend 生命周期
**IU S2.4:** `tests/cpp/test_storage.cpp` — SaveManager 完整测试

---

### S3: Engine 可测试性修复

> test_entry.cpp 不测试 Engine 构造因为 SIGSEGV。

**IU S3.1:** 修复 Engine 默认构造
- Engine 构造函数创建 GpuMonitor → 在无 GPU 环境下崩溃
- 改为延迟创建（init 时创建，构造时只设默认值）
- 或添加 headless guard

**IU S3.2:** 添加 Engine 构造测试
- test_entry.cpp 加入：Engine 构造不崩溃
- Engine 构造后 shutdown 不崩溃

---

### S4: CI 三端验证

> macOS CI `|| true` 已移除但从未远端跑过。

**IU S4.1:** 推送到 master 触发 CI
**IU S4.2:** 根据失败日志逐项修复
**IU S4.3:** 三端全绿后 README badge 更新

---

### S5: 编辑器收尾

**IU S5.1:** 完成 R1.2 — Live2D 面板（`GET /api/live2d/models` + `POST /api/live2d/load` + 前端列表）
**IU S5.2:** 完成 R1.3 — 一键打包（`POST /api/build` + 前端 Build 按钮）

---

## 执行顺序

```
S4 (CI) → S3 (Engine修复) → S2 (测试补齐) → S1 (Demo) → S5 (编辑器)
```

S4 先做确认 CI 基线。S3 消除 Engine 构造崩溃。S2 补齐 4 模块测试。S1 做出真正的 Demo。S5 编辑器收尾。

## 验收标准

- CI 三端全绿
- Engine 可在测试中安全构造
- 257 → 280+ 测试全绿
- Demo 在 headless CI 中稳定运行 30 秒不崩溃
- 编辑器可加载 Live2D 模型列表并打包项目