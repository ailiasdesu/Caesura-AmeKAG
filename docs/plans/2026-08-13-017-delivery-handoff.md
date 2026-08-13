# Caesura (AmeKAG) — 交接文档（2026-08-13 第 17 轮迭代）

> 面向后续 agent 的完整上下文。本轮为**真实验证收官轮**：
> AI 管线首次对**真实 Ollama** 端到端验证（本机已装 Ollama + gemma3:4b），
> 并修复了真实验证暴露的两个现实坑（默认模型 404、冷加载超时）。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交）

| 提交 | 内容 |
|---|---|
| `feat(script)` | **AIBinding 现实修复**：① 模型自动发现——Ollama 端点且 model 为空时 GET /api/tags 取首个可用模型（进程内缓存 + 互斥锁，失败回退 llama3），消除"默认 llama3 必然 404"（本机只有 gemma3:4b）；② 默认读超时 15000→60000ms（冷模型加载普遍超 15s；[ai_dialog] 的 max_wait_ms=15000 游戏内快降级语义不变，HTTP 层放宽给 IDE/aiwriter）；config.lua 注释同步 |
| `test(script)` | **条件式真实回归**（test_ai.cpp +1 用例，619→620/3028→3031）：探测 127.0.0.1:11434/api/tags（2s 超时），不可达→MESSAGE+return（test_audio 模式，CI 干净）；可达→空 model 走自动发现→真实 query→断言非空回复。本机实测通过 |
| `smoke+docs` | **headless_ai_smoke.py**（ctest 第 11 项 CaesuraHeadlessAiSmoke，TIMEOUT 300，SKIP_RETURN_CODE 77）：真实引擎 --headless + 真实 Ollama；同步 AI.query（自动发现）+ 异步 AI.query_async（回调经 pollMainThreadJobs，`package.loaded` 做沙箱安全结果通道）均断言真实回复；无 Ollama 退出 77 干净跳过。矩阵 S2n 更新、tour §18（AI 辅助章节）、api-stats 620/3031、交接 017 |

## 2. 架构要点（本轮变化）

- **模型发现**：`discoverOllamaModel(host, port)` 静态缓存（std::mutex 保护），
  doQuery 线程安全（async 在 JobSystem worker 上调用）；发现失败才回退 llama3。
- **沙箱协作**：RPC eval 在 sandbox 内运行——新建全局被拒（`_AI_SMOKE` 报
  "Sandbox: cannot create global"），异步结果通道用既有可写表 `package.loaded`；
  这同时证明沙箱与 AI.query_async 的 callback 机制兼容。
- **真实验证证据**（2026-08-13，gemma3:4b）：
  `AI.query('Reply with the single word: hello')` → "hello"（同步）；
  `AI.query_async` 回调 → "hello\n"（异步）。ctest 11/11 含 AiSmoke 2.02s。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| P1-6 Live2D GL/Steam、P0-1 Metal、P0-3 移动真机 | 硬件 | 见 009/010 |
| SMA S5 GPU 蒙皮 | 可选 | CPU 软变形已够用 |

> 真实 Ollama 端到端已闭环（本轮）。mock-only 项清零；路线图五大战役 + 表达力全部完成。

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 620/620（3031 断言）→ Lua 118/118
→ ctest 11/11（无 Ollama 环境 AiSmoke 以退出码 77 跳过，仍绿）
→ 耦合 PASS → benchmark 无退化（AI 路径不涉性能热点）。

## 5. 注意事项

- **本机环境**：Ollama 已安装（C:\Program Files\Ollama），服务由桌面应用
  （ollama app）常驻 127.0.0.1:11434；已拉取模型 gemma3:4b（3.3GB）。
  `ollama serve` 手启会因端口占用失败——app 已托管，无需干预。
  冷启动后首次推理慢（数十秒）：先 `ollama run gemma3:4b` 或任一短 generate 预热。
- **模型发现缓存**：进程内一次性（首次查询时）；运行中 pull/remove 模型不会刷新，
  重启引擎生效。显式传 model 时不做发现。
- **smoke 通道**：沙箱禁新建全局——异步结果经 `package.loaded._ai_smoke` 传递；
  若未来 sandbox 收紧 package 写入需同步更新 headless_ai_smoke.py。
- **超时分层**：config.ai.timeout_ms（HTTP 读，60s）与 [ai_dialog] max_wait_ms
  （游戏内等待，15s）独立——前者给 IDE/aiwriter 冷加载余量，后者保游戏快降级。
- 历史交接：`2026-08-13-016-delivery-handoff.md` 为上一权威状态。