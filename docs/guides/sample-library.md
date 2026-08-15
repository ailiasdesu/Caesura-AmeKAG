# Caesura 示例库（Sample Library）

> 示例库 = 可直接运行/参考的剧本 + 资产 + 教程。每个示例都经过
> 引擎与 Web 播放器双重验证（门禁：Lua 套件 + web flow 集成测试）。

## 已收录示例

| 示例 | 文件 | 覆盖命令 | 验证 |
|---|---|---|---|
| Command Showcase | demo/showcase.ks | **25 个命令**：bg/fg/ch/cl/font/pt/wait/playbgm/playse/p/position/flash/trans/vib/sprite_move/sprite_fade/eval/if/else/endif/jump/scroll/stopbgm/ending/end | 引擎 tokenize/compile（56 tokens/6 labels）+ Web 播放器 DONE:53 + backlog 9 页 + ending 解锁 |
| Galgame Demo | demo/galgame_demo.ks | 核心 VN 流程（bg/ch/playbgm/voice/sprite/ending） | Web flow 集成测试（park/点击/DONE/hana bg/立绘） |
| Full Pipeline | demo/full_pipeline_demo.ks | 全管线流程 | ks_bake bundle |
| SMA Demo | demo/sma_demo.ks | SMA 骨骼动画命令 | ks_bake bundle |

## 教程路径（Tutorial Path，从零开始）

按顺序运行 13 个递进式教学示例，即可掌握 KAG Neo-Genesis 剧本语言的全部基础：

| 步骤 | 示例 | 学习内容 | 命令 |
|---|---|---|---|
| 01 | demo/tutorial/tutorial_01_hello.ks | 最小剧本结构：注释/命令格式/[ch]/[p]/[end] | font, pt, ch, p, end |
| 02 | demo/tutorial/tutorial_02_text.ks | 文本与排版：字体/字号/打字速度/说话人/语音/滚动字幕 | font, pt, ch, voice_wait, wait, scroll |
| 03 | demo/tutorial/tutorial_03_layers.ks | 图层系统：背景/前景/角色立绘/位置/立绘动画/清层 | bg, trans, fg, position, ch sprite, sprite_move, sprite_fade, cl |
| 04 | demo/tutorial/tutorial_04_audio.ks | 三总线音频：BGM/SE/Voice/音量/淡入淡出/交叉淡化 | playbgm, setbgmvolume, playse, voice_wait, fadebgm, xfadebgm, stopse, stopbgm |
| 05 | demo/tutorial/tutorial_05_branching.ks | 变量与流程：赋值/条件分支/标签跳转 | set, if, else, endif, jump |
| 06 | demo/tutorial/tutorial_06_effects.ks | 特效与转场：闪白/震动/溶解转场/结局解锁 | flash, vib, trans, ending, scroll, wait |
| 07 | demo/tutorial/tutorial_07_saveload.ks | 存档与读档：槽位/保存/读取/结果分支（Web 无存档后端时优雅降级） | save, load, tf.save_result, tf.load_result |
| 08 | demo/tutorial/tutorial_08_system_ui.ks | 系统 UI：CG 画廊/音乐室/历史回看/章节选择/内容解锁 | unlock, gallery, music, history, chapter |
| 09 | demo/tutorial/tutorial_09_interpolation.ks | 文本插值与表达式：$tbl.key / %tbl.key% / ${expr}（TJS 运算符 ?: && !=） | set, ch, interpolate |
| 10 | demo/tutorial/tutorial_10_loops.ks | 循环控制流：正序/倒序 [for]、[while] 条件循环 + [eval] 递减（每场景 65536 迭代守卫） | for, endfor, while, endwhile, eval, set, ch, p |
| 11 | demo/tutorial/tutorial_11_switch.ks | 多路分支：裸变量选择器（KAG3 兼容）与 exp= 表达式选择器（数值/布尔/TJS 运算符），case tostring 匹配无 fallthrough，缺失变量走 default | switch, case, default, endswitch, set, ch, p |
| 12 | demo/tutorial/tutorial_12_expr_combo.ks | 表达式组合实战：三元在索引内、?? 空合并 + switch exp、循环 + 插值、eval 三元赋值（RHS 全管道） | eval, switch, case, endswitch, for, endfor, set, ch, p |
| 13 | demo/tutorial/tutorial_13_commands.ks | KAG3 兼容命令实战：打字速度（textspeed/cps）、算术链（add/sub/mul/div/mod/dec）、角色（csp/csd/csl）、notify/palette/vibrate/preload | textspeed, cps, add, sub, mul, div, mod, dec, csp, csd, csl, notify, palette, vibrate, preload |
| 13 | demo/tutorial/tutorial_13_commands.ks | KAG3 兼容命令实战：打字速度切换、变量算术链、角色立绘 / 移动 / 清除、吐司通知、LUT 色调、消息层震动、资源预加载 | textspeed, cps, add, sub, mul, div, mod, dec, csp, csl, csd, notify, palette, vibrate, preload, set, eval, ch, p |


每个教程都是独立可运行剧本（编译 + Web 播放器双重验证，运行到 [end] 零错误），
注释里逐行讲解命令含义。教程之间相互衔接，建议按 01→13 顺序学习。

## 如何运行示例

### 引擎（桌面）

```bash
cmake --build build --config Debug
./build/Debug/CaesuraAmeKAG.exe
# 或在编辑器里打开 demo/showcase.ks
```

### Web 播放器

```bash
cd web
npm install
npx vite            # http://127.0.0.1:5174
# 场景下拉框选择 showcase.ks，▶ Run，点击推进 / ⏩ Auto
```

Web 播放器优先加载 ks_bake 预编译 bundle（cache/story/story.lua，零解析启动），
bundle 由 `lua scripts/ks_bake.lua --dir demo --web cache/story` 生成（需重新生成
以包含新示例）。

## 如何贡献示例

1. 在 demo/ 下写 `my_sample.ks`（KAG 命令见 docs/api/command-contracts.md）
2. 引擎侧验证：`external/lua/lua.exe scripts/ks_bake.lua demo/my_sample.ks`（tokenize/compile 通过）
3. 更新 web/flow.integration.test.js 加用例（可复用 showcase 测试骨架）
4. 重新生成 bundle：`lua scripts/ks_bake.lua --dir demo --web cache/story`
5. 全量门禁：Lua 套件 + web vitest + C++ 套件

## 覆盖矩阵（showcase.ks 命令）

| 类别 | 命令 |
|---|---|
| 文本 | font / pt / ch / p / scroll |
| 图层 | bg / fg / cl / position / sprite_move / sprite_fade |
| 音频 | playbgm / playse / stopbgm |
| 特效 | flash / trans / vib |
| 流程 | wait / eval / if / else / endif / jump / ending / end |

---
*示例库 round 41-42 建设（G6 起点）*
