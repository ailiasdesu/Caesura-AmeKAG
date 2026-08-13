# Caesura (AmeKAG) — 交接文档（2026-08-13 第 14 轮迭代）

> 面向后续 agent 的完整上下文。本轮为**内联标记家族收官轮**：
> `{s}` 删除线渲染——至此 Ren'Py `{...}` 全部 5 个标记
> （{color}/{size}/{b}/{i}/{s}）在 Caesura 中全部真实渲染。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交）

| 提交 | 内容 |
|---|---|
| `feat(render)` | **`{s}` 删除线 C++ 渲染**：`IRenderDevice::renderText` 第 11 参 `bool strike = false`（接口 + Bgfx/Null/TextRenderer 全链）；`renderText` 内为每个被删除线覆盖的字形生成横线 quad（x=字形 x、y=字形 y+0.5h（中线）、w=字形步进、h=max(1, 0.1×字高)），第二趟 `submitStrikeBars` 用惰性创建的 1×1 白纹理（`ensureStrikeTexture`）提交实心条；shutdown/onDeviceLost 销毁、restore 惰性重建；缓存路径判定 `scale!=1 \|\| bold \|\| italic \|\| strike` 走直接路径；横线几何复用 `glyphQuadToNDC`（shear=0） |
| `feat(kag)` | **`{s}` Lua 管线**：`parse_open_tag` 的 `{s}` 从"未知标签字面显示"改为 `strike` 标志（此前文档声称"消费为 no-op"实为字面保留——本轮修正）；`MARKUP_CLOSE_NAMES` 加 `s`；strike 栈/span/字符/段/`same_style` 全链携带；`TextScene.add_text` 第 11 参 strike、绘制提交 `render_text(..., strike)`；KAGBinding 第 11 参、`Backend.render_text` 同步 |
| `test` | test_text_markup 31→34（{s} span 标志/{b}+{s} 叠加/未闭合）；test_kag_execution 参数计数 10→11；**新增 strike 横线几何单测**（test_render_pipeline：横线在字形中线、宽=步进、NDC y 反向修正）；C++ 619 用例/3028 断言 |
| `docs` | tour §13 `{s}` 已渲染表述；矩阵 S2s 行更新（34 Lua 断言、renderText 四参）；api-stats 重生成（619/3028）；交接 014 |

## 2. 架构要点（本轮变化）

- **横线是独立提交趟**：字形走字体图集纹理，横线走 1×1 白纹理
  （UV 0..1 全采样）——两种纹理不可混在同一趟，故 `submitStrikeBars`
  镜像 `submitGlyphQuads` 的 TVB/TIB 提交逻辑，共用 `glyphQuadToNDC`
  纯几何（shear=0）。1×1 纹理惰性创建，device lost/restore 后重建。
- **横线几何**：y = 字形 y + 0.5×字高（横线顶边贴字形中线），
  h = 0.1×字高（下限 1px），w = 字形步进（跨满字宽；TTF 字形步进
  可能略大于绘制宽度，横线可微有间隙——与 bold 双 pass 同策略）。
- **`{s}` 此前是字面显示**：`parse_open_tag` 无 `s` 分支 → 未知标签
  字面保留（"a{s}b" 原样输出）——文档"消费为 no-op"是漂移；本轮
  真正实现解析+渲染。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| 运行时语言热切换重绘 | 无 | 切换后已显示行保持原语言（Ren'Py 同行为） |
| NVL 前缀格式参数化 | 无 | 硬编码 `「Name」：` |
| 真实 Ollama 端到端 | 用户环境 | mock 全覆盖 |
| P1-6 Live2D GL/Steam、P0-1 Metal、P0-3 移动真机 | 硬件 | 见 009/010 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 619/619（3028 断言）→ Lua 118/118
→ ctest 10/10 → 耦合 PASS → benchmark 无退化（调度器/tokenizer 零改动）。

## 5. 注意事项

- **内联标记家族完整**：{color}/{size}/{b}/{i}/{s} 全部渲染且可任意
  叠加（same_style 含全部标志位）；未知 `{tags}` 仍字面保留。
- **横线 UV**：bar quad UV 固定 0..1（1×1 白纹理）——与字形 UV 无关；
  若未来改多色横线，改纹理或加 uniform。
- **接口链**：renderText 已 11 参（viewId,text,x,y,r,g,b,a,scale,bold,
  italic,strike）——第 15 参起建议改 flags 位域而非继续加 bool。
- 历史交接：`2026-08-13-013-delivery-handoff.md` 为上一权威状态。
