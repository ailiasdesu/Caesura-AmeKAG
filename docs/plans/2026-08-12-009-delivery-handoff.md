# Caesura (AmeKAG) — 交接文档（2026-08-12 第 9 轮迭代）

> 面向后续 agent 的完整上下文。本轮延续"代差路线图"（
> `docs/plans/2026-08-12-004-generation-gap-roadmap.md`），补齐 Ren'Py
> 标志性功能缺口 **NVL 模式**（全屏累积文本）——对标 Battle 2 表达力
> 增强（Ren'Py `nvl` 对齐）。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交，自 8483b566 起）

| 提交 | 内容 |
|---|---|
| `feat(kag)` | **NVL 模式**：`[nvl]`/`[nvl clear]`/`[nvl off]` 命令（schema 契约 `mode`）；`[ch]`/`[text]` 在 NVL 下**累积**（不再 clear_text/clear），行首固定列 x=48、页首 y=160、整行宽 1184；说话人以**行内标签**渲染（`nameplate_style.text_color`，`backend.render_text` 单次，不再用固定名字牌层）；`[p]`/`[er]` 在 NVL 下翻页并重置游标；`text_scene.commit` 封存已揭示行（打字机只动画新追加行）；`nvl_mode` 持久化（save 读/写 + snapshot capture/restore）；编辑器关键字表加 `nvl` |
| `docs` | 教程新增 §14 NVL 模式（SMA 顺延为 §15、完整示例 §16）、能力矩阵新增 S2t 行 + 总数 59→60、命令契约重生成 78→81（含 nvl/sma_play/sma_stop）、api-stats 刷新（契约 81、Lua 117、C++ 617/3002）、交接 009 |

**游标复用（零新状态字段）**：NVL 累积游标直接复用 `text_state.cursor_x/y`
（`textCursorX/Y` 镜像）——`[ch]`/`[text]` 绘制后 `add_wrapped*` 已把它推进到
行尾，下一行自然续排；`[nvl]`/`[nvl clear]`/`[p]`/`[er]` 通过
`nvl_reset_cursor` 复位到页首。存档/回滚的 `text_state` 序列化路径**无需改动**
即自动保存/恢复整页位置。

## 2. 架构要点（本轮变化）

- **累积 vs 替换**：`ch`/`text` 加 `local nvl = ctx.nvl_mode == true` 分支——
  非 NVL 走既有 `clear_text + TextScene.clear`；NVL 走 `TextScene.commit`
  （封存已揭示行）+ 从 `ctx.textCursorY or NVL_Y0` 续排。
- **commit 封存机制**：`TextScene.commit` 把已有 `typewriter` 绘制标记为
  `false` 并清 `_shown/_shown_len` 缓存——render() 按全局 `reveal_chars`
  切片所有 typewriter 绘制，封存后历史行不再被打字机截断，仅新行动画。
- **说话人行内标签**：NVL 下 `#speaker > 0` 走
  `backend.render_text(speaker, NVL_X, msgY - lineHeight, ...)`，颜色取
  `nameplate_style.text_color`；`clamp_byte` 复用既有 clamp 助手。
- **命令契约**：`nvl` 为 `category=text, blocking=false`，`mode` 为 string
  （`clear`/`off`/省略=enter）；裸 `[nvl clear]` 经 `params[1]` 透传。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| `{i}` 斜体剪切 | 无 | GlyphQuad 格式变更（四角偏移），可选 |
| NVL 说话人前缀样式（`「Name」：`） | 无 | 当前为行内独立标签；如需 Ren'Py 风格内联前缀可扩展 |
| SMA S5 GPU 蒙皮 shader | 可选 | CPU 软变形已够 2D VN 规模 |
| 真实 Ollama 端到端 | 用户环境 | 本机无服务；mock 全覆盖 |
| P1-6 Live2D GL/Steam | 硬件 | GL 需 Linux/macOS；Steam 需开发账号 |
| P0-1 Metal | 硬件 | macOS 实机 |
| P0-3 移动真机验证 | 设备 | Android 构建脚本已交付；APK 真机验证待设备 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 617/617（3002 断言）→ Lua 117/117
→ ctest 10/10 → 耦合 PASS（di 13/14、entry 14/14、script 11/14、其余 ≤4）。

## 5. 注意事项

- **NVL 游标依赖 text_state**：任何绕过 `TextScene.add_wrapped*` 直接写
  `textCursorY` 的新命令需在 NVL 下同步 `state.cursor_y`，否则存档/回滚会
  丢失页位置（`nvl_reset_cursor` 是复位/同步的标准姿势）。
- **commit 只封存 typewriter 绘制**：非 typewriter 绘制（ruby）不受影响；
  若未来给 ruby 加打字机，需在 commit 里同步处理。
- **save/snapshot 的 nvl_mode 是布尔白名单**：`ctx.nvl_mode = (state.nvl_mode
  == true)`，与 skip/auto/voice_muted 同款——新增布尔状态时照抄。
- **命令契约数 78→81**：`sma_play`/`sma_stop`（第 6 轮）此前未进
  command-contracts.md 的头数；本轮重生成后一并入列，非本轮新增命令。
- 历史交接：`2026-08-12-008-delivery-handoff.md` 为上一权威状态。
