# Caesura (AmeKAG) — 交接文档（2026-08-13 第 13 轮迭代）

> 面向后续 agent 的完整上下文。本轮为**本地化工具链收尾轮**：为
> `ks_i18n` 补上 `--missing` 未翻译统计（本地化作者工作流最后一块，
> 可作 CI 门禁）。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交）

| 提交 | 内容 |
|---|---|
| `feat(kag)` | **`ks_i18n --missing`**：`find_missing(dir, langData)` 扫描场景键 vs 语言文件非空译文，返回缺失清单（键 + 原文，按场景分组）；CLI `--missing --dir X --lang Y` 打印报告 + 汇总，**有缺失退出码 1**（CI 门禁：翻译完整性检查）；重构出 `collect_entries(dir)` 供 build_template 与 find_missing 共用（单一事实源） |
| `test` | test_i18n.lua 39→44 断言：缺失计数（4/5）、已翻译键排除、原文上下文携带、无语言文件→全缺失、全翻译→零缺失 |
| `docs` | tour §17 工作流补 `--missing` 用法；交接 013 |

## 2. 架构要点（本轮变化）

- **单一事实源**：`collect_entries` 是场景→键提取的唯一入口
  （build_template 生成模板 / find_missing 统计都用它）——键空间
  不会再出现工具内部分叉。
- **缺失判定**：`lines[key] == nil`（未在语言文件出现）或空字符串
  占位符都算未翻译；非空译文即视为已翻译（与运行时 localize 的
  "空占位符回退原文"语义一致）。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| 运行时语言热切换重绘 | 无 | 切换后已显示行保持原语言，新行生效（Ren'Py 同行为）；如需整页重绘可加 |
| `{s}` 删除线 | 无 | 仍为 no-op（可仿 `{i}` 加变体） |
| NVL 前缀格式参数化 | 无 | 硬编码 `「Name」：` |
| 真实 Ollama 端到端 | 用户环境 | mock 全覆盖 |
| P1-6 Live2D GL/Steam、P0-1 Metal、P0-3 移动真机 | 硬件 | 见 009/010 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 618/618（3021 断言）→ Lua 118/118
→ ctest 10/10 → 耦合 PASS → benchmark 无退化（本轮纯 Lua 工具改动，
C++/调度器零改动）。

## 5. 注意事项

- **本地化工具链现状**（完整）：`ks_i18n.lua` 支持生成模板（--out）、
  合并（--update，保留译文与 settings 键）、未翻译统计（--missing，
  退出码门禁）；运行时两级本地化覆盖 [ch]/[text]/[button]/[sel]。
- **CI 接入建议**：`lua scripts/ks_i18n.lua --missing --dir <场景目录>
  --lang <目标语言>` 退出码 1 即未翻译——可挂进发布流水线。
- **键空间**：`collect_entries` 键 = 场景裸文件名 + fnv1a(消息)；
  与运行时 i18n.localize 完全一致（工具/运行时口径统一，见 011 教训）。
- 历史交接：`2026-08-13-012-delivery-handoff.md` 为上一权威状态。
