# Caesura (AmeKAG) — 交接文档（2026-08-12 第 6 轮迭代）

> 面向后续 agent 的完整上下文。本轮延续"代差路线图"
> （`docs/plans/2026-08-12-004-generation-gap-roadmap.md`），覆盖
> bug 修复 / 性能优化 / 功能增加 / 代码补强 / 调试验证五类。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交，自 a0980988 起）

### Phase 1 — Bug 修复：Electron 偶发主线程停滞（4a 已知风险闭合）

| 提交 | 内容 |
|---|---|
| `fix(rpc)` | 有界 dispatch 等待：`RpcReplyStatus::Busy` 新增、HTTP 503 busy 映射、`EngineRpcDispatcher::dispatch` 无限 `wait()` → `wait_for`（5s 默认，`CAESURA_RPC_DISPATCH_TIMEOUT_MS` 可调）+ `CAESURA_TEST_STALL_MS` 诊断停滞钩子（pump 入口） |

- **根因**：驱动级 present 挂起（虚拟显示/串流环境，历史 3/3 复现、本次 0/1）→ 主线程停滞 → dispatch Pending 无限等待 → HTTP worker 堆积 → 连接拒绝（code=000）。渲染路径唯一阻塞调用为 `bgfx::frame()`；管道理论经 main.cjs 检视排除。
- **验证**：停滞模拟（TEST_STALL=10000 + TIMEOUT=800）→ `/api/status` 0.855s 返回 503 engine_busy、连发 3 次零堆积；正常路径 5.8ms 无退化；Electron 5.3 分钟稳定性验收（15933 帧）+ 2.5 分钟回归复测全 200。

### Phase 2 — 性能优化：表达式 AOT 扩展

| 提交 | 内容 |
|---|---|
| `perf(kag)` | `[button cond]` 编译期 AOT：EXPR_TOKENS["button"]="cond"、pass-3 条件扩展、endbutton 求值分流（TJS 运算符 `&|!?` 走运行时回退，否则 evaluateTranslated）——微基准 **7.06×**（20000 次 0.120s→0.017s） |

- **schema 探查结论**：coerceValue 无 exp 类型（number/boolean/string/list/enum/file），全部表达式参数（流命令 + button cond）已 AOT，运行时 translate 残留为零——覆盖完整，无可保守扩展。
- **基准门禁**：stash 对照证实 tokenizer/scheduler 数值为机器状态方差（52~55.5ms/19~22ms 与改动前一致），非回归。

### Phase 3 — 功能增加：脚本表达力 + IDE 能力

| 提交 | 内容 |
|---|---|
| `feat(kag)` | 内联文本标记：`kag/text_layout.lua` parse_markup（`{color=#RRGGBB}` 嵌套栈、`{b}/{i}/{size}` 消费为无操作、未知标签字面、游离闭合消费）+ wrap_spans（span 字符化 + 按色分组跨行）；`text_scene.lua` add_wrapped_spans（逐段 draw、x 按段宽推进、打字机揭示顺序正确）；`[ch]`/`[text]` 接入（backlog/字幕/揭示计数用剥离后纯文本）；教程 §13；test_text_markup 20 断言 |
| `feat(lsp)` | LSP 导航：`lsp.definition`（光标处 token：label 自身 / jump/call/link 目标解析到定义行；跨场景目标 name-only）+ `lsp.references`（定义 + 全部导航点）；Monaco registerDefinitionProvider/registerReferenceProvider；test_lsp +12 断言（36/36） |

### Phase 4 — 代码补强：SMA 动画 + 模糊测试 + 安全复扫

| 提交 | 内容 |
|---|---|
| `feat(render)` | **SMA S2**：`SmaSkinner.h`（纯数学：applyBonePose + skinMesh 双骨骼权重混合归一化）+ `SmaMeshRenderer`（bgfx 实现：transient VB 像素→NDC、posTex layout + fsTexture 复用零新 shader、deferred-GPU 惰性 init、构造/析构移出解决 unique_ptr 不完整类型）；Engine GPU 模式接线（headless 用 Null） |
| `feat(script)` | **SMA S3**：`SmaBinding`（Lua `sma.*` 6 API 经 BackendRegistry）+ `kag/sma.lua` 驱动（JSON 解析、层级世界变换[枢轴烘焙]、轨道 LERP、actor 生命周期、惰性 binding 解析）+ `[sma_play]`/`[sma_stop]` 契约与注册 + sandbox 白名单 `sma=true` + init.lua 预载；test_sma 25 断言 |
| `test(scripts)` | fuzz 扩展：expr.translate 300 随机畸形 TJS 表达式——零抛出零崩溃（test_fuzz +3 断言） |
| `fix(kag)` | 安全复扫：eval 桥面三构造点全转义（圆 5 修复后无新残留）；沙箱四层防线（require 白名单/io 只读白名单/os 受限/_G 写保护）审计通过 |

