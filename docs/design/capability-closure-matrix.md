# Capability Closure Matrix (auto-generated)

> 由 python scripts/capability_closure.py 生成；勿手动编辑。
> 生成时间（输入源最新 mtime）：2026-08-29T10:52:32Z
> 生成命令：python scripts/capability_closure.py
> 源指纹（输入内容 sha256 前 16 hex）：65f6b8d37ef164cf
> 输出确定性：同源指纹同字节（generated_at 为输入源最新 mtime；跨机 checkout 的 mtime 差异属 by-design，确定性以指纹为准）

## 概述

- 合约总数（Declared，docs/api/command-contracts.md）：**134**
- 已注册（Dispatched）：**165**
- 触达效果面（Consumed，调用形上下文，v2）：**66**
- 测试引用（Tested，启发式计数）：**137**
- UNWIRED：0 · PARTIAL：73 · CLOSED：61 · EXTRA：31
- **恒等式：134 = CLOSED(61) + PARTIAL(73) + UNWIRED(0)；165 = 134(Declared) + EXTRA(31)**

**范围声明（t103 MUST-FIX 3）**：本矩阵的 134 = 声明式 KAG 命令合约闭包（docs/api/command-contracts.md 全量条目）。下列能力**不在 134 内**：
- 原生手势链：SwipeDown / SwipeUp / LongPress / Pinch / TwoFingerTap / ThreeFingerHold（平台层）；
- 文本标记参数：letter_spacing / spacing / font / line_height 等内联标记（非命令）；
- KAG.* Lua API：jump/call/return_to_caller 等直接 API（注册键存在但非合约命令，见 EXTRA 的 api-alias）。
这些能力由矩阵底部「人工判级（范围外能力）」区段承载（docs/design/capability-closure-overrides.json 驱动）。
**EXTRA 属设计行为**：contracts 由声明式 schema registry（kag/schema.lua，经 scripts/schema_doc.lua 生成）
产出；流控/API 命令按设计不在注册表——EXTRA 不是缺陷信号（A 类入册与否=产品决策待议）。

> 状态定义：**UNWIRED**=有合约无处理器；**PARTIAL**=已注册但处理器体未以调用形触达效果面（v2 口径）；
> **CLOSED**=已注册且调用形触达效果面；**EXTRA**=已注册但无合约。
> ⚠ = 人工覆盖（docs/design/capability-closure-overrides.json；详见『人工覆盖』节）。

## Commands

