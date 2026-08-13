# Caesura (AmeKAG) — 交接文档（2026-08-13 第 12 轮迭代）

> 面向后续 agent 的完整上下文。本轮为**本地化覆盖扩展轮**：把第 11 轮
> 的本地化管线从对话（[ch]/[text]）扩展到选择按钮（[button]/[sel]），
> 并修复 `ks_i18n --update` 合并时丢失手写 settings 键的加载 bug。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交）

| 提交 | 内容 |
|---|---|
| `feat(kag)` | **`[button]`/`[sel]` 选择标签本地化**：`TextCommands.button` 注册时对 label（`params.text or params.caption`）调用 `i18n.localize(text, ctx.current_scene)`——`[endbutton]` 绘制与点击命中判定都用译文；`[sel]` 是 button 的别名共享该 handler；同一字符串在 [ch] 与 [button] 中共享一个翻译键（内容寻址） |
| `feat(kag)` | **`ks_i18n` 提取选择标签 + 修复合并丢键 bug**：`extract_messages` 增加 `[button]`/`[sel]` 的 text/caption 提取（demo 23→25 键）；修复 `load_lang` 对工具生成文件（注释头 + `return`）的解析失败——与 `i18n.load` 相同的剥注释 + 按形态补前缀策略（此前 `--update` 会把手写 settings 键全部丢掉） |
| `test` | test_i18n.lua 32→38 断言：按钮标签本地化（注册值 + draw 文本 "1. 選択肢A"）、[sel] 别名、工具提取按钮标签、load_lang 双形态（生成式 + 手写字面量）；套件 118/118 不变 |
| `docs` | tour §17 覆盖范围说明（按钮/菜单）；矩阵 S2u 行更新（37 断言、25 demo 键含选择标签）；ja.lua 重新生成（settings 键恢复 + 25 键）；交接 012 |

## 2. 架构要点（本轮变化）

- **选择标签在注册时本地化**：`[endbutton]` 只消费 `ctx._choiceButtons`
  里已存好的 text——注册点（而非绘制点）本地化使过滤（cond）、绘制、
  点击判定全部一致；`choice.text` 字段存译文。
- **ks_i18n 与运行时口径继续一致**：工具提取按钮 label 的键空间与
  [ch] 相同（`scene:fnv1a(text)`）——同字符串跨命令共享译文。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| 运行时语言热切换重绘 | 无 | 切换后**已显示**的对话/按钮保持原语言，新行生效（Ren'Py 同行为）；如需整页重绘可加 |
| `ks_i18n --missing` 未翻译统计 | 无 | 列出模板中空占位符键 |
| `{s}` 删除线 | 无 | 仍为 no-op（可仿 `{i}` 加变体） |
| NVL 前缀格式参数化 | 无 | 硬编码 `「Name」：` |
| 真实 Ollama 端到端 | 用户环境 | 本机无服务；mock 全覆盖 |
| P1-6 Live2D GL/Steam、P0-1 Metal、P0-3 移动真机 | 硬件 | 见 009/010 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 618/618（3021 断言）→ Lua 118/118
→ ctest 10/10 → 耦合 PASS → benchmark 无退化（本轮 Lua 套件内
bench 断言通过；C++ 零改动）。

## 5. 注意事项

- **ja.lua 合并历史**：第 11 轮的 bug 版 `load_lang` 曾把 settings 键
  从 ja.lua 丢掉——已从 `246b0bdd` 提交恢复后重新生成（现在 settings +
  25 个对话/按钮键并存）。若未来 `--update` 后 settings 消失，先查
  `load_lang` 是否又退化成 `load("return "..txt)` 前缀拼接。
- **套件顺序敏感点**：test_i18n 读取真实 `assets/lang/ja.lua`（生成式
  文件）做加载路径断言——若该文件被手改回裸表字面量，断言仍应通过
  （双形态都覆盖）；若文件缺失则失败，属预期（仓库自带该资产）。
- **本地化键空间**：[ch]/[text]/[button]/[sel] 共享 `scene:fnv1a(msg)`
  键；翻译键不含路径（场景文件名裸名）。
- 历史交接：`2026-08-13-011-delivery-handoff.md` 为上一权威状态。
