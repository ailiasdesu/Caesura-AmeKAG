# KAG Neo-Genesis 语言教程

> 面向新作者的完整语法指南。KAG Neo-Genesis 是脱胎于 KAG3 的全新一代
> 现代化标签语法——保留 `[标签 参数=值]` 形态，但以声明式契约、Lua
> 语义、可调试性和现代工具链重建。配套：`getting-started.md`（快速
> 上手）、`command-contracts.md`（78 命令权威契约）。

---

## 1. 语法形态

一个 .ks 场景文件由四种元素组成：

```kag
; 注释：分号开头（整行）

*start                  ; 标签：星号 + 标识符（跳转目标）

[ch name="Hero" text="Hello!"]   ; 标签命令：方括号 + 命令名 + 参数
[p]                              ; 无参数命令

裸文本行 = 对话           ; 裸文本自动作为 [ch] 处理
```

- **标签**：`*label_name`，`[jump]`/`[call]` 的目标
- **命令**：`[命令名 参数=值 ...]`，参数用引号或裸值
- **文本行**：非标签非命令的行 = 对话（等效 `[ch text="..."]`）
- **注释**：`;` 到行尾

## 2. 对话与文本

```kag
[ch name="Hero" text="带说话人"]      ; 标准对话（name + text）
[text text="无说话人旁白"]
裸文本行自动成为对话

[l]        ; 行内换行
[p]        ; 分页：点击继续（阻塞）
[er]       ; 清除消息层
[pt speed=50]    ; 打字机速度（ms/字符，契约钳制 8..5000）
[font face="default" size=24 color="#ffffff"]
[ruby text="漢" ruby="かん"]          ; 注音假名
```

## 3. 画面与角色

```kag
[bg storage="bg/school.png"]         ; 背景
[fg storage="chara/hero.png" layer=1] ; 前景角色
[position layer="fg0" x=0.5 y=0.3 scale=1.0]
[layopt layer="fg0" opacity=0.8]
[image storage="ui/logo.png" layer="ui" x=100 y=50]

; 角色精灵家族（Neo-Genesis 扩展）
[sprite_fade speaker="Hero" to=255 time=500]
[sprite_move speaker="Hero" x=0.5 y=0.3 time=800]
[sprite_scale speaker="Hero" scale=1.2 time=400]
[sprite_swap speaker="Hero" sprite="chara/hero_angry.png"]
```

## 4. 音频

```kag
[playbgm storage="bgm/theme.ogg" volume=0.8]   ; 或 [play file=... bus=bgm]
[fadebgm volume=0.3 time=2000]
[playse storage="se/click.ogg"]
[voice file="voice/line1.ogg"]                  ; 语音（阻塞）
[stopbgm time=500]
```

## 5. 流程控制

```kag
*start

; 条件分支
[set f.hp 100]
[if exp="f.hp > 50"]
[ch text="血量充足"]
[else]
[ch text="血量不足"]
[endif]

; 循环
[set f.i 0]
[while exp="f.i < 5"]
[inc f.i 1]
[endwhile]

[for var="i" start="0" end="3" step="1"]
[ch text="第 ${i} 次"]
[endfor]

; 跳转与子程序
[jump target=*ending]
[call target=*subroutine]      ; 同场景调用，[return] 返回
[return]

*subroutine
[ch text="子程序执行中"]
[return]

*ending
[end]                          ; 或 [stop] 终止
```

## 6. 变量与表达式

变量命名空间（KAG3 语义保留）：

| 表 | 用途 | 持久化 |
|---|---|---|
| `f.` | 游戏进度变量 | 存档保存 |
| `tf.` | 临时变量 | 不保存 |
| `sf.` | 系统变量 | 存档保存 |
| `mp.` | 消息参数（[call] 传递） | 调用帧 |
| `lf.` | 局部变量（[call] 帧栈） | 调用帧 |

```kag
[set f.gold 100]              ; 赋值
[inc f.gold 50]               ; 自增
[random f.dice 1 6]           ; 随机 1..6

[ch text="金币：${f.gold}"]   ; ${expr} 插值
[ch text="进度：%f.flag%"]    ; %var% 插值（KAG3 兼容）

[if exp="f.gold >= 150 && f.flag != 0"]   ; TJS 风格表达式自动翻译
```

## 7. 选择分支

```kag
[select]
[sel text="去公园" target=*park]
[sel text="留在家里" target=*home]
[endselect]                    ; 阻塞等待选择

*park
[ch text="公园线"]
[jump target=*merge]

*home
[ch text="家里线"]

*merge
[ch text="汇合"]
```

## 8. 参数化宏

```kag
[macro scene_intro args="bg,title"]
[bg storage="%bg%"]
[ch name="Title" text="%title%"]
[endmacro]

[scene_intro bg="bg/room.png" title="第一章"]
```

## 9. Lua 混合脚本（Neo-Genesis 核心差异化）

```kag
[iscript]                       ; 多行 Lua 块
  local t = { 1, 2, 3 }
  f.sum = t[1] + t[2] + t[3]
[/endscript]

[eval exp="f.result = f.sum * 2"]   ; 行内 Lua（结果存 tf.eval_result）
[emb exp="math.floor(f.result)"]    ; 输出表达式值

[emb exp="'总伤害：' .. (f.atk * f.times)"]  ; Lua 字符串拼接
```

Lua 侧反向驱动：`kag.jump('next.ks')` / `kag.call('*sub')` / `kag.save_game(slot)`。

## 10. 存档 / 回滚 / 历史

```kag
[save 1]                  ; 存到槽位 1（裸参数）
[load 2]
[listsaves]
[rollback]                ; token 级回滚（Neo-Genesis 差异化能力）
[history]                 ; 打开 backlog
```

## 11. 转场与特效

```kag
[trans type="fade" time=800]         ; 转场
[quake time=500 amplitude=8]         ; 震屏
[flash color="#ffffff" time=200]
[vfx type="grayscale" time=500]      ; 色盲/高对比滤镜
[shake]
```

## 12. 调试与工具

- **静态校验**：`lua scripts/ks_check.lua scene.ks`（契约检查，CI 门禁）
- **语言服务**：IDE 内补全/悬停/诊断（78 契约驱动）
- **确定性执行**：`require("kag.determinism").run_scene(...)` 无 GPU 跑场景
- **导入器**：`lua scripts/kag3_import.lua --carc game.carc --path assets/script/main.ks`
  （KAG3 存量脚本迁移）

## 13. 完整示例

```kag
; tutorial.ks — 综合演示
*start
[bg storage="bg/school.png"]
[playbgm storage="bgm/theme.ogg"]

[macro intro args="who,line"]
[ch name="%who%" text="%line%"]
[endmacro]

[intro who="Hero" line="这是参数化宏！"]

[set f.affection 0]
[select]
[sel text="打招呼" target=*greet]
[sel text="无视" target=*ignore]
[endselect]

*greet
[inc f.affection 10]
[intro who="Hero" line="你好！"]
[jump target=*check]

*ignore
[intro who="Hero" line="...（被无视了）"]

*check
[if exp="f.affection > 5"]
[ch text="好感度 ${f.affection}，进展顺利"]
[else]
[ch text="好感度 ${f.affection}，还需努力"]
[endif]

[save 1]
[rollback]           ; 演示回滚
[history]
[end]
```

运行：`lua scripts/kag_runner.lua` 或通过 IDE 的 Run 按钮驱动。
参考完整游戏：`demo/example_game/`（The Last Letter，选择分支 → 三结局）。