| Command | Declared | Dispatched | Consumed | Tested | Observable | Platform Tested | Packaged | Status | 证据 |
|---|---|---|---|---|---|---|---|---|---|
| Bezier | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/transition.lua:606 |
| LUTCache | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/transition.lua:605 |
| add | Y | Y | n | 50 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:121 |
| ai_dialog | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/system.lua:714 |
| assert | Y | Y | n | 7 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:506 |
| auto | Y | Y | n | 6 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:1111 |
| bg | Y | Y | Y | 21 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:120 |
| bgm | Y | Y | n | 4 | ? | ? | ? | PARTIAL | scripts/kag.lua:490 |
| blur | Y | Y | n | 4 | ? | ? | ? | PARTIAL | scripts/kag/commands/transition.lua:292 |
| br | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag.lua:212 |
| button | Y | Y | n | 38 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1340 |
| call | n | Y | n | 75 | ? | ? | ? | EXTRA | scripts/kag.lua:512 |
| camera | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:495 |
| cancel | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag.lua:220 |
| capture_state | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/save.lua:227 |
| ch | Y | Y | Y | 549 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:609 |
| chapter | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:349 |
| cl | Y | Y | Y | 11 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:183 |
| clear | n | Y | Y | 1 | ? | ? | ? | EXTRA | scripts/kag.lua:349 |
| clearscreen | n | Y | Y | - | ? | ? | ? | EXTRA | scripts/kag.lua:209 |
| close | Y | Y | Y | 2 | ? | ? | ? | CLOSED | scripts/kag.lua:228 |
| cps | Y | Y | n | 25 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:1218 |
| csd | Y | Y | Y | 9 | ? | ? | ? | CLOSED | scripts/kag/commands/character.lua:150 |
| csl | Y | Y | Y | 14 | ? | ? | ? | CLOSED | scripts/kag/commands/character.lua:166 |
| csp | Y | Y | Y | 25 | ? | ? | ? | CLOSED | scripts/kag/commands/character.lua:120 |
| ct | n | Y | Y | 2 | ? | ? | ? | EXTRA | scripts/kag.lua:352 |
| dec | Y | Y | n | 19 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:129 |
| delay | Y | Y | n | 33 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag.lua:337 |
| div | Y | Y | n | 15 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:124 |
| edit | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:1882 |
| emb | Y | Y | n | 16 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:92 |
| end | n | Y | n | 210 | ? | ? | ? | EXTRA | scripts/kag.lua:86 |
| endbutton | Y | Y | n | 30 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:1374 |
| endform | n | Y | n | 1 | ? | ? | ? | EXTRA | scripts/kag.lua:360 |
| ending | Y | Y | n | 22 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:365 |
| endmacro | n | Y | n | 54 | ? | ? | ? | EXTRA | scripts/kag.lua:242 |
| endselect | Y | Y | n | 22 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1490 |
| endtag | n | Y | n | 1 | ? | ? | ? | EXTRA | scripts/kag.lua:356 |
| er | Y | Y | Y | 2 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:930 |
| erasemacro | n | Y | n | 6 | ? | ? | ? | EXTRA | scripts/kag.lua:243 |
| eval | Y | Y | n | 49 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:179 |
| fade | Y | Y | Y | 2 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:568 |
| fadebgm | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:174 |
| fadeout | Y | Y | n | 2 | ? | ? | ? | PARTIAL | scripts/kag.lua:371 |
| fadevol | Y | Y | Y | - | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:104 |
| fg | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:156 |
| flash | Y | Y | n | 6 | ? | ? | ? | PARTIAL | scripts/kag/commands/vfx.lua:368 |
| flush_cache | n | Y | Y | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:254 |
| font | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:1067 |
| g | n | Y | n | 2 | ? | ? | ? | EXTRA | scripts/kag.lua:363 |
| gallery | Y | Y | n | 6 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:333 |
| get_texture | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:226 |
| has_pending_transition | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:308 |
| history | Y | Y | n | 9 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:231 |
| hr | Y | Y | n | 2 | ? | ? | ? | PARTIAL | scripts/kag.lua:217 |
| i18n | Y | Y | n | 60 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:691 |
| image | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:217 |
| inc | Y | Y | n | 29 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:471 |
| input | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:1630 |
| is_loaded | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:235 |
| is_pending | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:244 |
| jump | n | Y | n | 178 | ? | ? | ? | EXTRA | scripts/kag.lua:501 |
| l | Y | Y | n | 4 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:906 |
| layfade | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:335 |
| layopt | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:311 |
| layout | Y | Y | n | 26 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/layout.lua:169 |
| layout_place | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/layout.lua:243 |
| layout_slot | Y | Y | n | 25 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/layout.lua:204 |
| ld | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag.lua:397 |
| listsaves | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/save.lua:523 |
| live2d_expression | Y | Y | n | - | ? | ? | ? | PARTIAL | scripts/kag/commands/character.lua:189 |
| live2d_lip_sync | Y | Y | n | - | ? | ? | ? | PARTIAL | scripts/kag/commands/character.lua:200 |
| live2d_motion | Y | Y | n | - | ? | ? | ? | PARTIAL | scripts/kag/commands/character.lua:177 |
| load | Y | Y | Y | 69 | ? | ? | ? | CLOSED | scripts/kag/commands/save.lua:298 |
| loadplace | Y | Y | n | 6 | ? | ? | ? | PARTIAL | scripts/kag/commands/save.lua:554 |
| macro | n | Y | n | 56 | ? | ? | ? | EXTRA | scripts/kag.lua:241 |
| mod | Y | Y | n | 13 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:125 |
| move | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:387 |
| moveto | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:288 |
| mul | Y | Y | n | 11 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:123 |
| music | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:343 |
| nameplate | Y | Y | n | 6 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:418 |
| notify | Y | Y | n | 38 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:648 |
| nvl | Y | Y | Y | 18 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:976 |
| p | Y | Y | Y | 286 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:947 |
| palette | Y | Y | n | 26 | ? | ? | ? | PARTIAL | scripts/kag/commands/vfx.lua:451 |
| particle_weather | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/vfx.lua:589 |
| particles | Y | Y | Y | - | ? | ? | ? | CLOSED | scripts/kag/commands/vfx.lua:384 |
| play | Y | Y | n | 6 | ? | ? | ? | PARTIAL | scripts/kag.lua:467 |
| playbgm | Y | Y | Y | 16 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:109 |
| playbgmstop | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:148 |
| playse | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:210 |
| playstop | Y | Y | n | 6 | ? | ? | ? | PARTIAL | scripts/kag.lua:427 |
| playvoice | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:240 |
| position | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:297 |
| postprocess | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag/commands/vfx.lua:507 |
| postprocess_off | Y | Y | Y | 2 | ? | ? | ? | CLOSED | scripts/kag/commands/vfx.lua:519 |
| preload | Y | Y | n | 11 | ? | ? | ? | PARTIAL | scripts/kag/commands/resource.lua:154 |
| preload_transition | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:280 |
| promote_transition_slot | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:294 |
| pt | Y | Y | n | 11 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:1152 |
| push_backlog | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/text.lua:333 |
| quake | Y | Y | n | 2 | ? | ? | ? | PARTIAL | scripts/kag.lua:421 |
| r | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag.lua:288 |
| random | Y | Y | n | 14 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:528 |
| relocalize_backlog | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/text.lua:1513 |
| relocalize_page | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/text.lua:1571 |
| replay | Y | Y | n | 5 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:579 |
| reset | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:1135 |
| return_to_caller | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag.lua:528 |
| rollback | Y | Y | n | 12 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:389 |
| ruby | Y | Y | n | 7 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:1034 |
| s | Y | Y | n | 7 | ? | ? | ? | PARTIAL | scripts/kag.lua:303 |
| save | Y | Y | Y | 75 | ? | ? | ? | CLOSED | scripts/kag/commands/save.lua:243 |
| saveload | Y | Y | n | 9 | ? | ? | ? | PARTIAL | scripts/kag/commands/save.lua:495 |
| saveplace | Y | Y | n | 7 | ? | ? | ? | PARTIAL | scripts/kag/commands/save.lua:550 |
| scroll | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:215 |
| se | n | Y | n | 6 | ? | ? | ? | EXTRA | scripts/kag.lua:443 |
| sel | n | Y | n | 66 | ? | ? | ? | EXTRA | scripts/kag/commands/text.lua:1488 |
| select | Y | Y | n | 33 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1484 |
| set | Y | Y | n | 114 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:453 |
| setbgmvolume | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:356 |
| setsevolume | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:361 |
| setvoicevolume | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:366 |
| shake | Y | Y | n | 2 | ? | ? | ? | PARTIAL | scripts/kag.lua:417 |
| showtext | n | Y | Y | - | ? | ? | ? | EXTRA | scripts/kag.lua:206 |
| skip | Y | Y | n | 18 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1081 |
| sma_anim | Y | Y | n | - | ? | ? | ? | PARTIAL | scripts/kag/sma.lua:723 |
| sma_ik | Y | Y | n | - | ? | ? | ? | PARTIAL | scripts/kag/sma.lua:733 |
| sma_play | Y | Y | n | 6 | ? | ? | ? | PARTIAL | scripts/kag/sma.lua:712 |
| sma_stop | Y | Y | n | 4 | ? | ? | ? | PARTIAL | scripts/kag/sma.lua:747 |
| sma_variant | Y | Y | n | - | ? | ? | ? | PARTIAL | scripts/kag/sma.lua:741 |
| sprite_fade | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:466 |
| sprite_move | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:506 |
| sprite_scale | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:545 |
| sprite_swap | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:587 |
| steam_achievement | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/system.lua:791 |
| stopbgm | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:131 |
| stopse | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:229 |
| stopvideo | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/video.lua:110 |
| stopvoice | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:308 |
| sub | Y | Y | n | 8 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:122 |
| text | Y | Y | Y | 40 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:839 |
| textbox | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:381 |
| textspeed | Y | Y | n | 42 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1214 |
| trans | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:299 |
| tween | Y | Y | n | 14 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/tween.lua:201 |
| typewriter | Y | Y | n | - | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1272 |
| typewriter_sound | Y | Y | n | - | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:1286 |
| unlock | Y | Y | n | 50 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:399 |
| update | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/tween.lua:165 |
| vfx | Y | Y | Y | 11 | ? | ? | ? | CLOSED | scripts/kag/commands/vfx.lua:279 |
| vib | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:455 |
| vibrate | Y | Y | n | 13 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/vfx.lua:495 |
| video | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/video.lua:54 |
| voice | Y | Y | n | 4 | ? | ? | ? | PARTIAL | scripts/kag.lua:433 |
| voice_off | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:1125 |
| voice_wait | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag.lua:297 |
| wait | Y | Y | n | 63 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:50 |
| wait_click | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag.lua:323 |
| waitbgm | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:339 |
| waitclick | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag.lua:312 |
| waitforclick | Y | Y | n | 7 | ? | ? | ? | PARTIAL | scripts/kag.lua:388 |
| waitsound | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:327 |
| xfadebgm | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:194 |

