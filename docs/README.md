# Caesura (AmeKAG) 文档导航 — 按用户任务查找

> 本页是**任务中心化导航层**：以「你想完成什么」组织文档入口，而不是按源码模块组织（产品化总任务书 §13）。它只做导航，不搬移任何文件。
> 按类型划分的权威文档分类见 [AGENTS.md §12](../AGENTS.md)；本页与其冲突时以 AGENTS.md 为准。
>
> 状态标签（诚实分级，口径同任务书 §18.4）：**已验证** = 仓库内有证据可复现（测试/门禁/随包验证）；**可用** = 文档与工具完备、日常可用；**experimental** = 能力存在但未达产品级；**需外部资源** = 需要账号或手动下载的 SDK 才能走完全程。

---

## 七个优先任务路径（产品化总任务书 §13 优先级清单）

### 1. 5 分钟运行第一个 VN

- **任务**：从拿到仓库到亲眼看到一段 VN 剧情跑起来。
- **按序阅读**：
  1. [guides/getting-started.md](guides/getting-started.md) — 逐平台环境准备 → 构建 → 运行；§9 Smoke Checklist 含「Demo 运行」步骤
  2. [guides/community.md](guides/community.md) — 学习路径总览：跑通引擎 → 16 个教程 → 完整示例游戏 example_game
  3. [guides/sample-library.md](guides/sample-library.md) — showcase 与全部示例的运行方式（桌面引擎 / Web 播放器）
- **预计耗时**：目标 ≤5 分钟（引擎已构建的前提下）；首次克隆（约 1–2 GB）+ 全量构建需 15–40 分钟，为一次性成本。
- **当前状态**：已验证（v1.0.x 发布全链路终验 + 示例游戏双端验证）。

### 2. 30 分钟完成一个小游戏

- **任务**：用官方模板从零做出一个含对话、选择、存档的可玩小短篇。
- **按序阅读**：
  1. [guides/template-quickstart.md](guides/template-quickstart.md) — 五步法：拷贝模板 → 写场景 → 跑起来并校验 → 打包 → 发布
  2. [guides/kag-language-tour.md](guides/kag-language-tour.md) — KAG Neo-Genesis 语言教程（对话/画面/音频/流程/变量）
  3. [guides/sample-library.md](guides/sample-library.md) — 16 个递进式教程示例（每个独立可运行、注释逐行讲解）
- **预计耗时**：30–60 分钟（含一次打包试玩）。
- **当前状态**：已验证（模板 + 16 个教程均经编译与 Web 播放器双重校验）。

### 3. 如何做选择分支（`[select]` / `[sel]` / `[button cond]`）

- **任务**：给剧情加选项跳转、条件按钮或多路分支结局。
- **按序阅读**：
  1. [guides/kag-language-tour.md](guides/kag-language-tour.md) — §7 选择分支（速览语法与完整示例）
  2. [api/command-contracts.md](api/command-contracts.md) — `[select]`（`[sel]` 别名）、`[button]` 的声明式参数契约（123 命令权威参考）
  3. [api/kag-expression-language.md](api/kag-expression-language.md) — 条件表达式：`[if]` / `[switch exp=...]` / `${}` 插值
- **可运行样例**：[demo/tutorial/tutorial_05_branching.ks](../demo/tutorial/tutorial_05_branching.ks)、[demo/tutorial/tutorial_11_switch.ks](../demo/tutorial/tutorial_11_switch.ks)
- **预计耗时**：20 分钟。
- **当前状态**：已验证（契约测试 + 教程双端校验覆盖）。

### 4. 如何做 Live2D

- **任务**：接入 Cubism SDK，让角色立绘动起来。
- **按序阅读**：
  1. [guides/live2d-setup.md](guides/live2d-setup.md) — SDK 下载/放置/CMake 启用（`CAESURA_LIVE2D=ON`）与无 SDK 时的 PNG 序列回退
- **预计耗时**：1–2 小时（不含 Cubism SDK 官网下载与授权确认）。
- **当前状态**：需外部资源（Cubism SDK 须手动下载，官方模型行为无法在 CI 复现；PNG 回退路径已验证）。

### 5. 如何做多语言（i18n）

- **任务**：让同一份剧本支持中/英/日等多语言，并保持翻译不缺键。
- **按序阅读**：
  1. [guides/i18n.md](guides/i18n.md) — 工作机制 → 模板生成/增量合并 → 缺译与键引用清单（CI 门禁）→ 运行时切换
- **可运行样例**：教程 14 演示 i18n 运行时热切换（见 [guides/sample-library.md](guides/sample-library.md) 教程表）。
- **预计耗时**：30 分钟上手，翻译工作量另计。
- **当前状态**：已验证（CI 门禁双向校验 + 示例游戏 zh/en/ja 三语审计）。

### 6. 如何发布 Steam

- **任务**：把做完的游戏通过 Steamworks 发上 Steam 商店。
- **按序阅读**：
  1. [guides/steam-release.md](guides/steam-release.md) — Steam 发布链指南（AppID → Depot 布局 → steamcmd 上传 → 分支验证；逐步标注 ✅ 已验证 / ⏳ 待账号）
  2. [design/engine-capability-matrix.md](design/engine-capability-matrix.md) — C8 行：Steamworks 抽象（成就/统计/云存档 Lua 面）现状与边界
  3. [plans/audit/g0_steam.md](plans/audit/g0_steam.md) — steam 模块历史审计（已知问题与修复记录）
