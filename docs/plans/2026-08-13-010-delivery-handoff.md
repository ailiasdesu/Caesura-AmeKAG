# Caesura (AmeKAG) — 交接文档（2026-08-13 第 10 轮迭代）

> 面向后续 agent 的完整上下文。本轮为**表达力收尾轮**，补齐 009 交接
> 遗留的两个可闭环小项：`{i}` 斜体剪切（渲染管线）+ NVL 说话人行首
> 内联前缀「Name」：。延续"代差路线图"
> （`docs/plans/2026-08-12-004-generation-gap-roadmap.md`）Battle 2
> 表达力方向（Ren'Py `{...}` 标记对齐收尾）。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交）

| 提交 | 内容 |
|---|---|
| `feat(render)` | **`{i}` 斜体剪切**：`IRenderDevice::renderText` 增加 `bool italic = false`（第 10 参，接口 + BgfxRenderDevice/NullRenderDevice/TextRenderer 全链）；`GlyphQuad` 格式变更（TextRenderer.h:113）——增加 `shear` 字段（顶边水平偏移 px，默认 0，默认成员初始化），`submitGlyphQuads` 顶点构建顶边两角 `x + shear`、底边不动（倾斜不改变 advance 度量）；`renderText` 中 `shear = italic ? max(1.0f, 0.18f * m_fontGlyphH * scale) : 0`，bold 二次 quad 拷贝继承 shear（`{i}`+`{b}` 可叠加）；缓存路径判定 `scale!=1 \|\| bold \|\| italic` 走直接路径 |
| `feat(kag)` | **`{i}` Lua 管线**：`text_layout.lua parse_markup` 的 `{i}` 从 `noop` 改为 italic 标志（`italic_stack`/`current_italic`，span/字符/段全部携带）；`text_scene.lua add_text` 增加 italic 与 **instant** 两个尾部可选参数（`typewriter = not instant`），`add_wrapped_spans` 透传 `seg.italic`/`seg.instant`；段合并 `same_style` 含 italic 与 instant（保证 NVL 前缀独立成段） |
| `feat(kag)` | **NVL 说话人行首内联前缀**：`text.lua` NVL 分支——有说话人且有消息时构造 `「Name」：` 前缀 span（颜色取 `nameplate_style.text_color`，`instant=true`）插入 spans 头部，无 markup 消息转单 span 统一走 `add_wrapped_spans`；空消息 `[ch]` 保留原独立标签。前缀随行 wrap、游标由 `add_wrapped_spans` 返回值推进——**零新状态字段**，存档/回滚/NVL 游标机制不动 |
| `fix(kag)` | **潜伏颜色 bug**：`st and st.text_color and st.text_color:match(...)` 的 and 链在 Lua 中**截断多返回值**（二元运算只保留第一个），导致 NVL 说话人颜色 g/b 恒为 0（渲染成红色）。已改为 if 守卫 + 直接 `match` 调用（前缀分支 + 空消息标签分支两处） |
| `test` | 测试扩展：`test_text_markup.lua` 26→31（{i} italic 标志/`{b}`+`{i}` 嵌套/未闭合/instant 段独立/instant draw 非打字机）；`test_nvl.lua` 25 项（前缀 draw 结构/颜色/instant/backlog 纯文本/空消息回退）；`test_kag_execution.cpp` render_text 参数计数断言 9→10（含 italic 默认 false） |
| `docs` | 教程 §13 `{i}` 已渲染表述 + §14 NVL 前缀样式、§15 过时"无操作"表述清理；能力矩阵 S2s/S2t 行更新（总 60 项不变）；api-stats 重生成（数字无变化）；交接 010 |

## 2. 架构要点（本轮变化）

- **GlyphQuad 四角偏移**：`{x, y, w, h, shear = 0.0f, u0, v0, u1, v1}`——shear
  只偏移顶边两角，底边固定，因此 **advance 与换行布局完全不变**（斜体不
  影响文本流）。所有既有构造点（buildGlyph 两条路径）经默认成员初始化
  安全获得 0。
- **instant draw（非打字机绘制）**：`add_text(..., instant)` → `typewriter =
  not instant`。NVL 前缀用它保证"说话人立即显示、仅消息打字机揭示"——
  reveal 预算（`reveal.total`/`char_offset`）**仍只计消息 plain 长度**，
  存档/回滚/跳过语义零变化。
- **instant 参与段合并身份**：`same_style` 含 instant，确保前缀与消息
  即使颜色相同（nameplate 白 vs 默认白）也**各自独立成段**，消息始终是
  独立的打字机 draw。

## 3. 剩余项（按可闭环性，009 遗留清单更新后）

| 项 | 约束 | 说明 |
|---|---|---|
| `{s}` 删除线 | 无 | 仍为解析+消费 no-op；如需要可仿 `{i}` 加纹理/形态变体 |
| NVL 前缀样式参数化 | 无 | 前缀格式硬编码 `「Name」：`；如需自定义分隔符/括号可加 schema 参数 |
| SMA S5 GPU 蒙皮 shader | 可选 | CPU 软变形已够 2D VN 规模 |
| 真实 Ollama 端到端 | 用户环境 | 本机无服务；mock 全覆盖 |
| P1-6 Live2D GL/Steam | 硬件 | GL 需 Linux/macOS；Steam 需开发账号 |
| P0-1 Metal | 硬件 | macOS 实机 |
| P0-3 移动真机验证 | 设备 | Android 构建脚本已交付；APK 真机验证待设备 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 617/617（3002 断言）→ Lua 117/117
→ ctest 10/10 → 耦合 PASS（di 13/14、entry 14/14、script 11/14、其余 ≤4）。

## 5. 注意事项

- **instant 语义**：只有 NVL 前缀使用；`add_text` 的 instant 参数默认
  false，既有调用（nameplate、ruby 等）行为不变。新增"立即显示"文本时
  优先用 instant 而非绕过 TextScene，否则 reveal 预算会被吞。
- **{i} 与缓存路径**：italic 文本绕过 `renderTextCached`（几何随 shear
  变化），与 `{size}`/`{b}` 同策略；静态文本缓存不受影响。
- **Lua 多返回值陷阱**：`a and b and f()` 只保留 f 的第一个返回值——
  本轮在 text.lua 修了一处；后续写"取 match 多捕获"时务必 if 守卫 +
  直接调用。
- **命令契约数 81 不变**：本轮无新命令；`render_text` 第 10 参为绑定
  层扩展，不影响 KAG 命令面。
- 历史交接：`2026-08-12-009-delivery-handoff.md` 为上一权威状态。