## 人工覆盖（⚠）

- button — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：组合链（staging→render 分离系设计，架构注 :1297-1310）——button 注册本地化选项，endbutton cond 过滤+_renderChoices 绘制+blocking+命中跳转，间接真实触达 backend.render_text。
  - evidence：scripts/kag/commands/text.lua:1340-1372（注册 ctx._choiceButtons）→ :1374+ endbutton（_renderChoices 绘制+阻塞+跳转）→ TextScene draws → backend.render_text
- delay — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：别名链——delay=require(kag.commands.system).wait 同一实现（独立 schema 保 ms coercion），与 [wait] 帧流阻断语义完全一致。
  - evidence：scripts/kag.lua:337-347（require .wait + :343-345 裸位置防御）→ scripts/kag/commands/system.lua:50-83 wait（Operation+yield 帧流）
- emb — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：sandbox.execute/load 执行嵌段 + rawset(ctx.tf,emb_result)——ctx.tf 触达（t110 判据边缘形态，人工判真伪）；嵌段可经 env 触达任意能力。
  - evidence：scripts/kag/commands/system.lua:92-170（sandbox 执行 + rawset(ctx.tf,...)）
- endselect — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：别名=endbutton（同一实现），选择块完整链（t110 已核）。
  - evidence：scripts/kag/commands/text.lua:1490-1492（return TextCommands.endbutton）→ endbutton/_renderChoices → TextScene → backend.render_text
