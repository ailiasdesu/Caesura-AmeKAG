# 引擎性能基线（round 25）

> CPU 热路径基准（tests/cpp/test_perf_bench.cpp，纯 CPU 无 GPU，CI 全平台可跑）。
> 断言用宽松上限防 flaky；**打印的数字才是回归信号**——对比历史轮次数字即可发现退化。

## 1. 基准结果（本机 Debug，2026-08-14）

| 热路径 | 数值 | 说明 |
|---|---|---|
| Lua string.format + table append ×10k | **22.3 ms**（≈450 次/ms） | KAG 文本/backlog 每行格式化；上限 800ms（10× 余量） |
| Lua 嵌套表字段读取 ×10k | **0.56 ms** | KAG 变量读取热路径；上限 400ms |
| CPU 软蒙皮 8k 顶点（SmaSkinner） | **0.78 ms/帧** | GPU compute 路径参照（D3D11 实测 ~0.08ms，≈**9.8×**） |

## 2. 既有基准（对照）

- GPU 蒙皮主机侧成本：CPU 1.27ms vs GPU 0.08ms（≈15.8×，含上传/提交，test_render_integration）
- Lua 侧 scheduler/tokenizer 吞吐：tests/scripts/test_benchmark.lua
- 3D 碰撞：tests/cpp/test_mini_collision.cpp（large sparse set perf guard）

## 3. 观察

- 文本格式化（22.3ms/10k）是 VN 热路径：每行对话约 1-2 次 format → 单行 <0.01ms，量级安全；若未来出现大 backlog 渲染可先看这里。
- 表读取极快（0.56ms/10k）——KAG 变量访问不是瓶颈。
- CPU 软蒙皮 0.78ms/帧（纯变形）在 4k 顶点角色下 <0.4ms，移动端可接受；GPU compute 仅主机侧就快 ~10×，同屏多角色时优先 GPU。

## 4. 用法

```
cd build/tests/Debug
./CaesuraTests.exe -tc="Perf:*" -s     # 打印基准数字
./CaesuraTests.exe                     # 全量（含 perf guard 断言）
```
