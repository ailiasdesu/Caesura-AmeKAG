# Performance Benchmarks — Caesura (AmeKAG)

> 产品化任务书 §16（Performance Benchmark）要求：建立长期 benchmark、**必须记录
> baseline**；**每次 PR 如果影响 hot path，必须有 benchmark 或性能解释。**
> 本文是基准的跑法、维度与政策入口；数字基线的权威记录在
> `docs/plans/2026-08-04-006-perf-baseline-update.md`（round 66 → 114 逐轮追加）。

## 一键入口

```bash
bash scripts/run_benchmarks.sh          # 5 个 Lua 性能套件（默认门禁集）
bash scripts/run_benchmarks.sh --web    # 追加 web 播放器 vitest 性能套件
```

脚本依次运行全部套件、逐套判定 PASS/FAIL（exit 0 且输出无 FAIL 行）并计时，
stdout 打摘要表，完整日志写入 `tmp/bench-latest.txt`；任一失败退出码为 1。

## 套件维度表

| 套件 | 维度 | 关键预算（CI-loose os.clock） |
|---|---|---|
| `tests/scripts/test_frame_bench.lua` | 每帧成本守卫：`layers.render()` 5000 次均值；round 68-72 混合表达式 translate 1000x；schema 迁移后 `[add]` 链派发 1000x | render <500us/帧；expr <2s；add <2s |
| `tests/scripts/test_scale_stress.lua` | 大型资产压测（round 101，20 断言）：4096² 图集 texel 记账；8 万次音频句柄 alloc/free（并发上限 128）；9600-token 大场景 parse/run；500 页 backlog 堆增长；3000 行（~400KB）translate | 记账 <1s；句柄 <2s；parse/run 各 <10s；堆 <4096KB；translate <10s |
| `tests/scripts/test_benchmark.lua` | 吞吐：2000 行 .ks tokenizer.parse + mock 调度器派发进度 | parse <3s；总计 <3s |
| `tests/scripts/test_bench_dispatch.lua` | 调度器热循环：2000 条编译后 `[ch]` 派发计数精确 + parse/compile/run 管线；分支/jump 流序 | 管线总计 <10s（tokens/sec 仅打印） |
| `tests/scripts/test_label_bench.lua` | 标签索引：1500 标签建索引 + 3000 次查找；索引查找 ≤ 线性扫描（真实比值 ~300x） | indexed ≤ linear + 0.01s |
| `web/perf-baseline.test.js`（`--web`） | web 播放器基线（round 109，wasmoon/jsdom）：story.ks 帧吞吐 / token 吞吐 / Lua 堆增长；合成 1000 行帧吞吐 / 堆；2000 vs 1000 行规模化线性 | >1.3 frames/ms；>0.15 tok/ms；<1024KB；>2.5 frames/ms；<2048KB；wall <2.5x |
| `web/perf-bundle.test.js`（`--web`） | ks_bake bundle 路径 vs 原始 .ks 源路径吞吐（tiny / 真实 story.ks / 1000+ 命令） | bundle ≥ 0.8x token 吞吐（≤20% 慢） |

## Baseline 记录位置

- **权威基线**：`docs/plans/2026-08-04-006-perf-baseline-update.md`。
  round 66-98 元素级守卫逐轮刷新；round 101-114（阶段 G）汇总表为当前权威采样。
- 关键读数（round 101-114 汇总，本机 os.clock / vitest，3 次采样中位）：
  - frame_bench render **~277us / <500us**（≈45% 余量，全基线最紧守卫）；
  - scale_stress：图集 3.0ms/<1s、句柄 66ms/<2s、parse 863ms/run 51ms/<10s、
    backlog 堆 **934.7KB/<4096KB**（77.2% 余量）、translate ~1.10s/<10s；
  - web：story 帧吞吐 **3.854**（>1.3）、token **0.489**（>0.15）、堆 **384.0KB**（<1024KB）、
    合成 1000 行 **7.427**（>2.5）、规模化比 **1.991x**（<2.5x）。
- 预算哲学：全部为**宽松 CI 安全预算**（实测反推留 ~2x 以上余量）；
  test_benchmark 的调度吞吐存在既有机型噪声区间（25-60ms），不作回归判定依据。
- 刷新流程：跑 `bash scripts/run_benchmarks.sh --web` → 把读数按轮次追加进 perf 文档
  （格式沿用既有「预算 / 实测 / 余量」表）。

## Hot-path PR 政策（任务书 §16）

以下文件属引擎热路径，**改动它们的 PR 必须附 benchmark 或性能解释**：

- `scripts/tokenizer.lua`、`scripts/kag/compiler.lua`、`scripts/scheduler.lua`
- `scripts/kag/expr.lua`（表达式 translate）
- `scripts/layers.lua`（每帧渲染遍历）
- `web/bridge.js`（web 播放器帧循环 / doString 驱动）

附法（二选一）：

1. PR 描述贴入 `bash scripts/run_benchmarks.sh --web` 的摘要表（或附 `tmp/bench-latest.txt`），
   读数不得劣化超过预算余量（参考 perf 文档历史区间）；
2. 书面性能解释：说明为何该改动不影响上述维度（如纯新增命令、冷路径、编译期一次性成本）。

预算本身需要调整时（新硬件/新语义），在 perf 文档追加一轮记录并说明理由，不得静默放宽。

## 在 CI 外复现

Lua 套件逐个运行（仓库根目录，vendored Lua 5.4）：

```bash
external/lua/lua.exe tests/scripts/test_frame_bench.lua
external/lua/lua.exe tests/scripts/test_scale_stress.lua
external/lua/lua.exe tests/scripts/test_benchmark.lua
external/lua/lua.exe tests/scripts/test_bench_dispatch.lua
external/lua/lua.exe tests/scripts/test_label_bench.lua
```

web 套件（**不在 CI 内**，与 story.bundle.sweep 同地位，本地运行）：

```bash
cd web && npx vitest run perf-baseline.test.js    # ~6s
cd web && npx vitest run perf-bundle.test.js
```

注意：

- 预算按 `os.clock` 墙钟判定，CI 虚拟机时钟更松，本机读数更紧是正常现象；
- 采样受机器负载影响，回归判定建议 3 次采样取中位（perf 文档口径）；
- Lua 套件为 headless 纯 Lua（backend/audio mock），不触碰 C++ registry，可独立运行不污染套件。