- eval — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：双轨——主轨 scheduler 内联（flow-inline，表达式求值入 ctx.tf.eval_result 可观察）；handler 为 strict 兜底（sandbox.execute + rawset(ctx.tf,...)——t110 判据边缘形态，人工判真伪）。
  - evidence：scripts/scheduler.lua:4/30/97-109（inline 主轨，"eval"=true）；scripts/kag/commands/system.lua:179-224（strict 兜底，rawset(ctx.tf,...)）
- i18n — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 PARTIAL 复核批人工核真：handler 经 i18n.set_language + kt.relocalize_page 全页重放，间接但真实触达渲染效果面（TextScene draws→backend.render_text），画面即时换语言可观察。
  - evidence：scripts/kag/commands/system.lua:685-712（contract+schema；i18n.set_language + kt.relocalize_page）；scripts/kag/commands/text.lua relocalize_page（全页重放）→ scripts/kag/text_scene.lua draws → 渲染循环 backend.render_text
- layout — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：handler 调用同文件工具函数 apply_container（:126-137 layers.move_layer 真实移动图层）——扫描器漏检形态（handler 体外本地函数触达），人工判真伪 VERIFIED。
  - evidence：scripts/kag/commands/layout.lua:169-199（handler）→ :126-137 apply_container（layers.move_layer）+ :118-123 recompute（math2.measure 槽位）
