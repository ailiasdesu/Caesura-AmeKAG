# Example Game — "The Last Letter"

一个完整的小型视觉小说示例，演示 **KAG Neo-Genesis** 的现代化特性。
用 `lua demo/example_game/entry.lua` 启动（从仓库根或 build 输出目录）。

## 演示的能力

| 特性 | 用法 |
|---|---|
| 多章节流程 | `*label` + `[jump]` / `[call]` |
| 玩家选择 | `[select]` / `[sel]` / `[endselect]`（三条路线） |
| 多结局 | good / normal / bad + `[ending id=...]` 解锁画廊 |
| 变量与插值 | `f.` / `tf.` 变量、`${expr}` 插值、`[if]` / `[set]` |
| 参数化宏 | `[macro scene_intro args="bg,title"]` + `%bg%` 占位符 |
| Lua 混合 | `[iscript]` 调用 `_G.example_game` 全局 API |
| 存档/回滚 | `[save]` / `[load]` / `[rollback]`（引擎级能力） |
| 历史记录 | `[history]` 回顾已读文本 |
| 转场/特效 | `[trans]`、`[flash]`、`[scroll]`、`[vib]`、`[sprite_move]` |

## 结构

```
demo/example_game/
├── entry.lua    # KAG runner 启动 + Lua 侧 API + UI overlay
└── story.ks     # 完整剧本（选择分支 → 三结局）
```

## 修改剧本

编辑 `story.ks` 即可改剧情；资产复用 `assets/` 下的
`bg/classroom.png`、`bg/hana.png`、`fg/girl_uniform.png`、
`bgm/daily.wav`、`se/click.wav`、`voice/line01.wav`。

## 校验

```bash
lua scripts/ks_check.lua demo/example_game/story.ks   # 契约静态校验
```
