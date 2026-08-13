# Caesura (AmeKAG) — 交接文档（2026-08-13 第 16 轮迭代）

> 面向后续 agent 的完整上下文。本轮为**本地化收官轮**：
> 运行时语言热切换从"仅新行生效"升级为**整页重绘**——
> 当前页 / backlog / 选择按钮 / 字幕全部跟随新语言，**超 Ren'Py**。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交）

| 提交 | 内容 |
|---|---|
| `feat(kag)` | **语言热切换整页重绘**：`[ch]`/`[text]` 显示时把**本地化前**源文本（插值后）+ 布局参数记录进 `text_state.page_src`（与 draws 平行，随 TextScene.clear/reset 清空）；提取共享重放路径 `_drawMessage`（localize → parse_markup → NVL 前缀 span → wrap，绘制带 `_page_src` 标记）与 `_renderChoices`；新增 `relocalize_page`（按 page_src 重放重绘 + 译文折行级联 y 下移 + typewriter 封存 + 光标镜像）与 `relocalize_backlog`（`entry.src` 重译，旧档无 src 保持原样）；选择按钮注册存 `src`+`scene`（staging + active 均重译重绘）；cc_text 存 `src`+`scene` 并重译；settings.lua 两处语言切换点触发（惰性 require + pcall）；snapshot `copy_text_state` 镜像复制 page_src 数组；save.lua backlog 条目持久化 `src`（向后兼容） |
| `test` | test_i18n 37→67 断言：页源记录/标记、当前行重译、旧绘制移除、backlog 重译、typewriter 封存、条目不可变、{key} 二次展开、译文标记重解析、未翻译回落、NVL 两行页 + 级联（300 字译文折行、后行下移、光标跟随）、选择按钮 active 重译重绘、cc 重译、nil ctx/空页防御、换行清页源、无 src 条目不动 |
| `docs` | tour §17 热切换重绘小节；矩阵 S2u（37→67 断言 + 整页重绘说明）；交接 016 |

## 2. 架构要点（本轮变化）

- **重放源**：`text_state.page_src` 每条目 =
  `{kind, src=<本地化前文本>, scene, speaker, opts={nvl,pos,nameX,color,lineHeight,msgX,msgY,maxWidth,font_size}}`
  ——opts 全部取**绘制时**解析值，重放不重新解析（font_size 随 [font] 变化也不影响旧行几何）。
- **绘制标记**：`_drawMessage` 产生的 draws 带 `_page_src=true`；重绘时只移除标记绘制
  （`[ruby]` 等外来绘制保留），再重放重绘。标记随 draws 条目共享进 snapshot，回滚安全。
- **级联**：每条目 yOverride = 存储 msgY + 累计级联差（新结束 y − 原结束 y）；
  条目本身**永不修改**（快照共享条目，避免污染历史）。
- **封存**：重绘行 typewriter=false（同 TextScene.commit），用户在设置菜单中，回屏全显。
- **零新 ctx 字段**：page_src 在 text_state 内；choices/cc 复用现有字段 + src。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| 真实 Ollama 端到端 | 用户环境 | mock 全覆盖 |
| P1-6 Live2D GL/Steam、P0-1 Metal、P0-3 移动真机 | 硬件 | 见 009/010 |
| SMA S5 GPU 蒙皮 | 可选 | CPU 软变形已够用 |

> 语言热切换重绘已闭环（本轮）。路线图五大战役全部阶段完成。

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 619/619（3028 断言）→ Lua 118/118
→ ctest 10/10 → 耦合 PASS → benchmark 无退化（纯 Lua 样式改动）。

## 5. 注意事项

- **page_src 生命周期**：TextScene.clear/reset 是唯一清空点（ch/text 非 NVL、
  [er]、[p]、[nvl enter/clear/off] 全部经过）；snapshot copy_text_state 对
  page_src 镜像 draws 的数组复制语义（共享条目、append-only、重绘整体替换）。
- **双本地化代价**：显示路径 handler 先 localize 一次（plain/backlog/cc），
  `_drawMessage` 重放时再 localize 一次——幂等（已译文本不会命中 lines 键），
  每行一次 fnv1a+查表，可忽略。
- **译文折行级联**：仅当译文行数与原文不同时生效（NVL 累积页 + 长译文）；
  翻译更长时后续行下移，更短时上移。
- **旧档兼容**：backlog 无 `src` 字段的条目不重译（保持存档语言文本）。
- **说话人名牌 / [ruby] 不参与重绘**（管线本就不译）。
- 历史交接：`2026-08-13-015-delivery-handoff.md` 为上一权威状态。