- layout_slot — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：同链——槽位注册→recompute+apply_container→layers.move_layer，真实图层重排。
  - evidence：scripts/kag/commands/layout.lua:204-238 → :126-137 apply_container（layers.move_layer）
- notify — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：toast.show 模块表调用，toast 模块内 backend.render_text + create_solid_texture + _toast_bg layer——间接真实触达（角标 toast 可观察）。
  - evidence：scripts/kag/commands/system.lua:648-677 → toast.show → scripts/toast.lua:41 backend.render_text + :14 create_solid_texture + :36 _toast_bg layer
- rollback — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：模块链——kag_runner.rollback() token 级快照弹出+重跑（下一次渲染反映，可观察倒带；blocking=true 契约）。
  - evidence：scripts/kag/commands/system.lua:389-397 → scripts/kag_runner.rollback()（快照恢复+重跑）
- select — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：语法糖组合——select no-op 开块（契约 blocking=false 设计如此），sel=button、endselect=endbutton 别名赋值，选择块完整语义=button/endbutton 链。
  - evidence：scripts/kag/commands/text.lua:1484-1486（开块）+ :1488 sel=button + :1491 endselect=endbutton → button/endbutton 链（见 button 条目）
- skip — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：ctx.skip_mode 状态写 + kag_runner 明确消费点（auto-advance/seen-skip）——状态写+消费点模式（textspeed 同款）。
  - evidence：scripts/kag/commands/text.lua:1081-1097（ctx.skip_mode 切换，seen off-toggle 审计修复注记）→ scripts/kag_runner.lua:467/482-483（消费）
- textspeed — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：apply_text_cps 写 ctx.text_speed（注释自证 real read point kag_runner），kag_runner.update 揭示速率消费——状态写+明确消费点，字符揭示速度变化可观察。
  - evidence：scripts/kag/commands/text.lua:1214-1216（handler）→ :1173-1198 apply_text_cps（ctx.text_speed=floor(1000/cps)）→ scripts/kag_runner.lua update()（揭示速率消费，reference :446-455）
- tween — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：工具函数链——resolve_layer/step_tween/apply_step 内 layers.move_layer/set_layer_opacity/mark_dirty，真实图层属性动画（blocking Operation；wait=false 由 kag_runner.update 驱动）。
  - evidence：scripts/kag/commands/tween.lua:201-254（handler）→ :58-62 resolve_layer（layers.get/find）+ :73-83 step_tween/apply_step（layers.move_layer/set_layer_opacity/mark_dirty）+ :165-186 update 驱动
- typewriter — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t101 全链证据：引擎侧逐字揭示（kag_runner.lua update() 计算 shown=floor(reveal.elapsed/speed) 并写入 text_scene reveal_chars，见 commands/text.lua:1235-1239 注释引用的 :446-455；text_scene.lua render() 按 reveal 截断每线条形 draw :232-266）——reveal 非 0 时字符逐个可见，语义闭环。
  - evidence：scripts/kag_runner.lua（update 揭示推进，参考 commands/text.lua:1235-1239）；scripts/kag/text_scene.lua:232-266（reveal 截断）；引擎 C++ TextRenderer（字形增量渲染，t101 已核）
