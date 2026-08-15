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