### Phase 5 — 调试验证与收尾

- **Ollama 端到端**：本机无 Ollama 服务（11434 连接拒绝）——**如实记录为待用户环境**；mock 覆盖与降级路径已测。
- **Electron 回归**：2.5 分钟+ 稳定、全链路端点 200、LSP 导航实测生效（`{"line":1,"name":"start","col":1}`）。
- **全量门禁**：重建零错误、Lua **116/116**、C++ **617/617**（3002 断言）、ctest **10/10**、耦合 **PASS**、benchmark 套件 PASS、diff-check 干净。

## 2. 架构要点（本轮变化）

- **RPC 有界等待**：HTTP worker 不再无限阻塞——主线程停滞时 5s 内返回 `engine_busy`（503），IDE 可显示错误并重启引擎（驱动级挂起的实际恢复路径）。诊断钩子 `CAESURA_TEST_STALL_MS` 为测试专用（pump 入口 sleep）。
- **表达式 AOT 全覆盖**：`FLOW[cmd] or EXPR_TOKENS[cmd]` 双条件——非流命令（button）也走编译期翻译；endbutton 按 TJS 运算符检测分流（编译流直通 evaluateTranslated，手建流运行时翻译回退）。
- **文本渲染**：draws 逐项颜色机制复用实现内联着色（零 C++ 改动）；markup 解析在布局层（text_layout），换行时按色分组跨行。
- **SMA**：`BonePose` = 驱动侧已解析世界变换（S1 契约），层级/枢轴烘焙在 `kag/sma.lua`（Lua 纯数学），权重混合在 C++（SmaSkinner）；绘制用 transient VB + 既有程序，无新 shader；无 GPU 环境全链惰性空操作。
- **LSP 导航**：definition/references 纯函数 + JSON 桥；跨场景目标返回 `{name}`（无 line）→ Monaco 显示无位置。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| SMA S4 完整化（场景级确定性测试）/ S5 GPU 蒙皮 | 无 / 可选 | S4 数学用例已随 S2 交付；场景级 + 蒙皮 shader 待后续 |
| SMA 游戏循环接驳（engine_update/render 调 sma.update/render） | 无 | 入口脚本集成点已文档化（kag/sma.lua update/render） |
| 真实 Ollama 端到端 | 用户环境 | 本机无服务；安装并启动后可直接验证（mock 已全覆盖） |
| P1-6 Live2D GL/Steam | 硬件 | GL 需 Linux/macOS；Steam 需开发账号 |
| P0-1 Metal | 硬件 | macOS 实机 |
| P0-3 移动真机验证 | 设备 | Android 构建脚本已交付；APK 真机验证待设备 |
| 内联文本标记 b/i/size 视觉化 | 渲染器 | 需 FreeType 粗体/斜体/字号变体（当前消费为无操作，文档化限制） |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests ≥617 → Lua ≥116 → ctest ≥10/10 → 耦合 PASS →
benchmark 无退化（tokenizer ≤52ms/1000tok、scheduler ≥308k tok/s 为机器状态
参考值——stash 对照证实非代码回归；表达式 ≤165ms/400-if）。

## 5. 注意事项

- **RPC 超时环境变量**：`CAESURA_RPC_DISPATCH_TIMEOUT_MS`（默认 5000）与
  `CAESURA_TEST_STALL_MS`（诊断专用）——后者绝不可在生产环境设置。
- **Lua 套件顺序敏感**：写文件的测试必须在 `test_sandbox` 前；test_fuzz
  （含新 expr 模糊段）与 test_sma 均在 sandbox 前。
- **SMA 测试**：C++ skinner 数学为确定性用例；SmaMeshRenderer 走
  deferred-gpu 模式（无 GPU 环境惰性空操作）；Lua 侧 binding 用
  `_G.sma` 惰性解析 + 记录 mock。
- **CMake**：SmaMeshRenderer.cpp（Render 模块）与 SmaBinding.cpp（Script
  模块）已注册；新增源文件需同步 `cmake/CaesuraModules.cmake`。
- **编辑器前端**：AiPanel/kagLsp 的 eval 桥必须经 `luaString`/`luaValue`
  转义（安全复扫结论）；新 LSP 端点 definition/references 已接 Monaco。
- 历史交接：`2026-08-12-005-delivery-handoff.md` 为上一权威状态。