- vibrate — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：委托链——trans.vib（transition.lua:455-476 layers.get_layer(message)+mark_dirty 消息层抖动）+ blocking 300ms。
  - evidence：scripts/kag/commands/vfx.lua:495-500 → kag.commands.transition trans.vib（transition.lua:455-476）

## 人工判级（范围外能力）

| 能力 | 判级 | 证据 | 备注 |
|---|---|---|---|
| letter_spacing | VERIFIED（位置级） | t105 接线：scripts/kag/text_scene.lua add_wrapped_spans 逐字符 draw（commit 33fb6b19）+ tests/scripts/test_text_markup.lua 9h/9i 共 9 断言（x 序列 100/116、段边界 158、多字节 127、typewriter×spacing） | 位置级已按布局口径（measure_character：rawWidth*scale+spacing）精确复现；像素级视觉待 M4 GPU 冒烟确认。布局级（wrap/measure）t101 已核真。 |
| SwipeDown | WIRED | native 消费方已接（commit 4c074c7b：Engine 钩子 _KAG_onKeySpace + scripts/kag_demo_entry.lua 实装 + InputRouter/MobileAdapter 接线；测试 +6：test_history.lua/test_sandbox.lua） | per-game opt-in 语义（钩子经 kag_demo_entry 实装=游戏侧显式接入才生效）；web=player 层全游戏生效（web/main.mjs:634-647）；真机手势 E2E 仍 hardware-gated（M4 平台矩阵）；后续方向=缺省钩子下沉 kag 运行时（backlog）。 |
| SwipeUp | WIRED | native 消费方已接（commit 4c074c7b：Engine 钩子 _KAG_onKeyPageUp + scripts/kag_demo_entry.lua 实装 + InputRouter/MobileAdapter 接线；测试 +6） | 同 SwipeDown（per-game opt-in；web player 层 main.mjs:634-647；真机 E2E hardware-gated；缺省钩子下沉=kag 运行时 backlog）。 |
| LongPress | VERIFIED | t101 复核 file:line 链（SDL_EVENT_FINGER_DOWN 长按检测 → InputRouter/驱动消费；详见 t101 task output） | t101 全链证据在案；native 手势链已接。 |
| Pinch | VERIFIED | t101 复核 file:line 链（双指缩放 → 驾驶消费；详见 t101 task output） | t101 全链证据在案。 |
| TwoFingerTap | VERIFIED | t101 复核 file:line 链（详见 t101 task output） | t101 全链证据在案。 |
| ThreeFingerHold | VERIFIED | t101 复核 file:line 链（详见 t101 task output） | t101 全链证据在案。 |

## UNWIRED

（无）

## PARTIAL

