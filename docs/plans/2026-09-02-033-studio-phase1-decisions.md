# Caesura Studio Phase 1 — 决策记录（2026-09-02 · 033）

> 上游：`2026-09-02-031-studio-phase1-kickoff.md`（9 个开放决策位）→ `2026-09-02-032-studio-phase1-handoff.md` §3（待拍板清单）→ **本文件（决策定稿）**。
> 拍板方式：用户 2026-09-02 口令——「决策位 #2–#4、#6–#9 由 Claude Code 裁决；Web Package CI 硬门取 A+C；web-editor 与 editor 不合并」。
> 本文档只记录决策与依据，不描述实现；实现记录见各执行批的 commit body 与 `docs/solutions/`。
> 所有 file:line 均为写入时刻实读（HEAD `1a25fd0c`）。

## 1. 草案 031 决策位定稿

| # | 决策位 | 定稿 | 依据（实况） |
|---|---|---|---|
| 1 | 架构取向 | **A：editor/ 分发接线**（已执行完成，f0470234 / 69b932d0 / CI r21–r24 全绿） | 032 §2 |
| 2 | Inspector / Console 是否入 MVP | **纳入 MVP**。Inspector 归 R-SP-5，Console 归 R-BM-4；二者只补验收判据，不新增实现范围 | `editor/src/ide/InspectorView.tsx`（含 `InspectorView.test.tsx`）与 `editor/src/ide/OutputPanel.tsx`（含 `OutputPanel.test.tsx`）均已在场；纳入的边际成本为零，剔除反而让 R-SP-5 / R-BM-4 无落点 |
| 3 | R-AB-7 引用关系 / R-AB-8 拖入 | **明确后置到 Phase 1.5**（M2 出口后评估） | 任务书 §6.4:333-334 原文分别标注「后续」/「第一版可以后做」；029 §一 Feature 冻结纪律；SA 盘点无引用图资产 |
| 4 | Scene Preview 形态 | **MVP 取嵌入式画布（帧捕获）**；引擎独立窗口作为后续「弹出」选项，不进 Phase 1 | `src/rpc/EditorServer.cpp:1122` 已有 `GET /api/debug/getFrame`（base64 PNG）且 `editor/src/lib/rpc.ts` 已封装；`--editor` 模式本就是隐藏 GPU 窗口；嵌入式与任务书 §6.2 布局图一致，零新增引擎面 |
| 5 | 模板展示名映射 | **关闭：name 即展示名**（manifest 无 displayName） | 032 §2「五模板 e2e」结论；任务书 §6.3:316-320 |
| 6 | 029 → Studio 衔接注记 | **加注**：在 029 末尾追加一段软衔接（指向 031/032/033），不改 029 任何判据 | 029 全文零处提及 Studio（grep 实证）；加注成本一行，避免下一位读者从 029 找不到当前优先级 |
| 7 | Studio MVP 目标机 | **Windows 优先**：Electron 壳与桌面打包 Phase 1 只做 Windows；引擎侧与 editor/dist 的三平台 CI 构建保持不变 | 任务书 Phase 2 才要求跨平台打包；`editor/electron/main.cjs` 无任何平台分支、`editor/package.json` 无 electron-builder 配置——三平台同期会把「打包配置」缺口乘三；ci.yml 三个 Package job 已各含 Build editor IDE 步（:342/:415/:490），引擎侧跨平台证据不受影响 |
| 8 | Script Editor 文件管理归属 | **沿用现状**：Explorer（AB）是唯一文件树，EditorArea（SE）只管 tab；R-SE-11 落点 = EditorArea tab 管理，不在 SE 内再建树 | `editor/src/ide/ExplorerView.tsx:78-91` `openScript/openImage → openDoc`；`editor/src/ide/EditorArea.tsx:117` tab-bar、`:141` 空态文案 "Open a script from the Explorer"——代码已经是这个设计 |
| 9 | 标准 LSP / DAP 期望 | **Phase 1 不要求标准协议**：自研 `scripts/kag/lsp.lua` + `editor/src/lib/kagLsp.ts` 与 `DebugProtocol` + `kag_debug.lua` 即为 Studio 内嵌编辑器的正式面；候选 C 降级为 Phase 2+ 可选项（触发条件：出现第三方编辑器接入需求） | 任务书 §6.5:338「利用现有 LSP」的产品目标是编辑器功能闭环而非协议互操作；引入 LSP wire / DAP 适配器会在 M2 关键路径上加一层双轨协调（031 候选 C 弊项） |

## 2. 常设三项

| 项 | 定稿 | 状态 |
|---|---|---|
| ① Web Package CI 硬门 | **A + C**（t144 简报 `build/t144-web-package-ci-hard-gate-brief.md` §B/§C）：A = Linux CI job 在 vitest 前 bake → vite build → `package_game.sh --no-web-build tests/projects/first_vn` → 新 `scripts/verify_web_package.sh` 文件断言；C = `deploy-web.yml` 发布前对打包产物跑 `web_browser_smoke.mjs`。前置修复：`scripts/web_browser_smoke.mjs:77-88` `CHROME_PATHS` 只有 Windows 路径，须支持 Linux/macOS 与 `CHROME_BIN`；deploy-web.yml:45 Node 20 → 22（全局 WebSocket 不再需要 flag） | **本批执行** |
| ② A 类 16 条流程控制命令注册 | 未拍板 | 不开工 |
| ③ Showcase D1–D5（030 §7.1） | 未拍板 | 不开工 |

## 3. 两套前端的产品定位（用户拍板：不合并）

| 产物 | 位置 | 定位 |
|---|---|---|
| **Caesura Web Editor** | `web-editor/dist/index.html`（11KB 单文件；随发布包安装，`CMakeLists.txt:545`；EditorServer 默认静态根） | Studio 生态下的 **Web 端编辑器/调试面板**：零依赖、随引擎包走、浏览器直开 `http://127.0.0.1:9876/` |
| **Caesura IDE** | `editor/`（React/Monaco 五视图；`editor/dist` 随发布包安装于 `editor/dist/`，`CMakeLists.txt:546-553`；经 `CAESURA_EDITOR_WEBROOT` 覆盖静态根或 Electron 壳加载） | Studio 生态下的 **桌面 IDE** |

约束：二者共用同一 EditorServer HTTP RPC（36 条路由，/api/* 34 条）与同一 token 门控；不得为任一方新增平行 RPC 入口（任务书 §18.1 禁止平行架构）。文档中一律用上表名称，不再写「web-editor vs editor 待合并」。

## 4. 对后续切片的影响

- M1（PM + Template）入口条件（031「切片 M1」）：#1、#3/#6 已定 → 入口打开；Electron 打包配置按 #7 只做 Windows。
- M2（AB + SE）：#8 使 R-SE-11 无新增工作；#9 使 M2 关键路径不含协议层。
- M3（SP + Debugger）：#4 使 R-SP-1/2 走 `sceneRun` + `getFrame` 现成面。
- M4（BM）：#2 使 R-BM-4 直接由 OutputPanel 承接。
- 执行顺序：常设项 ① 先行（本批），随后 M1。
