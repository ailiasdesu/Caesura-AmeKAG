# Caesura 社区与入门（Community）

> 本文档面向**刚接触 Caesura (AmeKAG)** 的新来者——无论是想用引擎创作
> 视觉小说的作者，还是想深入引擎源码的开发者。告诉你：在哪讨论、从哪学起、
> 如何参与、如何发布你的作品。
>
> 相关入口：[README](../../README.md) · [贡献指南](../../CONTRIBUTING.md) ·
> [入门指南](getting-started.md) · [示例库](sample-library.md)

---

## 1. 什么是 Caesura (AmeKAG)

Caesura 是一个开源的跨平台视觉小说 / 文字冒险引擎：**C++20 + bgfx 渲染 + SDL3 窗口 + Lua 5.4 脚本**，内置 Live2D、3D 小游戏、SMA 骨骼动画与云存档。
它的脚本语言是 **KAG Neo-Genesis**——脱胎于老一代 KAG3 的现代化标签语法，老 KAG3 工程可以导入迁移。

引擎自带一个 **16 步教程路径**（`demo/tutorial/tutorial_01_hello.ks` → `tutorial_16_tween.ks`）
和一个**完整示例游戏《单程回信》**（`demo/example_game/`），新人可以照猫画虎。

---

## 2. 社区入口：在哪讨论

主阵地为 **GitHub Discussions**（与 Issue 分开：讨论是「对话」，Issue 是「追踪」）。建议按话题分类发起 / 参与讨论：

| 话题分类 | 用途 | 示例 |
|----------|------|------|
| 💬 **提问 / 求助** | 编译报错、引擎 / 工具用法、KAG 语法困惑 | Windows 上 CMake 找不到 SDL3、[tween] 怎么用 |
| 🎨 **作品展示** | 用引擎做的游戏 / 场景 / 立绘 / MV | 贴 itch.io 或 GitHub Pages 链接 + 截图 |
| 🔧 **引擎开发** | 源码级讨论：接口设计、渲染、脚本、构建 | RFC、性能、架构讨论 |
| ✍️ **内容创作** | 剧本写作、UI / 美术 / 音频资源、经验分享 | 写作技巧、素材制作流程 |
| 🚀 **发布与打包** | 一键打包、itch / GitHub Pages 分发 | package_game.sh 用法 |

**Guidelines**：
- 先搜再问（重复问题先看已有讨论 + [docs/](../../docs/)，尤其是 getting-started 与常见 Issue）。
- 提问带环境与复现：操作系统、CMake 版本、报错日志。
- 报告真实 Bug / 功能请求请改用 **Issues**（用 [bug_report](../../.github/ISSUE_TEMPLATE/bug_report.md) / [feature_request](../../.github/ISSUE_TEMPLATE/feature_request.md) 模板），Discussions 内讨论定的结论可转成 Issue 追踪。
- 安全漏洞请**私信维护者**，勿公开披露。

---

## 3. 学习路径：从 0 到能创作

按此路径循序渐进，约半天到几天即可上手：

```text
getting-started（跑通 Demo）
      │
      ▼
tutorial 01–16（逐能力学习 KAG Neo-Genesis）
      │
      ▼
example_game（完整示例游戏，照着改剧情）
      │
      ▼
引擎文档（深入 API / 架构）
      │
      ▼
发布作品（itch.io / GitHub Releases / GitHub Pages）
```

### 第 1 步 — 跑通引擎

[docs/guides/getting-started.md](getting-started.md) —— 从克隆到 Demo 可跑，含逐平台依赖安装、构建命令与 Smoke 自检清单。目标是：能看到引擎窗口、能跑起示例剧本。

### 第 2 步 — 教程路径 tutorial 01–16

按顺序运行 16 个递进式教学剧本，即可掌握 KAG Neo-Genesis 全部基础（每个都是独立可跑、带逐行注释、经引擎 + Web 播放器双重验证）：

| 步骤 | 文件 | 学习内容 |
|------|------|----------|
| 01 | demo/tutorial/tutorial_01_hello.ks | 最小剧本结构：[ch] / [p] / [end] |
| 02 | demo/tutorial/tutorial_02_text.ks | 文本与排版：字体 / 字号 / 打字速度 / 说话人 / 等待 / 滚动 |
| 03 | demo/tutorial/tutorial_03_layers.ks | 图层系统：背景 / 前景 / 立绘 / 位置 / 动画 / 清层 |
| 04 | demo/tutorial/tutorial_04_audio.ks | 三总线音频：BGM / SE / Voice / 音量 / 淡入淡出 / 交叉淡化 |
| 05 | demo/tutorial/tutorial_05_branching.ks | 变量与流程：赋值 / 条件分支 / 标签跳转 |
| 06 | demo/tutorial/tutorial_06_effects.ks | 特效与转场：闪白 / 震动 / 溶解 / 结局解锁 |
| 07 | demo/tutorial/tutorial_07_saveload.ks | 存档 / 读档：槽位 / 结果分支（Web 优雅降级） |
| 08 | demo/tutorial/tutorial_08_system_ui.ks | 系统 UI：画廊 / 音乐室 / 历史 / 章节选择 / 解锁 |
| 09 | demo/tutorial/tutorial_09_interpolation.ks | 文本插值与表达式：$tbl.key / %key% / 表达式插值 |
| 10 | demo/tutorial/tutorial_10_loops.ks | 循环控制：for / while + eval 递减 |
| 11 | demo/tutorial/tutorial_11_switch.ks | 多路分支：switch / case / default（KAG3 兼容） |
| 12 | demo/tutorial/tutorial_12_expr_combo.ks | 表达式组合实战：三元 / 空合并 / 循环 + 插值 |
| 13 | demo/tutorial/tutorial_13_commands.ks | KAG3 兼容命令实战：打字速度 / 算术链 / 立绘移动 / 通知 / 调色 |
| 14 | demo/tutorial/tutorial_14_flow_timing.ks | 计时与流程：wait / delay / 混合跳转 / i18n 热切换 |
| 15 | demo/tutorial/tutorial_15_expr_deep.ks | 高级表达式：嵌套三元 / 多参函数 / 空合并 / 作用域前缀 |
| 16 | demo/tutorial/tutorial_16_tween.ks | 声明式补间 [tween]：属性插值 / 缓动 / 非阻塞 |

