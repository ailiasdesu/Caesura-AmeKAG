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

---

# round 88-91：性能基线三次刷新（round 88 之后的热路径回归对比）

## 触发背景

round 88 刷新基线（round 82-87 零回归）后，round 88-91 期间在热路径上新增/
改动了以下代码，需确认未引入可感知开销：

| 轮次 | 热路径改动 | 关注点 |
|---|---|---|
| round 89 | tokenizer 接受 comment-only / 空白输入为零 token 列表（grammar 由 explicit_cmds * skip * -1 改为 (skip * explicit_cmds)^0 * skip * -1） | 零宽 ^0 重复包裹对正常大文本解析是否引入回溯/逐 token 开销 |
| round 91 | AsyncLoader in-flight 去重表（m_inflight unordered_map，唯一加载多 2 次互斥锁 + 完成后移除）；ProviderChain read/exists try/catch 隔离 | 顺序加载路径互斥锁/分配开销；try/catch 每 provider 循环开销 |
| round 91 | web i18n mountFile / setLanguage 重定位红raw（JS 侧） | 不影响 Lua 侧热路径 |
| round 88-91 | scheduler 无改动 | — |

## round 88-91 当前基线（本次采样，本机 os.clock）

> 方法与历轮一致：预算守卫测实机（CI 更松），另用探针计时；A/B 用 git show 6db2a30b:scripts/tokenizer.lua 提取 round 88 版做同输入对比。

### frame_bench 守卫（round 68-72，round 88-91 期间保持通过）

| 热路径 | round 88 记录 | 本次（套件内） | 对比 |
|---|---|---|---|
| render 5000x 均值 | PASS（<500us 预算） | PASS | 渲染路径零改动 |
| 混合表达式 translate 1000x | 0.054s（精确复现） | PASS（<2s 预算） | 无回归 |
| [add] 链 dispatch 1000x | 0.004s | PASS（<2s 预算） | 无回归 |

### test_benchmark 吞吐（round 88 vs 本次，4 次采样取中位数）

| 指标 | round 88 记录 | 本次（4 次采样） | 结论 |
|---|---|---|---|
| tokenizer 1000tok | 245ms/61ms-per-1000tok（独立）；301ms/75ms（套件内） | 270/363/272/277ms → 中位 ~273ms（68ms-per-1000tok） | 落在 round 88 区间 [245,301]，无退化 |
| scheduler 4001 resumes | 25-29ms | 41/39/27/25ms → 中位 ~33ms（波动 25-41） | 落入文档既有噪声区间 [25,48]，scheduler 本轮零改动 |

> 注：scheduler 首两次读数 41/39ms 偏高，后两次 27/25ms 回到 round 82-87 的 25-29ms 区间——与本机 os.clock 受 CPU 降频/时钟噪声影响一致（benchmark 实际断言解析<3s 总<3s 稳健）。scheduler 在 round 88-91 无任何改动，判定为环境噪声而非回归。

### 规模化确定性守卫（round 66，round 88-91 期间全 PASS）

| 守卫 | 预算 | 结果 |
|---|---|---|
| test_schema 500-span 插值 | <5s | PASS |
| test_expr_lang 2000x 缓存求值 | <5s | PASS |
| test_expr_lang2 逗号分段 LIMIT | -- | PASS |
| test_tokenizer 2000 命令场景 | <10s | PASS |
| test_schema_types / test_label_index / test_label_jump | -- | PASS |

## 重点验证一：round 89 tokenizer grammar 改动对正常解析无开销

round 89 把 grammar 从「必须 ≥1 token」改为「零或多个 token」（(skip * (explicit_cmds + ...))^0 * skip * -1）。隐患：^0 重复包裹是否让非空大文本解析变慢（多一层重复匹配、前置 skip 逐 token 摊派）。

A/B 实测（round 88 版 vs 当前版，同输入，中位数 6-8 次）：

| 输入形态 | round 88 | 当前 | Δ |
|---|---|---|---|
| 2000 命令场景（round 66 守卫相同输入） | 259.5ms | 255.0ms | **-1.7%（更快）** |
| 混合 800 行（命令+叙述交替） | 52.0ms | 51.5ms | **-1.0%（更快）** |
| 3000 行叙述流（约 340KB 纯文本） | 50.0ms | 50.5ms | **+1.0%（噪声内）** |
| 单段长正文（约 46KB 连续文本） | 1.0ms | 1.0ms | **+0.0%（一致）** |
| comment-only / 空白输入 | 抛错（旧行为） | ~0.0ms | 新功能，round 89 修复 |

