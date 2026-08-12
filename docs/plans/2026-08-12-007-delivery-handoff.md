# Caesura (AmeKAG) — 交接文档（2026-08-12 第 7 轮迭代）

> 面向后续 agent 的完整上下文。本轮延续"代差路线图"
> （`docs/plans/2026-08-12-004-generation-gap-roadmap.md`），完成
> SMA 4d 闭环（游戏循环接驳 + 场景级确定性测试）、编辑器高亮对齐、
> 教程扩展。**先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交，自 82c2cbc9 起）

| 提交 | 内容 |
|---|---|
| `feat(sma)` | **SMA 4d 闭环**：① KAG 游戏循环接驳——`kag_demo_entry.lua` engine_update 末尾挂 `sma.update(ctx, dt)`、engine_render 在图层树之后挂 `sma.render(ctx)`（均 pcall + 惰性 require，无 actor/无模块/无 GPU 绑定全惰性）；② S4 场景级确定性测试——test_sma.lua 新增 7 断言（32/32）：`determinism.run_scene` 经真实 scheduler 无 GPU 跑 `[sma_play]`（actor 状态 anim/x/y/scale + 脚本继续）/`[sma_stop]`（移除 + 继续）/未知资产（惰性不崩溃）三场景；要点：命令模块须预载使 schema 契约表完整（mock 表由 dumpContracts 构建）、场景段置 `_G.sma=nil` 走真实惰性路径 |
| `feat(editor)` | **语法高亮对齐**：kagLanguage.ts KAG_COMMANDS 补全 12 个缺失命令（对照 schema.dumpContracts + FLOW 权威集）——until/auto/moveto/camera/particles/playbgmstop/voice_off/chapter/ending/replay/sma_play/sma_stop；typecheck + build 通过 |
| `docs(guides)` | 语言教程新增 §14 骨骼网格动画（SMA）：数据注册（[iscript] + sma.register/sma.load）、场景内用法、每帧驱动说明、枢轴烘焙/软变形路径、无 GPU 惰性、与 §13 标记限制互引 |

## 2. 架构要点（本轮变化）

- **SMA 全链路闭环**：命令（[sma_play]/[sma_stop]）→ 驱动（kag/sma.lua
  actor 状态）→ 每帧 update（层级+权重混合+绑定更新）/render（绘制）
  已全部接入 KAG 入口；无 GPU 全链惰性。
- **场景级测试模式**：determinism.run_scene + kag_override 注入真实
  handler 的用法已固化（见 test_sma.lua §5 注释）；契约表完整性依赖
  命令模块预载。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| SMA S5 GPU 蒙皮 shader | 可选 | CPU 软变形已够 2D VN 规模（<4k 顶点 <0.1ms）；性能触发时再引入 |
| 内联文本标记 b/i/size 视觉化 | 渲染器 | 需 FreeType 粗体/斜体/字号变体（loadTTF 按面尺寸）——文档化限制保持 |
| 真实 Ollama 端到端 | 用户环境 | 本机无服务；mock 全覆盖 |
| P1-6 Live2D GL/Steam | 硬件 | GL 需 Linux/macOS；Steam 需开发账号 |
| P0-1 Metal | 硬件 | macOS 实机 |
| P0-3 移动真机验证 | 设备 | Android 构建脚本已交付；APK 真机验证待设备 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests ≥617 → Lua ≥116 → ctest ≥10/10 → 耦合 PASS →
benchmark 无退化（tokenizer/scheduler 为机器状态参考值——stash 对照证实
非代码回归）。

## 5. 注意事项

- **SMA 测试顺序**：test_sma 在 runner 中位于 test_sandbox 之前（写
  binding mock 的全局替换必须在 sandbox 锁环境前）；场景段依赖命令模块
  预载与 `_G.sma=nil`。
- **kagLanguage.ts**：KAG_COMMANDS 为手维护列表——新增契约命令后需同步
  补全（本轮已对齐 12 个；后续以 schema.dumpContracts 为权威集核对）。
- **kag_demo_entry 接驳**：SMA 钩子位于 engine_update 末尾与
  engine_render 的 layers.render/kag_runner.render 之后——新入口脚本
  若复刻 demo 入口需同步这两个钩子。
- 历史交接：`2026-08-12-006-delivery-handoff.md` 为上一权威状态。
