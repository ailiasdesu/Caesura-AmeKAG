---
date: 2026-06-17
topic: engine-stability-e2e-demo
---

## Summary

逐模块加固引擎稳定性——从零依赖模块向组合根逐层排查 null 路径、异常处理和资源生命周期——然后用一个完整 galgame demo 本地验证端到端流程可跑通。

## Problem Frame

引擎当前状态：16 模块、~400 测试通过，但端到端 galgame 流程跑不通。当时记录有 5 个已知失败测试（音频加载、图片解码、Lua 脚本、bgfx null 崩溃，详见当时的 STRATEGY.md，现已删除）。用户反馈引擎崩溃、KAG 执行逻辑缺陷、渲染/音频表现异常、脚本绑定层断裂同时存在。在添加新功能之前，必须先让引擎稳定跑通一部完整的 galgame。

## Key Decisions

- **逐模块加固而非 e2e smoke test 驱动。** 模块间通过 BackendRegistry 接口隔离——逐层加固确保每层稳定后再进入下一层，避免多个崩溃源同时出现难以定位。
- **加固顺序从零依赖模块向组合根推进。** L0（零依赖）→ L1（仅依赖 di）→ L2（依赖 2-3 模块）→ L3（枢纽）→ L4（组合根）。每层加固后本地验证，再进入下一层。
- **加固完成后本地开发 demo 验证，不跑 CI。** CI 修复是独立任务，本次目标是在本地确保引擎能跑通 galgame 流程。
- **加固期间不修 KAG 逻辑 bug、不加新功能。** P1-P3 已实现的命令保持不变；发现的 KAG 逻辑 bug 记录下来但不在此次修复——本次只做防御性加固。

## Requirements

### 防御性加固

- R1. 每个 BackendRegistry getter 返回 null 时，调用方不崩溃——降级为 null backend 行为或记录错误后跳过。
- R2. 每个模块的 init/shutdown 路径幂等且可安全重入——重复 init 不泄漏资源，未 init 就 shutdown 不崩溃。
- R3. 所有文件 I/O 操作（archive、resource、storage、script 加载）失败时返回明确错误码，不抛异常穿透模块边界。
- R4. bgfx 初始化失败时引擎可降级运行（headless 模式或 null render backend），不崩溃。
- R5. Lua VM 执行中脚本错误被 pcall 捕获，错误信息记录到 DebugManager，不导致引擎崩溃。
- R6. 异步加载（AsyncLoader）失败时回调携带错误信息，调用方有超时兜底，不无限等待。

### 加固分层

- R7. L0（platform、input、steam）：SDL 窗口/事件创建失败路径有 fallback；input router 未注入时事件丢弃不崩溃；Steam 条件编译隔离。
- R8. L1（audio、job、debug、archive、storage）：SoLoud init 失败退回 NullAudioBackend；线程池析构等待所有任务完成；ring buffer 溢出截断旧数据；文件损坏返回错误而非崩溃。
- R9. L2（resource、live2d、minigame、render、rpc）：图片解码失败返回占位纹理；null animation/minigame backend 默认行为无副作用；RPC 端口占用时优雅降级。
- R10. L3（di、script）：BackendRegistry::getXxx() 全部含 null 检查；Lua 绑定层参数校验完整——类型错误返回 Lua error 而非 C++ 崩溃。
- R11. L4（entry）：Engine::init() 四阶段中任一步骤失败回滚已初始化的资源；ErrorUI 在任何 render 状态下可安全渲染。

### Demo 验证

- R12. Demo 流程覆盖：title 画面 → 多场景叙事（≥3 个场景）→ 多角色立绘切换（≥2 角色带 pos）→ 选项分支 → 双结局 → 存档/读档 → CG 画廊 → 音乐室。
- R13. Demo 从头到尾执行 5 次，不出现崩溃或未处理异常。

## Key Flows

- F1. 模块加固流程
  - **Trigger:** 进入一个新层级
  - **Steps:** (1) 审查该层所有模块的 `.cpp` 实现文件，标记 null 指针解引用、未检查返回值、资源泄漏点 (2) 添加防御代码 (3) 本地构建验证零编译错误 (4) 运行该模块已有测试确保不回归 (5) 手动触发异常路径验证降级行为
  - **Outcome:** 该层模块在异常路径下不崩溃

- F2. Demo 开发与验证流程
  - **Trigger:** L0-L4 全部加固完成
  - **Steps:** (1) 编写 demo KAG 脚本覆盖 R12 所有节点 (2) 准备最小资源集（背景图、立绘、BGM、语音） (3) 启动引擎执行 demo (4) 记录崩溃点和异常行为 (5) 修复后重跑至 5 次无崩溃
  - **Outcome:** 引擎可稳定执行完整 galgame 流程

## Acceptance Examples

- AE1. 覆盖 R1 — 在没有注入 AudioBackend 时调用 playBgm：引擎在 debug log 记录 "AudioBackend not available"，继续执行后续命令，不崩溃。
- AE2. 覆盖 R4 — 在无 GPU 环境启动引擎：bgfx init 失败，引擎降级为 headless 模式，Lua 脚本仍可加载执行。
- AE3. 覆盖 R5 — KAG 脚本中 `[ch name=nil]` 触发 Lua error：pcall 捕获，ErrorUI 显示错误上下文，引擎不退出。
- AE4. 覆盖 R10 — Lua 调用 `render.drawTexture(123, "not_a_table")`：绑定层参数校验失败，返回 `nil, "expected table as second argument"`，不 segfault。
- AE5. 覆盖 R13 — Demo 第 3 次执行时在场景切换时崩溃：修复后重跑至连续 5 次无崩溃。

## Scope Boundaries

### Deferred for later

- CI 修复与 CI 上跑 demo 测试
- KAG 逻辑 bug 修复（命令行为不正确但不导致崩溃的问题）
- 性能优化
- 新功能开发（P3 3D 小游戏等）
- 缩略图 `captureThumbnailPNG`（依赖 GPU 上下文）

### Outside this product's identity

- 更换渲染后端（bgfx 以外的方案）
- 跨平台 demo 验证（本次仅 Windows 本地）

## Success Criteria

- L0-L4 所有模块的异常路径降级行为可手动验证
- Demo 流程覆盖全部 R12 节点
- Demo 连续 5 次从头到尾执行无崩溃
- 已发现的崩溃点全部修复，剩余 KAG 逻辑 bug 记录但不阻塞

## Dependencies / Assumptions

- **假设**：P1-P3 已实现的 KAG 命令逻辑正确——加固不改命令行为，只加防御层。
- **假设**：本地开发环境有 GPU（D3D11）和音频设备——demo 渲染和音频播放可正常验证。
- **依赖**：`docs/solutions/` 中已有的崩溃修复模式（如 `engine-constructor-sigsegv-testing.md`）可作为加固参考。
- **依赖**：`tests/mocks/NullJobSystem.h`、各模块的 `Null*Backend` 作为降级参考实现。

## Outstanding Questions

- Q1. bgfx init 失败时的 headless 模式当前支持到什么程度？需在 L2 加固时确认 `NullGpuMonitor` 和 null render path 的完整性。
- Q2. Demo 所需的最小资源集（图片、音频）从何而来？需准备或生成占位资源。