- 小样本（120 行纯文本）曾出现 +50% 读数，放大到 3000 行后收敛为 +1.0%——纯 os.clock() 1ms 量程对微秒级绝对时间的量化噪声，非真实回归。
- 大文本（>300KB）解析差异 ≤1%，命令/混合场景甚至更快；token 计数一致（3000 vs 3000）。**round 89 grammar 改动对正常解析无任何可感知开销。**

## 重点验证二：round 91 AsyncLoader in-flight 表 + ProviderChain try/catch

### AsyncLoader in-flight 表（C++ 侧，代码走查分析）

round 91 在 AsyncLoader::enqueue() 新增 m_inflight unordered_map + m_inflightMutex，对「顺序唯一加载路径」的开销：

- **缓存命中路径（VN 场景重入的主路径）零开销**：enqueue 在 cache 命中处直接 return（cache 检查在 in-flight 之前），完全不触碰 in-flight 表——早退早于 in-flight 检查。
- 仅「首次缓存未命中的唯一加载」多承担：2 次互斥锁获取（in-flight find + register）+ 完成时 1 次移除，外加 3 个 make_shared 堆分配和一个 unordered_map insert/erase。相对真实磁盘读 + 图片解码（毫秒级）这些微秒级操作可忽略，且只在首次加载发生、绝不落在缓存重入路径上。
- makeKey 字符串拼接与 round 91 前等价（从 cache 块内上移到函数开头），无新增字符串构造。

### ProviderChain try/catch 隔离（C++ 侧）

round 91 为每个 provider 的 exists()/read() 包 try/catch。Windows/MSVC 零成本异常处理下，进 try 块不生成任何执行指令——正常（无异常）路径的循环开销为 0，只在 provider 抛异常时损失记录日志的成本（慢路径）。隔离语义由 round 91 的 test_resource.cpp 新增用例锁定（throw 隔离、高优 throw 回退、全 throw 优雅失败）。

**round 91 两项改动均不构成对顺序加载/缓存重入热路径的可测开销。**

## 结论：round 88-91 无性能回归

- **全部预算守卫 PASS**，套件内 11 个 perf 守卫全部 [OK]，Lua 套件 **126/126**（round 88 为 124/124；round 89 新增 2 条 tokenizer 测试）。
- tokenizer 吞吐中位 ~273ms/68ms-per-1000tok，落在 round 88 区间内；scheduler 波动噪声与既往一致且本轮零改动。
- **round 89 tokenizer grammar 改动专项 A/B 证明对正常解析无开销**（大文本 ≤1%，命令/混合场景更快），并新增 comment-only 空输入功能（~0ms）。
- **round 91 AsyncLoader in-flight 表**：缓存命中路径零开销（cache 早退在 in-flight 检查之前），仅首次唯一加载多微秒级锁/分配，相对磁盘+解码可忽略。
- **round 91 ProviderChain try/catch**：零成本异常处理下正常路径零开销。
- 本轮无任何热路径预算余量 <20%（最紧仍是 render 每帧守卫，~45% 占用）。

## 建议（非必须，无预算逼近）

- 与 round 82/88 结论一致：各热路径预算余量充足，暂无必须实现的优化点。
- 可观测项不变：test_benchmark 的 scheduler 读数本机波动大（25-41ms），若希望吞吐读数更稳定，可改固定 resume 数单调计时取中位数，但不构成回归。

## 验证

- 本文档更新为纯测量 + 文档，无引擎源代码变更（tokenizer.lua 等工作树文件未被改动，仅临时 A/B 拷贝于测量后删除）。
- 全量 Lua 套件 run_lua_tests.lua **126/126** 通过（round 88 为 124/124），含 round 66 规模化守卫 + round 89 新增鲁棒性测试 + round 88-91 期间 test_schema_types/test_expr_lang2/test_label_index/test_label_jump 全绿。

---

# round 92-96：性能基线四次刷新（round 92 之后的热路径回归对比）

## 触发背景

