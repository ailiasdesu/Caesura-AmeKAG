# Example Game — 《单程回信》The One-Way Reply

现代校园 · 温情悬疑 · 短篇多结局视觉小说，用 **KAG Neo-Genesis** 语言编写，
演示引擎的完整能力：**123 个契约命令**、i18n 中英热切换、SMA 骨骼动画、
声明式补间/布局、三结局解锁、信任差分分支。

- **启动**：`lua demo/example_game/entry.lua`（从仓库根或 build 输出目录均可）
- **设计权威**：`demo/example_game/DESIGN.md`（8 流程节点 / 三结局 / 能力展示清单）
- **剧本**：`story.ks`（约 446 行，8 个流程节点 → 3 个结局）

> 这是**引擎“最强形态”的验证作品** + 生态种子：每项能力在剧本里都有真实的命令落点，
> 即插即改即可学习完整创作流程（详见 [docs/guides/sample-library.md](../../docs/guides/sample-library.md)）。

---

## 演示的能力

| 特性 | 用法示例 |
|---|---|
| 多场景流程 | `*label` + `[jump]` / `[call]` + 跨场景跳转（预算护栏） |
| 玩家选择 | `[select]` / `[sel]` / `[endselect]` + `[button cond=]` 条件选择 |
| 信任经济学 | `f.trust` 差分文本 + 分支阈值（场景 4 双层分支） |
| 三结局 | 归零 / 同行 / 守约 + `[ending id=...]` 解锁画廊 |
| i18n 双语 | `[i18n language=en/zh]` 热切换 + `{settings}`/`{items}` 词典键（含复数） |
| 表达式 | `${expr}` 插值（含三元）、`[eval]` 赋值、`[if]`/`[while]`/`[until]` 计时 |
| 存档 | 双存档点 `[save slot=]` + `[notify]` + 循环续跑 |
| 转场/特效 | `[trans]`、`[flash]`、`[quake]`、`[vib]`、`[particles]`、`[palette]` |
| SMA 融合 | `[sma_play]`/`[sma_anim]`/`[sma_variant]`/`[sma_ik]` 信使幻象 |
| 计时选择 | `[until exp= timeout=]` 雨声计数 + `[button cond=]` 三选项分支 |
| 声明式动画 | `[tween]`（round 106 起）+ `[layout]` 声明式布局（round 107 起） |

> 命令参考：`docs/api/command-contracts.md`（**123 契约命令**，自动生成、权威）——
> 每个命令的参数/默认值/取值范围/必填都可查。

---

## 结构

```
demo/example_game/
├── DESIGN.md     # 设计文档（故事 / 场景流程 / 能力清单 / 资产 / 验收）
├── README.md     # 本文档
├── entry.lua     # KAG runner 启动 + Lua 侧 API + UI overlay
├── story.ks      # 完整剧本（8 个流程节点 → 三结局，约 446 行）
└── story_lastletter.ks  # 旧作 The Last Letter 的剧本（历史参考，非默认入口）
```

---

## 修改剧本（内容作者最快上手路径）

1. 备份：`cp demo/example_game/story.ks my_story.ks`
2. 编辑剧本：改台词、加场景、加分支（分支模板见 docs/guides/kag-language-tour.md §22 的“选择分支”模板）。
3. 改资产：立绘/背景/音乐/语音放在 `assets/` 下（命名规范见 [docs/guides/asset-pipeline.md](../../docs/guides/asset-pipeline.md)）。
4. 校验：

```bash
# 静态契约（0 violations = 通过）
lua scripts/ks_check.lua demo/example_game/story.ks

# 端到端验证（ks_check + headless DONE + 三结局可达，5/5 PASS）
bash scripts/verify_sample_game.sh
```

5. 打包分发：

```bash
# 一键打包为 Web 静态站（itch.io / GitHub Pages / Netlify）
bash scripts/package_game.sh demo/example_game
# 桌面 Release：见 docs/guides/release-process.md（含 CPack + gh release）
```

> **缺资产不炸引擎**：缺失图片显示占位纹理（开发紫/发布灰）、缺失音频静音无等待死锁
> ——可以先用占位资产写剧本、后补正式素材（安全降级策略见 DESIGN.md §5）。

---

## 校验（端到端，含测试基线）

| 门禁 | 命令 | 期望 |
|------|------|------|
| 静态契约 | `lua scripts/ks_check.lua demo/example_game/story.ks` | 0 violations |
| Headless 全流程 | `bash scripts/verify_sample_game.sh` | RESULT DONE + 5/5 PASS |
| 三结局可达 | `SAMPLE_ENDING=ending_zero bash scripts/verify_sample_game.sh`（另 companion/promise） | 各自 DONE |
| 引擎全量 | `cd build/tests/Debug && ./CaesuraTests.exe` | 0 failed（基线 976/976，8858 断言） |
| Lua 套件 | `external/lua/lua.exe tests/scripts/run_lua_tests.lua` | 主套件 132 + 孤儿 24 全绿 |
| 契约参考 | `docs/api/command-contracts.md` | 123 命令（自动生成权威） |

---

## 相关文档

- [docs/guides/sample-game-verification.md](../../docs/guides/sample-game-verification.md) — 双端验证设施说明
- [docs/guides/sample-game-release.md](../../docs/guides/sample-game-release.md) — 发布就绪检查与双路径发布（GitHub / itch.io）
- [docs/guides/sample-game-assets.md](../../docs/guides/sample-game-assets.md) — 资产审计与 6 项降级策略
- [docs/guides/sample-library.md](../../docs/guides/sample-library.md) — 示例库总览（16 教程 + 示例游戏）