- add - scripts/kag/commands/math.lua:121；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- assert - scripts/kag/commands/system.lua:506；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- auto - scripts/kag/commands/text.lua:1111；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- bgm - scripts/kag.lua:490；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- blur - scripts/kag/commands/transition.lua:292；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- br - scripts/kag.lua:212；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- button - scripts/kag/commands/text.lua:1340；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- chapter - scripts/kag/commands/system.lua:349；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- cps - scripts/kag/commands/text.lua:1218；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- dec - scripts/kag/commands/math.lua:129；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- delay - scripts/kag.lua:337；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- div - scripts/kag/commands/math.lua:124；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- emb - scripts/kag/commands/system.lua:92；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- endbutton - scripts/kag/commands/text.lua:1374；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- ending - scripts/kag/commands/system.lua:365；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- endselect - scripts/kag/commands/text.lua:1490；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- eval - scripts/kag/commands/system.lua:179；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- fadeout - scripts/kag.lua:371；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- flash - scripts/kag/commands/vfx.lua:368；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- gallery - scripts/kag/commands/system.lua:333；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- history - scripts/kag/commands/system.lua:231；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- hr - scripts/kag.lua:217；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- i18n - scripts/kag/commands/system.lua:691；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- inc - scripts/kag/commands/system.lua:471；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- l - scripts/kag/commands/text.lua:906；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- layout - scripts/kag/commands/layout.lua:169；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- layout_slot - scripts/kag/commands/layout.lua:204；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- live2d_expression - scripts/kag/commands/character.lua:189；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- live2d_lip_sync - scripts/kag/commands/character.lua:200；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- live2d_motion - scripts/kag/commands/character.lua:177；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- loadplace - scripts/kag/commands/save.lua:554；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- mod - scripts/kag/commands/math.lua:125；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- mul - scripts/kag/commands/math.lua:123；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- music - scripts/kag/commands/system.lua:343；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- nameplate - scripts/kag/commands/text.lua:418；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- notify - scripts/kag/commands/system.lua:648；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- palette - scripts/kag/commands/vfx.lua:451；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- play - scripts/kag.lua:467；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- playstop - scripts/kag.lua:427；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- postprocess - scripts/kag/commands/vfx.lua:507；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- preload - scripts/kag/commands/resource.lua:154；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- pt - scripts/kag/commands/text.lua:1152；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- quake - scripts/kag.lua:421；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- r - scripts/kag.lua:288；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- random - scripts/kag/commands/system.lua:528；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- replay - scripts/kag/commands/system.lua:579；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- rollback - scripts/kag/commands/system.lua:389；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- ruby - scripts/kag/commands/text.lua:1034；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- s - scripts/kag.lua:303；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- saveload - scripts/kag/commands/save.lua:495；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- saveplace - scripts/kag/commands/save.lua:550；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- select - scripts/kag/commands/text.lua:1484；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- set - scripts/kag/commands/system.lua:453；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- shake - scripts/kag.lua:417；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- skip - scripts/kag/commands/text.lua:1081；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- sma_anim - scripts/kag/sma.lua:723；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- sma_ik - scripts/kag/sma.lua:733；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- sma_play - scripts/kag/sma.lua:712；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- sma_stop - scripts/kag/sma.lua:747；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- sma_variant - scripts/kag/sma.lua:741；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- sub - scripts/kag/commands/math.lua:122；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- textspeed - scripts/kag/commands/text.lua:1214；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- tween - scripts/kag/commands/tween.lua:201；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- typewriter - scripts/kag/commands/text.lua:1272；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- typewriter_sound - scripts/kag/commands/text.lua:1286；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- unlock - scripts/kag/commands/system.lua:399；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- vibrate - scripts/kag/commands/vfx.lua:495；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- voice - scripts/kag.lua:433；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- voice_off - scripts/kag/commands/text.lua:1125；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- voice_wait - scripts/kag.lua:297；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- wait - scripts/kag/commands/system.lua:50；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- waitclick - scripts/kag.lua:312；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）
- waitforclick - scripts/kag.lua:388；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）

## EXTRA

- Bezier - scripts/kag/commands/transition.lua:606；C:subtable-key（已注册但无合约条目）
- LUTCache - scripts/kag/commands/transition.lua:605；C:subtable-key（已注册但无合约条目）
- call - scripts/kag.lua:512；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- capture_state - scripts/kag/commands/save.lua:227；B:api-helper-export（已注册但无合约条目）
- clear - scripts/kag.lua:349；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- clearscreen - scripts/kag.lua:209；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- ct - scripts/kag.lua:352；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- end - scripts/kag.lua:86；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- endform - scripts/kag.lua:360；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- endmacro - scripts/kag.lua:242；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- endtag - scripts/kag.lua:356；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- erasemacro - scripts/kag.lua:243；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- flush_cache - scripts/kag/commands/resource.lua:254；B:api-helper-export（已注册但无合约条目）
- g - scripts/kag.lua:363；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- get_texture - scripts/kag/commands/resource.lua:226；B:api-helper-export（已注册但无合约条目）
- has_pending_transition - scripts/kag/commands/resource.lua:308；B:api-helper-export（已注册但无合约条目）
- is_loaded - scripts/kag/commands/resource.lua:235；B:api-helper-export（已注册但无合约条目）
- is_pending - scripts/kag/commands/resource.lua:244；B:api-helper-export（已注册但无合约条目）
- jump - scripts/kag.lua:501；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- macro - scripts/kag.lua:241；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- preload_transition - scripts/kag/commands/resource.lua:280；B:api-helper-export（已注册但无合约条目）
- promote_transition_slot - scripts/kag/commands/resource.lua:294；B:api-helper-export（已注册但无合约条目）
- push_backlog - scripts/kag/commands/text.lua:333；B:api-helper-export（已注册但无合约条目）
- relocalize_backlog - scripts/kag/commands/text.lua:1513；B:api-helper-export（已注册但无合约条目）
- relocalize_page - scripts/kag/commands/text.lua:1571；B:api-helper-export（已注册但无合约条目）
- return_to_caller - scripts/kag.lua:528；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- se - scripts/kag.lua:443；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- sel - scripts/kag/commands/text.lua:1488；B:api-helper-export（已注册但无合约条目）
- showtext - scripts/kag.lua:206；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）
- update - scripts/kag/commands/tween.lua:165；B:api-helper-export（已注册但无合约条目）
- wait_click - scripts/kag.lua:323；A:user-command-missing-contract（已注册但无合约条目）
      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）