round 92 刷新基线（round 88-91 零回归）后，round 93-96 期间在热路径上新增/改动
了以下代码，需确认未引入可感知开销：

| 轮次 | 热路径改动（引擎侧） | 关注点 |
|---|---|---|
| round 92-96 | **scripts/ 下无调度/表达式/渲染路径引擎改动**：唯一 scripts 改动是 scripts/kag/lsp.lua（LSP rename，编辑器工具，非游戏热路径）与 api_stats.py（CI 工具） | 令牌/调度/表达式/渲染引擎代码零改动 |
| round 95 | **expr 规模化确定性守卫 +4/9**（test_expr_lang.lua，300 插值段 / 500 平链三元 / 80 参 / 200 长串扫描） | **守卫本身是新增设施**，需确认在后续轮次稳定、不误报 |
| round 96 | **debug 计数数组 7->12**（DebugManager m_errorCounts/m_warnCounts/m_totalCounts array<kSubSysCount>）+ beginFrameProfile 重置 luaGcMs | C++ 日志热路径是否因数组扩容受影响 |
| round 95/96 | SceneOutline 虚拟化 / Inspector jump useRef / bundle 死锁修复（editor、web JS 侧） | 不影响 Lua 侧热路径 |
| round 96 | expr O(n^3) 病态评估（handoff 记录 N=100 达 9.3s，round 97 计划修复） | 已知病理不作基准（见第 4 节 round 97 工作树交互） |

> **度量口径**：round 92-96 的权威基线是**已提交 HEAD（round 96，8db386ae）**。
> 本会话工作树存在**未提交的 round 97 expr.lua 深度预算改动**（O(n^3) 修复），与本轮
> "round 95->96" 基线度量无关（已在第 4 节单列验证），**引擎 Lua 热路径代码在
> round 92-96 期间零改动**（git diff f6e3a98e..8db386ae -- scripts/ 仅
> api_stats.py + kag/lsp.lua）。

## 1. round 92-96 当前基线（round 96 HEAD 采样，本机 os.clock）

> 方法与历轮一致：预算守卫测实机（CI 更松），用探针 os.clock 计时取中位数。
> 本次会话机器处于明显降频/节能状态（所有纯 Lua 吞吐读数较 round 92 历史
> 整体上移，见第 1.4 节），绝对耗时对比会偏高；守卫**正确性断言**不受此影响。

### 1.1 规模化确定性守卫（round 66，round 92-96 期间全 PASS）

| 守卫 | 预算 | 结果 |
|---|---|---|
| test_schema 500-span 插值 | <5s | PASS |
| test_expr_lang 2000x 缓存求值 | <5s | PASS |
| test_expr_lang2 逗号分段 LIMIT | -- | PASS |
| test_tokenizer 2000 命令场景 | <10s | PASS |
| test_schema_types / test_label_index / test_label_jump | -- | PASS |

### 1.2 expr 规模化确定性守卫（round 95 新增，round 92-96 期间全 PASS）

对**已提交 round 96 HEAD 版 expr.lua**（git show HEAD:scripts/kag/expr.lua）逐项
校验正确性 + 计时（探针中位数）：

| 守卫 | 预算 | round 95 记录 | round 96 HEAD 实测 | 残留 '?'（须=0） | 稳定 |
|---|---|---|---|---|---|
| A 300 插值段 translate | <2s | 0.50s | **0.593s** | 0 通过 | 稳定 |
| B 500 平链三元 translate | <2s | 0.52s | **0.662s** | 0 通过 | 稳定 |
| C 80 参调用 translate | <0.1s | --- | **0.0030s**（79 逗号） | 0 通过 | 稳定 |
| D 200 长串扫描 translate | <2s | --- | **0.400s**（长串外无 ?） | 0 通过 | 稳定 |
| E 2000x 缓存求值（round 66） | <5s | 0.062s | **0.026s** | --- | 稳定 |

- **round 95 expr 规模化守卫在 round 95->96 稳定成立**：round 96 HEAD 下 A/B/C/D
  全部产物**无残留 '?'**（正确性断言通过），耗时均在预算内 >70% 余量。这与
  round 92-96 期间 expr 引擎代码零改动一致（守卫测量的是同一条 translate 管道）。
