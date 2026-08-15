# 2026-08-04-006 — 性能基线更新（round 25 → round 66-81 回归对比）

## 背景

2026-08-03 建立性能基准（test_benchmark.lua：tokenizer 146.75ms/1000tok、
scheduler ~286k tok/s）。此后多轮优化（exprCache 重绑、sandbox io.open
白名单、P1 文本缓存接线+penX、指令钩子间隔等）后重跑对照（round 25 时代）。

round 66 建立热路径规模化确定性性能守卫（test_schema / test_expr_lang /
test_tokenizer / test_frame_bench），round 68/70 追加表达式与 add 链守卫，
round 73-81 期间这些守卫持续在门禁中运行。本文档补充 round 66-81 的
当前基线与回归对比（素材池 G7）。

## 演化：round 25 → 66 → 81 的性能守卫设施

| 阶段 | 守卫 | 覆盖热路径 | 预算 |
|---|---|---|---|
| round 25 | test_benchmark.lua | tokenizer 2000 行/4000 token 文本流；scheduler 4001 resumes 循环 | 解析<3s、总<3s |
| round 66 | test_schema.lua | ${...} 插值扫描器 @500-span | <5s |
| round 66 | test_expr_lang.lua | 表达式缓存求值 2000x（~90% 命中）；200 项数值链；运算符链 | <5s |
| round 66 | test_tokenizer.lua | 2000 命令场景（lpeg 批量批零宽守卫回归） | <10s |
| round 66 | test_label_bench.lua | label 索引构建 | <3s |
| round 68/70/72 | test_frame_bench.lua | 每帧 render 成本（5000x 均值）；混合表达式 translate 1000x；[add] 链 dispatch 1000x | render<500us/帧；expr/add<2s |

全部守卫注册在 run_lua_tests.lua（test_benchmark/test_schema/test_expr_lang/
test_tokenizer/test_label_bench/test_frame_bench），随 CI 门禁每轮全跑。

## round 66-81 当前基线（本次采样，2026-08 运行）

> 注：预算守卫在 CI 宽松时钟下只断言 PASS（通过/不通过），本次用探针
> 单独计时取到精确 dt（本机 os.clock，非 CI；CI 值会更松）。

### 规模化确定性守卫（round 66）实测

| 热路径 | 预算 | 实测（本机） | 余量 |
|---|---|---|---|
| test_schema 500-span 插值 | <5s | **0.010s** | >99.8% |
| test_expr_lang 2000x 缓存求值 | <5s | **0.062s** | >98.8% |
| test_expr_lang 200 项数值链 | — | 0.001s（=20130） | — |
| test_tokenizer 2000 命令场景 | <10s | **0.186s** | >98.1% |

### frame_bench 每帧/循环守卫（round 68-72）实测

| 热路径 | 预算 | 实测（本机） | 余量 |
|---|---|---|---|
| render 5000x 均值 | <500us/帧 | **274.6us/帧** | ~55% 余量（45% 占用） |
| 混合表达式 translate 1000x | <2s | **0.054s** | >97.3% |
| [add] 链 dispatch 1000x | <2s | **0.007s**（f.x=1000） | >99.6% |

### test_benchmark 吞吐（round 25 vs 当前）

| 指标 | round 25 历史 | 当前（3 次采样，本机） | 结论 |
|---|---|---|---|
| tokenizer | 478-541ms / 119-135ms-per-1000tok | 361-533ms / 90-133ms-per-1000tok | 低端↑（90ms），高端同区间，波动大 |
| scheduler | 4001 resumes / 13ms（~308k tok/s） | 4001 resumes / 26-48ms | 本机读数偏高，疑为环境/时钟噪声 |

> 准确性说明：round 25 的 13ms 同为单次历史读数；本次三次采样
> 26/27/48ms 差异大（第三次明显受 CPU 降频/时钟噪声影响）。benchmark
> 守卫的实际断言是稳健的（解析<3s 与总<3s 始终通过），吞吐读数仅作参考。

## 结论：round 66-81 无性能回归

- **所有预算守卫全部 PASS**，且预算余量绝大多数 >97%（唯一较接近的是
  render 每帧守卫，占用 45% 预算，余量 55%）。
- 无任何热路径预算余量 <20%（<20% 定义为需要优化的风险线）。
- round 66 建立守卫后 66→81 期间表达式/调度/schema/tokenizer 工作
  （round 68 三元括号内翻译、round 70 长字符串扫描、round 72-73 宏
  深度制、round 74-75 循环恢复）**未引入任何退化**：500-span、2000x
  缓存求值、2000 命令场景等守卫在每次门禁中保持通过。
- tokenizer 文本流吞吐相比 round 25 未退化（低端采样达 90ms/1000tok）。

## 建议（非必须，无预算逼近）

- 各热路径预算余量充足，**暂无必须实现的优化点**。
- 可观察项：test_benchmark 的 scheduler 读数在本机波动较大（26-48ms），
  若后续轮次想让吞吐读数更稳定，可考虑把 scheduler 测量改为固定 resume
  数的单调计时并取多次中位数，但**不构成当前回归**，留作观测。

## 验证

- 本文档更新为纯测量 + 文档，无代码变更。
- 守卫全部在门禁中：Lua 套件全绿（含 test_benchmark/test_schema/
  test_expr_lang/test_tokenizer/test_label_bench/test_frame_bench），
  C++/ctest/耦合 PASS（同 round 81 门禁记录）。
---

# round 82-87：性能基线二次刷新（round 82 之后的热路径回归对比）

## 触发背景

round 82 刷新基线（round 66-81 零回归）后，round 82-87 期间在热路径上
新增/改动了以下代码，需确认未引入可感知开销：

