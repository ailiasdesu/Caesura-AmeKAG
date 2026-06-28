# Phase 1: 需求分析 & Scope 锁定

## 核心目标

对 Caesura 引擎 16 个模块，按 11 个 galgame 功能域 (D1-D11) 共 76 项验证清单，逐模块排查缺陷，修复后确保引擎可完整支撑 galgame 开发。

## In Scope

- D1-D11 全部 76 项验证，每一项必须手动/自动化验证
- 验证发现的缺陷，先记录到飞书多维表格，再修复
- 修复优先保证引擎不崩溃、核心 galgame 流程可跑通
- 已有 412 测试保持全绿，耦合预算不超标 (entry≤14 di≤14 script≤14 others≤4)
- 全流程 demo (galgame_demo.ks) 连续 5 次无崩溃通过

## Out of Scope (明确不做)

- 编辑器 (Editor/RPC server) 功能完善（最后再做）
- 新功能开发（P3 3D 小游戏、Live2D Cubism SDK 集成）
- 性能优化（除非排查中发现的明显性能缺陷）
- KAG 命令语义完善（只修崩溃和错误行为，不完善"缺失但不崩溃"的功能）
- CI 修复（CI 跑 demo 是后续任务）

## Acceptance Criteria

- [ ] D1-D11 全部 76 项验证完成，结果记录在飞书 Base
- [ ] 所有 P0 缺陷修复完成
- [ ] 412 测试全部通过
- [ ] demo/galgame_demo.ks 连续 5 次无崩溃通过
- [ ] 耦合预算不超标
- [ ] 每个修复 commit 原子化，message 符合 conventional commit

## 依赖 & 风险

- 依赖: 已有 412 测试全绿 (967 assertions passed)
- 依赖: 飞书 Base 已创建（76 项记录），audit-tracker.md 已就绪
- 风险: 排查可能发现深层架构缺陷，需要回溯 Phase 2 重新设计
- 风险: 3 人并行排查可能发现跨模块问题，需要 A 每天汇总裁决
- 假设: 本地有 GPU (D3D11) 和音频设备，渲染/音频可正常验证