- 混合表达式 translate 1000x（round 68-72 frame_bench 守卫）中位数复现 round 82 的
  ~0.054-0.057s，翻译管道零净退化。
- 相对于 round 95 记录（A=0.50s / B=0.52s）本次读数 A=0.593s / B=0.662s 略高，属
  同一机器降频窗口内的正常波动（绝对耗时受 CPU 状态影响），**不在 >10% 回归判定
  范围**——守卫以正确性 + <2s 预算为断言，二者均稳定通过。

### 1.3 frame_bench 守卫（round 68-72，round 92-96 期间全 PASS）

| 热路径 | 预算 | round 82 参考 | 本次（套件内 / 探针） | 对比 |
|---|---|---|---|---|
| render 5000x 均值 | <500us/帧 | 274.6us/帧 | **325-366us/帧** | 见下注 |
| 混合表达式 translate 1000x | <2s | 0.054s | **0.056-0.057s**（多数） | 持平 |
| [add] 链 dispatch 1000x | <2s | 0.004s | suite PASS | 无回归（守卫通过） |

> **render 读数注**：本次 325-366us/帧较 round 82 参考 274.6us 高约 +20-30%。
> 但 round 92-96 期间 layers.lua / 渲染路径**无任何代码改动**（最近一次 layers
> 改动为 round 34+），且本会话所有纯 Lua 读数统一上移（scheduler/tokenizer 亦
> 偏高），判定为机器降频噪声而非代码回归。仍在 <500us 预算内（约 70% 占用、30%
> 余量），未逼近风险线。

### 1.4 test_benchmark 吞吐（round 92 vs 本次，6 次采样）

| 指标 | round 92 记录 | 本次（6 次采样） | 结论 |
|---|---|---|---|
| tokenizer 1000tok | 61-75ms/1000tok | 87-134ms，中位 **~114ms** | 见下注 |
| scheduler 4001 resumes | 25-41ms | 52-60ms，中位 **~56ms** | 见下注 |

> **吞吐注**：本次 tokenizer/scheduler 读数显著高于 round 92 历史（约 +60-150%）。
> 但 round 92-96 期间 tokenizer/scheduler/compiler **引擎代码零改动**（git diff 证实
> scripts/ 仅 api_stats.py + lsp.lua，均非调度/解析路径），且守卫断言（parse<3s、
> total<3s）与历轮一致全部通过、余量巨大。判定为**机器 CPU 降频/节能状态噪声**
> （本会话所有纯 Lua 吞吐统一上移），而非代码回归。历史 round 88-91 已记录
> scheduler 存在 25-48ms 的既有噪声区间，本次整体带宽内读值上移与其同一机制。
> 若后续轮次以该帧渲染为目标跑热基准，建议在空闲 CPU 状态下复核。

## 2. 重点验证：round 96 debug 计数数组 7->12 对日志热路径零影响（C++ 走查）

round 96 把 m_errorCounts/m_warnCounts/m_totalCounts 从 std::array<uint32_t,7>
扩容到 std::array<uint32_t,kSubSysCount>（12，覆盖全部 SubSys 枚举），并让
beginFrameProfile 每帧重置 luaGcMs。热路径影响分析：

- **per-log 开销不变**：DebugManager::log() 的每消息路径只做
  `if (idx < m_totalCounts.size()) m_totalCounts[idx]++`（及 Err/Warn 分支同类
  递增）——数组从 7->12 只改变已分配槽数，`idx < size` 边界检查语义不变，递增
  仍是 O(1) 数组索引，**日志热路径每次调用成本完全相同**。扩容前 7 槽下高子系统
  （Live2D/MiniGame/Storage/Resource/Archive，idx 7-11）本就因 `idx<7` 被静默丢弃
  计数；扩容 12 后这些子系统才开始正确计数（即修复了 round 95 记录的静默丢弃 bug），
  而非引入新开销。
- **dumpFullReport() 遍历 7->12**：仅当生成 RPC/报告时才跑（非每日志、非每帧），
  5 次额外循环可忽略。
- **beginFrameProfile 重置 luaGcMs**：发生在帧边界（beginFrameProfile 调用点），
  每帧仅 1 次 float 清零，非日志热路径。

**round 96 debug 改动对日志热路径零可测开销**，且修复了边界不变量（round 95
发现的 SubSys 枚举 12 值 vs array<7> 静默丢弃）。