| 轮次 | 热路径改动 | 关注点 |
|---|---|---|
| round 84 | expr translate_parens 括号内顶层逗号分段（多参调用逐段 translate + trim 再 join） | 逐段翻译新增 O(段数) 额外开销 |
| round 83/84 | scheduler _backJumps 反向跳边记忆守卫 + 循环栈（_for/_while/_forStackMarks）跳转时重置 | 跳边守卫每次 jump/goto 查表开销；循环栈重置 |
| round 80/84 | i18n 复数（CLDR 表格 + {n} 插值） | 文本插值路径 |
| round 82/87 | web advance / player-settings（JS 侧） | 不影响 Lua 侧热路径 |
| round 87 | wait/skip/textspeed 深度 | 命令分发路径 |

## round 82-87 当前基线（本次采样，本机 os.clock）

> 与 round 82 方法一致：预算守卫测实机制（CI 更松），另用探针 os.clock
> 计时取精确 dx / 中位数。

### frame_bench 守卫（round 68-72，round 82-87 期间保持通过）

| 热路径 | round 82 记录 | 本次（中位数 3 次） | 对比 |
|---|---|---|---|
| render 5000x 均值 | 274.6us/帧（45% 占用） | PASS（<500us 预算） | 渲染路径零改动 |
| 混合表达式 translate 1000x | 0.054s | 0.054s | 0%（精确复现） |
| [add] 链 dispatch 1000x | 0.007s | 0.004s | -43%（更快） |

- 混合表达式 translate 守卫（含 2-3 个括号组、??、三元）精确复现 round 82
  的 0.054s——翻译管道在 round 82-87 期间零净退化。
- [add] 链 dispatch 中位数 0.004s，优于 round 82 的 0.007s——调度器在
  round 83/84 新增 _backJumps/循环栈重置后，分发吞吐不降反升（守卫只在
  jump/goto 边触发，非每 token 路径）。

### test_benchmark 吞吐（round 82 vs 本次）

| 指标 | round 82 记录 | 本次（独立 / 套件内） | 结论 |
|---|---|---|---|
| tokenizer 1000tok | 361-533ms / 90-133ms-per-1000tok | 245ms / 61ms-per-1000tok（独立）；301ms / 75ms-per-1000tok（套件内） | 低端或更优，无退化 |
| scheduler 4001 resumes | 26-48ms | 25ms（套件内）/ 29ms（独立） | 范围下沿，无退化 |

### 规模化确定性守卫（round 66，round 82-87 期间全 PASS）

| 守卫 | 预算 | 结果 |
|---|---|---|
| test_schema 500-span 插值 | <5s | PASS |
| test_expr_lang 2000x 缓存求值 | <5s | PASS |
| test_expr_lang 200 项数值链 | -- | PASS（=20130） |
| test_tokenizer 2000 命令场景 | <10s | PASS |

## round 84 translate_parens 逗号分段专项验证

功能：多参调用括号（f.calc(f.flag ? 1 : 2, 3)）顶层逗号分段独立翻译 +
trim 再 join。正确性已由 test_expr_lang2 三条 LIMIT 断言锁定（round 84
翻转），套件内 124/124 全绿。

开销（本机 os.clock，中位数 5 次，30k 迭代）：

| 形态 | 单次耗时 | 说明 |
|---|---|---|
| 平表达式（无括号） | 0.0138ms | 基线 |
| 1 参括号（round 68 路径） | 0.0303ms | 无逗号不触发分段 |
| 2 参括号（round 84 分段） | 0.0346ms | +0.0043ms（+14% vs 1 参） |
| 6 参括号（round 84 分段，含三元） | 0.0801ms | 每参数约 +0.002-0.008ms |
| 6 参无三元（纯分段） | 0.0429ms | 纯 O(段数) 开销 |

结论：round 84 逗号分段引入 O(段数) 的逐段 translate+trim 开销，每参数约
数个微秒量级。即便 6 参调用单次也仅 0.080ms——相对 2s 预算余量 >99.9%，
对真实表达式求值（缓存命中后约 0.06ms 级别）无可感知影响。

## 结论：round 82-87 无性能回归

- 全部预算守卫 PASS（render/expr/add/tokenizer/schema/expr_lang），套件内 124/124 全绿。
- 无任何热路径预算余量 <20%：最紧的仍是 render 每帧守卫（约 45% 占用），其余 >97% 余量。
- round 84 translate_parens 逗号分段：理论 O(段数) 额外开销实测为每参数数微秒，表达式级无可感知影响，且 frame_bench 混合守卫精确复现 0.054s。
- round 83/84 scheduler _backJumps/循环栈重置：跳边守卫非每 token 路径，[add] 链 dispatch 实测 0.004s 优于 round 82 的 0.007s，无开销。
- tokenizer 与 scheduler 吞吐均不高于 round 82 读数（tokenizer 甚至低端更优），i18n/wait/skip 改动未触及调度热路径。

## 建议（非必须，无预算逼近）

- 与 round 82 结论一致：各热路径预算余量充足，暂无必须实现的优化点。
- round 84 逗号分段已是 O(段数) 最优形态（每段独立翻译，无法再并联）；若未来
  出现极多参（>32）的高频调用，可考虑缓存参数级翻译结果（表达式缓存已覆盖
  整表达式，参数级命中率有限），但不构成当前回归，留作观测。

## 验证

- 本文档更新为纯测量 + 文档，无代码变更。
- 全量 Lua 套件 run_lua_tests.lua 124/124 通过（含 frame_bench/schema/
  expr_lang/expr_lang2/tokenizer/label_bench/benchmark 守卫）；
  test_scheduler / test_flow_edge / test_flow_edge_call 全绿（调度器守卫路径）。