## 私有辅助（_ 前缀，注册但不属命令面）

- _postfx - vfx.lua:274
- _relocalizeCC - text.lua:1554
- _relocalizeChoices - text.lua:1529
- _renderNameplate - text.lua:431
- _safeScenePath - save.lua:30

## 数据与判级局限（v2）

1. **Consumed 为调用形文本启发式**：注释与字符串字面量先被剥离（strip_lua 状态机），
   然后要求 backend./layers./kag. 的 <ident>( 调用形，或 ctx.tf. / ctx.tf[ / ctx.tf= 字段/赋值。
   仍可能低估（经本地别名或工具函数间接调用时本体不含直接调用：palette 命令经 palette 模块间接生效，
   如实归 PARTIAL），也可能高估（kag. 自派发计入）。脚本不执行 Lua。
2. **Dispatched 为静态解析**：commands 导出表函数/赋值键 + kag.lua 显式映射与 function KAG.x 定义 + 
   sma_commands 子表。jump/call/endmacro 等直接 API 计入 EXTRA(api-alias)；[jump]/[if] 等 token 的
   流控处理由 scheduler.lua 编译期内联；两条轨道并存，本扫描器按注册键计 Dispatched。
3. **Tested 为原始引用计数**：tests/scripts/*.lua 与 web/*.test.js 中 [<name> 或 kag.<name> 出现次数，
   不区分断言与非断言上下文（注释/数组/字符串也算）。
4. **Observable / Platform Tested / Packaged 首版为 ?**，可由 overrides JSON 人工覆盖（⚠ 标记）；
   平台运行矩阵/打包验证由 M4/release 验证工件补证。
5. 判级只依赖命令名静态匹配；同名异构（如 vfx 的 flash 与 transition 的 flash）以注册表实际键为准。
   导出表引用的子表（如 TransCommands.Bezier = Bezier）经 pairs() 一并注册为调度键——EXTRA
   (subtable-key)，非用户命令面。
6. 合约计数以 command-contracts.md 的 ### 条目数为准（表头标注 134 须一致）。
7. overrides JSON 的 commands 键必须落在已知命令名集合内；未知键被响亮拒绝（exit 非 0），
   绝不静默忽略。
8. **同文件工具函数/委托链内的效果面调用不被 v3 判据捕获**：v3 只扫 handler 直接体，
   handler 调用的同文件 local function（layout/layout_slot 的 apply_container、tween 的
   resolve_layer/step_tween、vibrate 委托的 trans.vib）内部的效果面调用属已核真形态——
   t110/t113 两批 18 命令人工核真中 5 例属此，由人工证据层（overrides）弥补；
   v4 候选=同文件 local function 一层调用穿透。

## 复现

```
python scripts/capability_closure.py
```