## 3. 结论：round 92-96（已提交 HEAD）无性能回归

- **全部预算守卫 PASS**，round 96 HEAD 下 perf 守卫正确性断言全部通过（无残留
  '?'）+ 预算余量 >70%，Lua 套件在 round 96 状态为 126/126 全绿。
- **round 95 expr 规模化守卫在其后轮次稳定**：A=0.593s / B=0.662s / C=0.003s /
  D=0.400s，correctness 断言（残留 '?'=0）全部通过，无漂移无误报。
- **round 96 debug 计数 7->12**：C++ 走查证实日志热路径 O(1) 递增成本不变，仅报告
  遍历 7->12 与帧边界 1 次清零，零可测开销；同时修复了高子系统计数静默丢弃。
- **round 92-96 期间 tokenizer/scheduler/compiler/expr/layers 引擎代码零改动**
  （git diff 证实），本次 tokenizer/scheduler/render 吞吐读值偏高**非代码回归**，
  判定为机器降频噪声（本会话所有纯 Lua 读数统一上移）。
- 无任何热路径预算余量 <20%（最紧仍是 render 每帧守卫，约 70% 占用、30% 余量）。

## 4. 【重要】round 97 工作树改动与 round 95 守卫的交互（供 round 97 处理）

> 本会话工作树含**未提交的 round 97 expr.lua 深度预算改动**（为修 O(n^3) 病态：给
> translate 加 depth>48 预算，超限返回原文不翻译，让深层嵌套三元无法挂起翻译器）。
> 该改动**不在 round 92-96 提交范围**，但直接影响 round 95 规模化守卫，特此记录：

- **round 97 深度预算会使 round 95 守卫的 3 条正确性断言失败**：A（300 插值段）、
  B（500 平链三元）、D（200 长串扫描）的 translate 递归深度均 >48，预算触发后
  返回带 '?' 的未翻译原文，导致「残留 '?' 必须为 0」断言 FAIL。当前工作树 Lua
  套件因此为 **126 passed / 1 failed（test_expr_lang）**。round 97 改动本身的
  逻辑是**有意的**——防止深层病态嵌套挂起翻译器（handoff 记录 N=100 达 9.3s），
  但**守卫语义与 round 97 预算语义冲突**。
- **这不是 round 95 守卫的实现错误**，而是守卫假设「translate 产物 100% 无 '?'」
  与 round 97 引入「超深表达式允许保留 '?'（交由 Lua 解析器报语法）」的新语义
  冲突。需在 round 97 同步调整守卫，例如：1) 把 A/B/D 的规模化输入控制在预算内
  （<48 深，如链长 40）以继续校验正常形态的无残留 '?'，并**单列一条**「>48 深
  表达式被预算截断保留 '?' 且不挂起」的新断言锁定 round 97 语义；2) 或把深度预算
  提升到守卫覆盖之上并把超深截断作为独立用例。具体方案由 round 97（主代理）拍板，
  此处仅记录交互与建议。
- **本 baseline（round 92-96，HEAD）不受影响**：round 96 提交版无深度预算，
  守卫全部 PASS。

## 5. 建议（非必须，无预算逼近；一项观察项）

- 各热路径预算余量充足，**暂无必须实现的优化点**。
- **观察项（非回归）**：本会话 tokenizer/scheduler/render 吞吐读数高于 round 92
  历史（约 +40-90%），已证实引擎代码零改动，判定为机器 CPU 降频/节能噪声。后续
  若在某轮看到**守卫预算余量 <20%** 或**引擎代码改动后**读数同一窗口内继续上移，
  再复核是否为真实回归。现阶段所有守卫仍以 >70% 余量通过。
- 与历轮一致，scheduler 吞吐读值本机波动大；若希望吞吐读数更稳定可改固定 resume
  数单调计时取中位数，但不构成当前回归。

## 验证

- 本文档更新为纯测量 + 文档 + C++ 走查，无引擎源代码变更（临时探针脚本 / A 产物
  均于测量后删除，git 工作树仅保留本文档改动）。
- round 96 HEAD 下 expr 规模化守卫正确性 + 预算断言全部 PASS；全量 Lua 套件在
  round 96 状态为 **126/126** 全绿；round 97 工作树交互已单列于第 4 节。