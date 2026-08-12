# Caesura (AmeKAG) — 交接文档（2026-08-12 第 8 轮迭代）

> 面向后续 agent 的完整上下文。本轮延续"代差路线图"（
> `docs/plans/2026-08-12-004-generation-gap-roadmap.md`），将内联文本
> 标记的文档化限制（b/i/size 无视觉化）闭环为 **{size} 缩放 + {b}
> 合成粗体**，达成 Ren'Py `{...}` 文本标记的主体对齐。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交，自 775a5c28 起）

| 提交 | 内容 |
|---|---|
| `feat(render)` | **内联文本标记视觉化**：`IRenderDevice::renderText` 新增 `scale`/`bold` 默认参数（接口 + Bgfx/Null/测试 mock 同步）；`TextRenderer` 字形缩放（复用 ruby 路径的 buildGlyph scale 机制）+ 合成粗体（双 pass x 偏移 ≈8% 字宽）；`BgfxRenderDevice` 在 scale/bold 时**旁路缓存路径**（几何随 scale 变化，缓存键不含 scale）；`KAGBinding` 参数 8=scale/9=bold + `backend.lua` 透传（旧调用点默认零影响）；`text_layout.parse_markup` 增 {size} 栈/{b} 栈（嵌套恢复）、span 携带 {text,color,size,bold}、`measure_character` 按 scale 缩放（**{size} 影响换行布局**）、`append_line_segments` 按 (color,scale,bold) 分组；`text_scene` add_text 增绘制字段 + 渲染透传 |
| `docs` | 教程 §13 规则更新（size 缩放/影响换行、b 合成粗体、i 保持无操作）、能力矩阵 S2s 行更新、交接 008 |

**限制收敛**：`{i}`（斜体）仍为解析并消费——GlyphQuad 是轴对齐矩形，
斜体剪切需 quad 格式变更（{x,y,w,h} → 四角偏移），记录为后续可选。

## 2. 架构要点（本轮变化）

- **renderText 双路径**：`scale==1 && !bold` → 既有缓存路径（零重建）；
  `scale!=1 || bold` → 直接路径（几何随 scale 变化，缓存不适用）。
- **markup 全链路**：解析（size/bold 栈）→ 布局（按 scale 测量换行）
  → 绘制属性（draw.scale/draw.bold）→ 渲染（render_text 8/9 参）。
- **接口变更流程**（AGENTS.md §10）：IRenderDevice.h + 3 实现 +
  KAGBinding + backend.lua + 3 处测试 mock/断言，全量重建零错误。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| `{i}` 斜体剪切 | 无 | GlyphQuad 格式变更（四角偏移），可选 |
| SMA S5 GPU 蒙皮 shader | 可选 | CPU 软变形已够 2D VN 规模 |
| 真实 Ollama 端到端 | 用户环境 | 本机无服务；mock 全覆盖 |
| P1-6 Live2D GL/Steam | 硬件 | GL 需 Linux/macOS；Steam 需开发账号 |
| P0-1 Metal | 硬件 | macOS 实机 |
| P0-3 移动真机验证 | 设备 | Android 构建脚本已交付；APK 真机验证待设备 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests ≥617（3002 断言）→ Lua ≥116 → ctest ≥10/10
→ 耦合 PASS → benchmark 无退化（机器状态参考值，stash 对照证实非回归）。

## 5. 注意事项

- **renderText 签名**：任何 IRenderDevice 新实现必须带
  `float scale = 1.0f, bool bold = false`；测试 mock 两处
  （EntryLifecycleBackends.h / test_live2d.cpp）已同步——新增 mock 时照抄。
- **缓存旁路**：scaled/bold 文本不走 renderTextCached；若未来缓存键加入
  scale/bold，可恢复单路径。
- **markup 布局**：{size} 的 scale = span.size / options.font_size（布局时
  换算）；文本测量按 scale 缩放——超大字号会提前换行（符合预期）。
- 历史交接：`2026-08-12-007-delivery-handoff.md` 为上一权威状态。