- **诚实结论**：引擎抽象就绪（Null 后端可测），但**发布链需要 Steamworks 合作伙伴账号 + 付费 AppID**，上传/成就/覆盖层从未真实执行过。
- **预计耗时**：工程侧半天；账号审批与商店流程以 Valve 为准（数天–数周）。
- **当前状态**：需外部资源。

### 7. 如何从 KAG3 迁移

- **任务**：把既有 KAG3/KiriKiri 作品（.xp3 包 + .ks 脚本 + tlg 图）迁到 Caesura。
- **按序阅读**：
  1. [guides/kag3-migration.md](guides/kag3-migration.md) — 四步迁移总览：xp3 解包 → tlg 转 png → 音频映射 → .ks 转换
  2. [guides/kag3-import.md](guides/kag3-import.md) — kag3_import 工具用法（check/convert/strict 三模式，strict 可作 CI 门禁）
  3. [guides/xp3-compat.md](guides/xp3-compat.md) 与 [guides/tlg-compat.md](guides/tlg-compat.md) — 两个格式的支持范围与明确不支持清单
  4. [compatibility.md](compatibility.md) — §2 KAG3 兼容范围（明确兼容 / 明确不兼容，不假装支持）
- **预计耗时**：小型作品半天；大型作品取决于素材量与自定义 TJS 逻辑占比。
- **当前状态**：可用（导入器与两个格式解码器均有测试；整项目迁移结果依赖原作素材复杂度）。

---

## 附加任务

### 打包分发（CARC / Web / package_game）

[guides/packaging-ux.md](guides/packaging-ux.md) 一键打包总览 → [guides/carc-packaging.md](guides/carc-packaging.md) CARC 加密归档 → [scripts/package_game.sh](../scripts/package_game.sh) 把 .ks 游戏打成自包含 Web 静态站（可部署 GitHub Pages / itch.io）→ [guides/release-process.md](guides/release-process.md) 引擎本体发布与门禁 → [guides/sample-game-release.md](guides/sample-game-release.md) 游戏侧两条发布路径（GitHub Releases / itch.io）。**状态**：可用（Windows ZIP 经 r114 全链路终验；Web 包脚本实测产出静态站）。

### 调试（断点 / LSP）

[api/editor-api-reference.md](api/editor-api-reference.md)：HTTP `/api/debug/setBreakpoint` 等断点端点、stdin JSON-RPC 的 `kagSetBreakpoint` KAG 场景层调试、附录 C 的 LSP 语言服务方法表（completion/hover/diagnostics，经 `/api/eval` 桥接）。**状态**：可用（headless smoke 测试覆盖 stdio 调试命令）。

### 存档与迁移

[compatibility.md](compatibility.md) §4 Save compatibility（存档格式 / schema 迁移链 / Golden Save 跨版本承诺 / 字段只增不删）+ [design/save-security-audit.md](design/save-security-audit.md) 存档加密与防篡改审计。**状态**：已验证（AES 加密存档 + schema 迁移测试）。

### mod 制作

[compatibility.md](compatibility.md) §5.4 定义了项目内 `mods/` 目录约定（加载语义见 §5 Project compatibility）。**状态**：experimental —— 目前只有目录约定，尚无端到端的 mod 制作指南与示例。

### 性能基准（run_benchmarks.sh）

[guides/performance-benchmarks.md](guides/performance-benchmarks.md) 套件维度表与热路径 PR 政策 → [scripts/run_benchmarks.sh](../scripts/run_benchmarks.sh) 一键跑全部基准 → [design/engine-performance-baseline.md](design/engine-performance-baseline.md) 基线数字与解读。**状态**：可用（触碰 tokenizer/compiler/scheduler/render/web loop 的 PR 必须附基准结果或书面说明）。

---

## 文档地图速览

| 目录 | 内容 |
|---|---|
| [api/](api/) | API 参考命令契约（权威，自动生成）、Lua 模块、C++ 接口、编辑器 API、API 普查、表达式语言；`kag-commands.md` 已弃用 |
| [design/](design/) | 架构拓扑、能力矩阵、安全机制、KAG Neo-Genesis 语言标准、BackendRegistry 依赖指南、性能基线、市场分析快照 |
| [guides/](guides/) | 用户与开发者指南：入门、模板、教程库、i18n、Live2D、CARC/一键打包、KAG3 迁移、跨平台与各平台构建（分主题索引见 [guides/README-index.md](guides/README-index.md)） |
| [plans/](plans/) | 执行记录与计划：日期命名的交接文档（最新 023 为权威现状）、[plans/audit/](plans/audit/) 下的 ROADMAP-200 与产品化总任务书 |
| [solutions/](solutions/) | 过往问题解法（architecture-patterns / build-errors / runtime-crashes / kag-language 四类 + 无 GPU 测试清单），YAML frontmatter 可检索 |

`docs/brainstorms/` 仅保留被 plans/ 引用的历史需求文档。

---

维护约定：新文档先按 AGENTS.md §12 规则归类落盘，再回到本页对应任务路径补一条链接；发现失效链接请顺手修复。