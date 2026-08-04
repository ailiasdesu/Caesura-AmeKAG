# 2026-08-04-006 — 性能基线更新（多轮优化后对照）

## 背景

2026-08-03 建立性能基准（test_benchmark.lua：tokenizer 146.75ms/1000tok、
scheduler ~286k tok/s）。此后多轮优化（exprCache 重绑、sandbox io.open
白名单、P1 文本缓存接线+penX、指令钩子间隔等）后重跑对照。

## 对照结果（2026-08-04）

| 指标 | 初始基线 | 当前（两次采样） | 提升 |
|---|---|---|---|
| tokenizer | 587ms / 146.75ms-per-1000tok | 478-541ms / 119-135ms | **8-19%** |
| scheduler | 4001 resumes / 14ms（286k tok/s） | 4001 resumes / 13ms（~308k tok/s） | **8%** |

## 结论

- 优化带来可测增益（tokenizer 分配优化 + exprCache 重绑生效）
- 性能零退化（无一项变慢）
- 新基线：tokenizer ≤135ms/1000tok、scheduler ≥300k tok/s
- 后续优化以新基线为对照（防回归）

## 验证

- Lua 套件 12/12、CaesuraTests 569/569、ctest 10/10、耦合度 PASS
- 无代码变更（纯测量 + 文档）
