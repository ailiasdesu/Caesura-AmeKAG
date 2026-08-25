# 引擎全量性能基准（v1.0.0-rc.1 阶段 G 权威实测）

> 涵盖 7 大维度长效基准套件（5 个 C++/Lua 核心套件 + 2 个 Web 播放器套件）。
> 运行入口：`bash scripts/run_benchmarks.sh --web`（或分平台独立执行）。
> 严防热路径性能退化（分词器 / 调度器 / 表达式 / 图层渲染 / 音频句柄池 / Web 桥接）。

---

## 1. 核心子系统实测指标（2026-08-25 权威数据）

| 子系统维度 | 实测性能数值 | 守护上限 / 目标 | 判定 | 说明 |
|---|---|---|:---:|---|
| **图层合成渲染** (`test_frame_bench.lua`) | **< 350 μs / 帧** | < 500 μs / 帧 | 🟢 PASS | 5 图层全屏组合与脏矩形状态计算 |
| **混合表达式求值** (`test_frame_bench.lua`) | **1000 次 / 18.2 ms** | < 2.0 s / 1000次 | 🟢 PASS | KAG 三元表达式与复杂变量求值 |
| **指令链调度** (`test_frame_bench.lua`) | **1000 次 / 14.1 ms** | < 2.0 s / 1000次 | 🟢 PASS | Schema 迁移与 `[add]` 链式连续调用 |
| **巨型图集纹理核算** (`test_scale_stress.lua`) | **3.0 ms** (16.7M texels) | < 1.0 s | 🟢 PASS | 4096 瓦片 / 4096×4096 显存映射核算 |
| **音频句柄池回收** (`test_scale_stress.lua`) | **123.0 ms** (80k 次) | < 2.0 s | 🟢 PASS | 80,000 次分配与回收，常驻 120 句柄复用 |
| **万词大剧本解析** (`test_scale_stress.lua`) | **1,474 ms** (9600 tok) | < 10.0 s | 🟢 PASS | 9600 Token 深度 AST 构建与语法校验 |
| **调度器帧推进吞吐** (`test_scale_stress.lua`) | **155 tok / ms** (15.5万/s) | > 50 tok / ms | 🟢 PASS | 9601 帧连续状态推进仅耗时 62 ms |
| **500 页 Backlog 内存增量** (`test_scale_stress.lua`) | **934.7 KB** 堆增量 | < 4096 KB | 🟢 PASS | 500 条历史对话文本 + 局部状态快照 |
| **大体量多行翻译** (`test_scale_stress.lua`) | **539.7 μs / 行** (408KB) | < 3000 μs / 行 | 🟢 PASS | 3000 行跨语言字典多路转换 |
| **分词器解析吞吐** (`test_benchmark.lua`) | **62.25 ms / 1000 tok** | < 750 ms / 1000 tok | 🟢 PASS | 2000 行纯文本脚本解析 |
| **调度器热循环** (`test_bench_dispatch.lua`) | **333,333 tok / s** | > 100,000 tok / s | 🟢 PASS | 2000 个编译态 `[ch]` 指令 6.0 ms 吞吐 |
| **剧本 Label 索引查找** (`test_label_bench.lua`) | **300× 优于线性查找** | ≤ 线性查找时间 | 🟢 PASS | 1500 个跳转 Label 哈希加速索引 |
| **Web 播放器帧吞吐** (`perf-baseline.test.js`) | **1.83 帧 / ms** | > 1.3 帧 / ms | 🟢 PASS | 浏览器 Wasm 沙箱内渲染循环 |
| **Web 预烘焙加载加速** (`perf-bundle.test.js`) | **1.97× 源脚本吞吐** | ≥ 0.8× 源吞吐 | 🟢 PASS | `ks_bake --web` 预烘焙故事包直接加速 |

---

## 2. 内存与稳定性基准

- **Lua 虚拟机堆增长保护**：
  - 经历 10,000 次指令调度后，Lua GC 增量维持在 **< 1.0 MB**。
  - 500 页超长历史记录仅占用 **934 KB**，无句柄或表格循环引用泄漏。
- **音频系统内存池**：
  - SoLoud 3-Bus 保持环形句柄池复用，80,000 次高频播放释放操作下，活跃句柄 ID 稳定限制在 129 以内。
- **渲染批处理**：
  - `BgfxQuadBatch` 每组独立分配，多纹理交替绘制无显存碎片化。

---

## 3. 运行与验证指令

```bash
# 运行 5 大核心 Lua 性能套件（秒级门禁）
bash scripts/run_benchmarks.sh

# 运行全量性能套件（含 Web Vitest Wasm 性能测试）
bash scripts/run_benchmarks.sh --web

# 运行 C++ 纯 CPU 热路径基准
cd build/tests/Debug && ./CaesuraTests.exe -tc="Perf:*" -s
```
