# Example Game — "The One-Way Reply" / 《单程回信》

现代校园 · 温情悬疑 · 短篇多结局视觉小说，演示 **KAG Neo-Genesis** 的引擎能力（118+ 契约命令、i18n 双语、SMA 骨骼动画、声明式补间/布局、三结局解锁）。
用 `lua demo/example_game/entry.lua` 启动（从仓库根或 build 输出目录）。
设计权威：`demo/example_game/DESIGN.md`（8 场景 ~17.5 分钟 / 三结局 / 能力展示清单）。

## 演示的能力

| 特性 | 用法 |
|---|---|
| 多场景流程 | `*label` + `[jump]` / `[call]` / 跨场景跳转（预算护栏） |
| 玩家选择 | `[select]` / `[sel]` / `[endselect]` + `[button cond=]` 条件选择 |
| 信任经济学 | `f.trust` 差分文本 + 分支阈值（双层分支 B） |
| 三结局 | 归零 / 同行 / 守约 + `[ending id=...]` 解锁画廊 |
| i18n 双语 | `[i18n language=en/zh]` 热切换 + `{settings}` 词典键 |
| 表达式 | `${expr}` 插值（含三元）、`[eval]` 赋值、`[if]/[while]/[until]` |
| 存档 | 双存档点 `[save slot=]` + `[notify]` + 循环续跑 |
| 转场/特效 | `[trans]`、`[flash]`、`[quake]`、`[vib]`、`[particles]`、`[palette]` |
| SMA 融合 | `[sma_play]`/`[sma_anim]`/`[sma_variant]`/`[sma_ik]` 信使幻象 |
| 计时选择 | `[until exp= timeout=]` 雨声计数 + `[button cond=]` |
| 声明式动画 | `[tween]`（round 106 起） |

## 结构

```
demo/example_game/
├── DESIGN.md     # 设计文档（故事/场景流程/能力清单/资产/验收）
├── README.md     # 本文档
├── entry.lua     # KAG runner 启动 + Lua 侧 API + UI overlay
├── story.ks      # 完整剧本（8 流程节点 → 三结局，~446 行）
└── story_lastletter.ks  # 旧作 The Last Letter（历史参考）
```

## 修改剧本

编辑 `story.ks` 即可改剧情；资产复用 `assets/` 下的
`bg/classroom.png`、`bg/hana.png`、`fg/girl_uniform.png`、
`bgm/daily.wav`、`se/click.wav`、`voice/line01.wav`、`demo/assets/sma/hero.json`。
缺失资产按 DESIGN §5 安全降级。

## 校验（端到端）

```bash
bash scripts/verify_sample_game.sh          # ks_check 零警告 + headless DONE + 三结局可达（5/5）
lua scripts/ks_check.lua demo/example_game/story.ks   # 契约静态校验
```
