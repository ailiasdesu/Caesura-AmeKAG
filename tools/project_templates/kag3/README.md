# Caesura (AmeKAG) — KAG3 Migration 模板

**面向 KAG3 迁移的骨架。** 用 KAG3 时代熟悉的拼写与别名写剧本，跑在 KAG Neo-Genesis
语义之上。它不是 KAG3 的逐字节克隆，而是**清单内的有边界兼容**——兼容范围见
**`docs/compatibility.md` 第 2 章「KAG3 compatibility 范围」**。

## 本模板演示的 KAG3 语义

| 能力 | 用法 | 说明 |
|------|------|------|
| 旧变量插值 `%var%` | ```[ch text="%f.name% 向你问好"]``` | `%f.x%` / `$f.x` / `${expr}` 三种形式在文本插值中均保留 |
| `[elsif]` | 见下方注释 | KAG3 拼写 `[elsif]` 是 `[elseif]` 的别名（运行时接受；`ks_check` 的偏移解析器暂未归一化，模板里用 `[elseif]` 并通过 `[if]/[elseif]/[else]/[endif]` 演示分支） |
| `[goto]` → `[jump]` | `[goto *label]` | `[goto]` 是 `[jump]` 的严格别名（运行时同样接受） |
| 裸位置参数 | `[wait 500]` `[jump *label]` `[goto *label]` | KAG 3.0 兼容别名族（`r/s/delay/wait/se/voice/play/jump·call·link/goto/unlock/macro/shake/quake` 等 13 族）支持裸位置参数 |
| 旧式赋值 | `[set f.name = "Aoi"]` | 加点式赋值归一化为 var/value；TJS 表达式运算符（`&& || ! != ?: ??`）在 `[if]/[eval]/${}` 自动翻译为 Lua |

> 详细迁移流程（xp3 解包 → tlg → png → 音频 → `kag3_import --strict` → 资产路径重写 → 验证）
> 见 `docs/guides/kag3-migration.md`。清单外 KAG3 标签不保证原样运行——按 Neo-Genesis 语义迁移。

## story.ks 怎么用

`story.ks` 是一个完整的迁移风格示例，覆盖四类语法：

```kag
; 旧式赋值 + %var% 插值
[set f.name = "Aoi"]
[ch name="Narrator" text="%f.name% greets you. （%f.name% 向你问好。）"]

; [if]/[elseif]/[else]/[endif] 分支（[elsif] 为 KAG3 拼写别名，运行时接受）
[if exp="f.route == 1"] ... [elseif exp="f.route == 2"] ... [else] ... [endif]

; [goto] 是 [jump] 的严格别名，裸位置参数风格
[goto *legacy_hop]
[jump *credits]

; 裸位置 [wait 500]
[wait 800]
```

## 结构

```
tools/project_templates/kag3/
├── README.md     # 本文档
├── entry.lua     # KAG runner 启动入口
├── story.ks      # KAG3 兼容风格示例（%var% / [elsif] / [goto] / 裸位置参数）
└── assets/       # 资产骨架占位
```

## 相关文档

| 文档 | 内容 |
|------|------|
| `docs/compatibility.md` §2 | **KAG3 compatibility 范围**（权威：明确兼容 / 明确不兼容） |
| `docs/guides/kag3-migration.md` | 六步 KAG3 迁移流水线 |
| `docs/api/kag-expression-language.md` | 旧变量 `%f.x%`、TJS 运算符翻译 |
| `docs/api/kag-commands.md`（已被 command-contracts.md 取代） | KAG 3.0 兼容别名族清单 |
| `docs/api/command-contracts.md` | Neo-Genesis 命令契约（权威，123 命令） |