> ⚠️ 教程 16 依赖 [tween] 命令（round 106 起）。若 ks_check 将其判为未知命令，说明模块尚未登记进 `kag/init.lua`，见 [sample-library.md](sample-library.md) 的当前状态标注。

运行方式（仓库根）：
```bash
external/lua/lua.exe scripts/ks_check.lua demo/tutorial/tutorial_01_hello.ks
# 或全部教程一起校验
for f in demo/tutorial/tutorial_*.ks; do external/lua/lua.exe scripts/ks_check.lua $f; done
```

### 第 3 步 — 完整示例游戏 example_game

引擎自带成品示例 **《单程回信》（The One-Way Reply）**：现代校园 · 温情悬疑 · 短篇多结局，约 15–18 分钟，演示三结局 + 玩家选择 + 信任差分 + i18n 双语 + SMA 骨骼动画 + 双存档点。

- [demo/example_game/DESIGN.md](../../demo/example_game/DESIGN.md) —— 完整设计文档（世界观 / 角色 / 流程 / 能力展示清单）
- [demo/example_game/README.md](../../demo/example_game/README.md) —— 快速上手（修改剧本 story.ks）
- 启动：`lua demo/example_game/entry.lua`（无 lua 用 `external/lua/lua.exe demo/example_game/entry.lua`）

**把它当成模板**：改 `story.ks` 的剧情、换 `assets/` 下的立绘与 BGM，就是你的第一个游戏原型。

### 第 4 步 — 深入引擎文档

当需要底层能力时，按 AGENTS.md §12 的 5 类文档查阅：

| 类别 | 目录 | 关键入口 |
|------|------|----------|
| api/ | docs/api/ | 命令契约 command-contracts.md（权威）、Lua 模块、C++ 接口、编辑器 RPC |
| design/ | docs/design/ | 架构拓扑、能力矩阵、KAG Neo-Genesis 标准、市场分析 |
| guides/ | docs/guides/ | getting-started、asset-pipeline、carc-packaging、live2d-setup、packaging-ux |
| plans/ | docs/plans/ | 执行记录与 roadmap（产品化阶段状态） |
| solutions/ | docs/solutions/ | 可复用经验 / 模式（YAML 可搜索） |

快速导航：
- [KAG Neo-Genesis 语言白皮书](../../docs/design/kag-neo-genesis-language.md)
- [命令契约（119 个）](../../docs/api/command-contracts.md)（自动生成，权威）
- [KAG 语言速查](../../docs/guides/kag-language-tour.md)
- [资源管线 / 目录规范](../../docs/guides/asset-pipeline.md)

### 第 5 步 — 发布你的作品

引擎提供**一键打包**为静态 Web 播放器站点（无后端、无需额外运行时），可直接上传到 itch.io / GitHub Pages / Netlify / 任意静态托管：

```bash
# 在仓库根（git bash）
bash scripts/package_game.sh demo/example_game   # 打包示例游戏
bash scripts/package_game.sh my_game/scene.ks   # 打包你自己的剧本
# 产物在 dist/<game>/——静态站点，直接上传分发
```

发布入口建议：
- **itch.io**：免费 / 付费上架，社区人气高，支持 Web 嵌入式试玩。
- **GitHub Releases**：发桌面版二进制（Windows / macOS / Linux 分平台构建产物，见 [release-process.md](release-process.md)）。
- **GitHub Pages**：把 dist/ 下的站点部署为静态站，仓库里即点即玩。

详情见 [docs/guides/packaging-ux.md](packaging-ux.md)（一键打包指南）与 [docs/guides/release-process.md](release-process.md)（发版流程）。完成品记得回 Discussions「作品展示」贴链接——让社区看到你的作品！

---

## 4. 参与贡献

想改代码 / 文档 / 测试？完整流程见 **[CONTRIBUTING.md](../../CONTRIBUTING.md)**，要点：

- **Fork → 分支（codex/<描述>）→ 语义提交 → PR**。
- **合并门禁**：全量构建零错误 + 四套件测试全绿（C++ doctest + Lua 套件 + Web vitest + Editor vitest）+ 耦合门禁 + git diff --check。
- **文档规范**：按 api / design / guides / plans / solutions 五类归位（AGENTS.md §12）。
- 想贡献但不确定从哪开始？去 Discussions「引擎开发」或 Issues 里找标着 good first issue 的。

还不会写代码、但会用引擎创作内容？同样是贡献——优质教程、示例游戏、美术 / 音频素材都能帮助社区。

---

## 5. 资源总览

- [README](../../README.md) —— 项目总览（特性 / 架构 / 模块 / 文档索引 / 示例库）
- [CONTRIBUTING.md](../../CONTRIBUTING.md) —— 参与贡献（PR 流程 / 测试 / 文档规范）
- [getting-started.md](getting-started.md) —— 从克隆到可跑
- [sample-library.md](sample-library.md) —— 示例库 + 教程路径 01–16 覆盖矩阵
- [packaging-ux.md](packaging-ux.md) —— 一键打包分发

现在就差你啦——选择你的第一步（跑 Demo / 写剧本 / 看源码 / 发作品），然后到 Discussions 与大家碰头！
