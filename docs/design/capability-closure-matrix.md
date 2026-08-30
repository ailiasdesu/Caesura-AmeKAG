# Capability Closure Matrix (auto-generated)

> 由 python scripts/capability_closure.py 生成；勿手动编辑。
> 生成时间（输入源最新 mtime）：2026-08-30T02:47:04Z
> 生成命令：python scripts/capability_closure.py
> 源指纹（输入内容 sha256 前 16 hex）：6b9f0a3e42f4e77d
> 输出确定性：同源指纹同字节（generated_at 为输入源最新 mtime；跨机 checkout 的 mtime 差异属 by-design，确定性以指纹为准）

## 概述

- 合约总数（Declared，docs/api/command-contracts.md）：**134**
- 已注册（Dispatched）：**165**
- 触达效果面（Consumed，调用形上下文+一跳穿透 v4）：**89**
- 测试引用（Tested，启发式计数）：**137**
- UNWIRED：0 · PARTIAL：51 · CLOSED：79 · EXTRA：31 · EXPERIMENTAL(人工)：4
- **恒等式：**134 = CLOSED(79) + PARTIAL(51) + UNWIRED(0) + EXPERIMENTAL(在册 4)；165 = 134(Declared) + EXTRA(31) + EXPERIMENTAL(合约外 0)**

**范围声明（t103 MUST-FIX 3）**：本矩阵的 134 = 声明式 KAG 命令合约闭包（docs/api/command-contracts.md 全量条目）。下列能力**不在 134 内**：
- 原生手势链：SwipeDown / SwipeUp / LongPress / Pinch / TwoFingerTap / ThreeFingerHold（平台层）；
- 文本标记参数：letter_spacing / spacing / font / line_height 等内联标记（非命令）；
- KAG.* Lua API：jump/call/return_to_caller 等直接 API（注册键存在但非合约命令，见 EXTRA 的 api-alias）。
这些能力由矩阵底部「人工判级（范围外能力）」区段承载（docs/design/capability-closure-overrides.json 驱动）。
**EXTRA 属设计行为**：contracts 由声明式 schema registry（kag/schema.lua，经 scripts/schema_doc.lua 生成）产出；流控/API 命令按设计不在注册表——EXTRA 不是缺陷信号（A 类入册与否=产品决策待议）。

> 状态定义：**UNWIRED**=有合约无处理器；**PARTIAL**=已注册但处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中）；
> **CLOSED**=已注册且调用形触达效果面；**EXTRA**=已注册但无合约。
> **EXPERIMENTAL**=人工覆盖状态（能力存在但无消费方/无真实测试面——freeze 政策显式标注；见『EXPERIMENTAL』节与 overrides reason/note；机器判级仍为 PARTIAL/EXTRA）。
> ⚠ = 人工覆盖（docs/design/capability-closure-overrides.json；详见『人工覆盖』节）。

## Commands

| Command | Declared | Dispatched | Consumed | Tested | Observable | Platform Tested | Packaged | Status | 证据 |
|---|---|---|---|---|---|---|---|---|---|
| Bezier | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/transition.lua:606 |
| LUTCache | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/transition.lua:605 |
| add | Y | Y | n | 50 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:121 |
| ai_dialog | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/system.lua:714 |
| assert | Y | Y | n | 7 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:506 |
| auto | Y | Y | n | 6 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1111 |
| bg | Y | Y | Y | 21 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:120 |
| bgm | Y | Y | Y | 4 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag.lua:490 |
| blur | Y | Y | Y | 4 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/transition.lua:292 |
| br | Y | Y | n | 3 | VERIFIED（位置级） ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag.lua:212 |
| button | Y | Y | n | 38 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1340 |
| call | n | Y | n | 75 | ? | ? | ? | EXTRA | scripts/kag.lua:512 |
| camera | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:495 |
| cancel | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag.lua:220 |
| capture_state | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/save.lua:227 |
| ch | Y | Y | Y | 542 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:609 |
| chapter | Y | Y | Y | 3 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/system.lua:349 |
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
| end | n | Y | n | 208 | ? | ? | ? | EXTRA | scripts/kag.lua:86 |
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
| fadeout | Y | Y | n | 2 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag.lua:371 |
| fadevol | Y | Y | Y | - | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:104 |
| fg | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:156 |
| flash | Y | Y | Y | 6 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/vfx.lua:368 |
| flush_cache | n | Y | Y | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:254 |
| font | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:1067 |
| g | n | Y | n | 2 | ? | ? | ? | EXTRA | scripts/kag.lua:363 |
| gallery | Y | Y | Y | 6 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/system.lua:333 |
| get_texture | n | Y | Y | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:226 |
| has_pending_transition | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:308 |
| history | Y | Y | n | 9 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:231 |
| hr | Y | Y | n | 2 | ? | ? | ? | PARTIAL | scripts/kag.lua:217 |
| i18n | Y | Y | n | 60 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:691 |
| image | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:217 |
| inc | Y | Y | n | 29 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:471 |
| input | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:1630 |
| is_loaded | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:235 |
| is_pending | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:244 |
| jump | n | Y | n | 178 | ? | ? | ? | EXTRA | scripts/kag.lua:501 |
| l | Y | Y | Y | 4 | VERIFIED（位置级） ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/text.lua:906 |
| layfade | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:335 |
| layopt | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:311 |
| layout | Y | Y | Y | 26 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/layout.lua:169 |
| layout_place | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/layout.lua:243 |
| layout_slot | Y | Y | Y | 25 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/layout.lua:204 |
| ld | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag.lua:397 |
| listsaves | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/save.lua:523 |
| live2d_expression | Y | Y | n | - | ? ⚠ | ? ⚠ | ? ⚠ | EXPERIMENTAL | scripts/kag/commands/character.lua:189 |
| live2d_lip_sync | Y | Y | n | - | ? ⚠ | ? ⚠ | ? ⚠ | EXPERIMENTAL | scripts/kag/commands/character.lua:200 |
| live2d_motion | Y | Y | n | - | ? ⚠ | ? ⚠ | ? ⚠ | EXPERIMENTAL | scripts/kag/commands/character.lua:177 |
| load | Y | Y | Y | 63 | ? | ? | ? | CLOSED | scripts/kag/commands/save.lua:298 |
| loadplace | Y | Y | Y | 6 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/save.lua:554 |
| macro | n | Y | n | 56 | ? | ? | ? | EXTRA | scripts/kag.lua:241 |
| mod | Y | Y | n | 13 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:125 |
| move | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:387 |
| moveto | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:288 |
| mul | Y | Y | n | 11 | ? | ? | ? | PARTIAL | scripts/kag/commands/math.lua:123 |
| music | Y | Y | n | 3 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:343 |
| nameplate | Y | Y | n | 6 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:418 |
| notify | Y | Y | n | 38 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:648 |
| nvl | Y | Y | Y | 18 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:976 |
| p | Y | Y | Y | 282 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:947 |
| palette | Y | Y | Y | 26 | ? | ? | ? | PARTIAL | scripts/kag/commands/vfx.lua:451 |
| particle_weather | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/vfx.lua:589 |
| particles | Y | Y | Y | - | ? | ? | ? | CLOSED | scripts/kag/commands/vfx.lua:384 |
| play | Y | Y | Y | 6 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag.lua:467 |
| playbgm | Y | Y | Y | 16 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:109 |
| playbgmstop | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:148 |
| playse | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:210 |
| playstop | Y | Y | Y | 6 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag.lua:427 |
| playvoice | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:240 |
| position | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/layer.lua:297 |
| postprocess | Y | Y | Y | 3 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/vfx.lua:507 |
| postprocess_off | Y | Y | Y | 2 | ? | ? | ? | CLOSED | scripts/kag/commands/vfx.lua:519 |
| preload | Y | Y | Y | 11 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/resource.lua:154 |
| preload_transition | n | Y | Y | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:280 |
| promote_transition_slot | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/resource.lua:294 |
| pt | Y | Y | n | 11 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1152 |
| push_backlog | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/text.lua:333 |
| quake | Y | Y | n | 2 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag.lua:421 |
| r | Y | Y | Y | 3 | VERIFIED（位置级） ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag.lua:288 |
| random | Y | Y | n | 14 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:528 |
| relocalize_backlog | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/text.lua:1513 |
| relocalize_page | n | Y | Y | - | ? | ? | ? | EXTRA | scripts/kag/commands/text.lua:1571 |
| replay | Y | Y | n | 5 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:579 |
| reset | Y | Y | Y | 1 | ? | ? | ? | CLOSED | scripts/kag/commands/text.lua:1135 |
| return_to_caller | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag.lua:528 |
| rollback | Y | Y | n | 12 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/system.lua:389 |
| ruby | Y | Y | Y | 7 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/text.lua:1034 |
| s | Y | Y | n | 7 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag.lua:303 |
| save | Y | Y | Y | 71 | ? | ? | ? | CLOSED | scripts/kag/commands/save.lua:243 |
| saveload | Y | Y | n | 9 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/save.lua:495 |
| saveplace | Y | Y | n | 7 | ? | ? | ? | PARTIAL | scripts/kag/commands/save.lua:550 |
| scroll | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:215 |
| se | n | Y | Y | 6 | ? | ? | ? | EXTRA | scripts/kag.lua:443 |
| sel | n | Y | n | 66 | ? | ? | ? | EXTRA | scripts/kag/commands/text.lua:1488 |
| select | Y | Y | n | 33 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1484 |
| set | Y | Y | n | 113 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:453 |
| setbgmvolume | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:356 |
| setsevolume | Y | Y | Y | 6 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:361 |
| setvoicevolume | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:366 |
| shake | Y | Y | n | 2 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag.lua:417 |
| showtext | n | Y | Y | - | ? | ? | ? | EXTRA | scripts/kag.lua:206 |
| skip | Y | Y | n | 18 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1081 |
| sma_anim | Y | Y | n | - | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/sma.lua:723 |
| sma_ik | Y | Y | n | - | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/sma.lua:733 |
| sma_play | Y | Y | n | 6 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/sma.lua:712 |
| sma_stop | Y | Y | n | 4 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/sma.lua:747 |
| sma_variant | Y | Y | n | - | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/sma.lua:741 |
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
| tween | Y | Y | Y | 14 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/tween.lua:201 |
| typewriter | Y | Y | n | - | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag/commands/text.lua:1272 |
| typewriter_sound | Y | Y | n | - | ? ⚠ | ? ⚠ | ? ⚠ | EXPERIMENTAL | scripts/kag/commands/text.lua:1286 |
| unlock | Y | Y | n | 50 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:399 |
| update | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag/commands/tween.lua:165 |
| vfx | Y | Y | Y | 11 | ? | ? | ? | CLOSED | scripts/kag/commands/vfx.lua:279 |
| vib | Y | Y | Y | 5 | ? | ? | ? | CLOSED | scripts/kag/commands/transition.lua:455 |
| vibrate | Y | Y | Y | 13 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag/commands/vfx.lua:495 |
| video | Y | Y | Y | 4 | ? | ? | ? | CLOSED | scripts/kag/commands/video.lua:54 |
| voice | Y | Y | Y | 4 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | CLOSED | scripts/kag.lua:433 |
| voice_off | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag/commands/text.lua:1125 |
| voice_wait | Y | Y | n | 3 | VERIFIED ⚠ | ? ⚠ | ? ⚠ | PARTIAL | scripts/kag.lua:297 |
| wait | Y | Y | n | 63 | ? | ? | ? | PARTIAL | scripts/kag/commands/system.lua:50 |
| wait_click | n | Y | n | - | ? | ? | ? | EXTRA | scripts/kag.lua:323 |
| waitbgm | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:339 |
| waitclick | Y | Y | n | 3 | ? | ? | ? | PARTIAL | scripts/kag.lua:312 |
| waitforclick | Y | Y | n | 7 | ? | ? | ? | PARTIAL | scripts/kag.lua:388 |
| waitsound | Y | Y | Y | 7 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:327 |
| xfadebgm | Y | Y | Y | 3 | ? | ? | ? | CLOSED | scripts/kag/commands/audio.lua:194 |

## 人工覆盖（⚠）

- auto — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t116 复核：ctx.auto_mode 状态写+明确消费点（kag_runner 自动前进）
  - evidence：scripts/kag/commands/text.lua:1111-1116（ctx.auto_mode）→ scripts/kag_runner.lua:511（auto 消费）
- bgm — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：别名链 KAG.bgm=KAG.play→play 的 backend 链
  - evidence：scripts/kag.lua:490（KAG.bgm=KAG.play）→ play（见 play 条目）→ backend.audio_*
- blur — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：模块委托链 VFX.blur→rtt.alloc GPU blur（headless 无 GPU 降级注记）
  - evidence：scripts/kag/commands/transition.lua:292 → scripts/vfx.lua:183 VFX.blur（rtt.alloc GPU blur）
- br — Observable=VERIFIED（位置级） · PlatformTested=? · Packaged=?
  - reason：t119 复核：自派发链 br→KAG.l（t117 位置级已核）——行断效果；照 l 先例用位置级口径
  - evidence：scripts/kag.lua:212-214（function KAG.br → KAG.l(ctx,params)）→ l（text.lua:906 位置级）
- button — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：组合链（staging→render 分离系设计，架构注 :1297-1310）——button 注册本地化选项，endbutton cond 过滤+_renderChoices 绘制+blocking+命中跳转，间接真实触达 backend.render_text。
  - evidence：scripts/kag/commands/text.lua:1340-1372（注册 ctx._choiceButtons）→ :1374+ endbutton（_renderChoices 绘制+阻塞+跳转）→ TextScene draws → backend.render_text
- chapter — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：模块链+状态-流链——ChapterSelect.show（层系统+render_text）→ctx._pendingJump runner 消费
  - evidence：scripts/kag/commands/system.lua:349-361 → scripts/chapter_select.lua:51 layers.ensure/_chapter_bg + :59/:69 backend.render_text → ctx._pendingJump
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
- fadeout — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：模块委托 Layer.layfade（opacity 0..1→0..255 换算注记）→层透明度动画
  - evidence：scripts/kag.lua:371-385 → scripts/kag/commands/layer.lua layfade
- flash — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t116 复核：模块表委托 VFX.flash→backend.create_solid_texture+__flash 层，阻塞全屏闪白
  - evidence：scripts/kag/commands/vfx.lua:368-370 → scripts/vfx.lua:266-320 VFX.flash（backend.create_solid_texture :314 + __flash 层 :286-292, z=9998）
- gallery — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：模块委托链 gallery.lua show→backend.set_input_focus/get_resolution+layers.ensure 覆盖层，UI 全链路通过
  - evidence：scripts/kag/commands/system.lua:333 → scripts/gallery.lua:97 show → :133 backend.set_input_focus(GAME) / :135 backend.get_resolution / :143 layers.ensure(_gallery_overlay,95)
- history — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t116 复核：HistoryUI.show 模块调用→backlog 覆盖层（层系统+backend 渲染），真实可观察
  - evidence：scripts/kag/commands/system.lua:231-241 → scripts/history_ui.lua :19 backend.create_solid_texture / :27-31 layers.get / :86-87 layers.ensure(_history_bg/_history_title)+注释 :9 自证 backend.render_text；jump→ctx._pendingJump
- i18n — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 PARTIAL 复核批人工核真：handler 经 i18n.set_language + kt.relocalize_page 全页重放，间接但真实触达渲染效果面（TextScene draws→backend.render_text），画面即时换语言可观察。
  - evidence：scripts/kag/commands/system.lua:685-712（contract+schema；i18n.set_language + kt.relocalize_page）；scripts/kag/commands/text.lua relocalize_page（全页重放）→ scripts/kag/text_scene.lua draws → 渲染循环 backend.render_text
- l — Observable=VERIFIED（位置级） · PlatformTested=? · Packaged=?
  - reason：t117 复核：行断语义=cursor 状态（textCursorY/X + text_scene cursor + update_text_state(l)）→渲染循环消费；位置级与 letter_spacing 同类（像素级待 M4）
  - evidence：scripts/kag/commands/text.lua:906（ctx.textCursorY/X + TextScene.get_state().cursor_x/y + update_text_state(l)）→ scripts/kag/text_scene.lua:42-43/65-66（渲染循环 cursor 消费）
- layout — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：handler 调用同文件工具函数 apply_container（:126-137 layers.move_layer 真实移动图层）——扫描器漏检形态（handler 体外本地函数触达），人工判真伪 VERIFIED。
  - evidence：scripts/kag/commands/layout.lua:169-199（handler）→ :126-137 apply_container（layers.move_layer）+ :118-123 recompute（math2.measure 槽位）
- layout_slot — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：同链——槽位注册→recompute+apply_container→layers.move_layer，真实图层重排。
  - evidence：scripts/kag/commands/layout.lua:204-238 → :126-137 apply_container（layers.move_layer）
- live2d_expression — Observable=? · PlatformTested=? · Packaged=? · Status=EXPERIMENTAL
  - reason：t119 判级：写 ctx.live2d[model].expression 状态；LIVE2D=OFF 无消费+Tested=0——feature-gated
  - evidence：scripts/kag/commands/character.lua:189-197
- live2d_lip_sync — Observable=? · PlatformTested=? · Packaged=? · Status=EXPERIMENTAL
  - reason：t119 判级：写 ctx.live2d[model].lip_sync 状态；LIVE2D=OFF 无消费+Tested=0——feature-gated
  - evidence：scripts/kag/commands/character.lua:200-207
- live2d_motion — Observable=? · PlatformTested=? · Packaged=? · Status=EXPERIMENTAL
  - reason：t119 判级：handler 仅写 ctx.live2d[model].current_motion 状态；本构建 CAESURA_LIVE2D=OFF（NullAnimation）无消费方+Tested=0——feature-gated
  - evidence：scripts/kag/commands/character.lua:177-186
- loadplace — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：状态-流链——ctx._pendingJump+stop_flag→runner 跳转路径（bookmark 恢复可观察；test_flow_edge_call.lua:363-381 有覆盖）
  - evidence：scripts/kag/commands/save.lua:554-556 → scripts/system.lua:334-345 loadplace（ctx._pendingJump={scene,index}+ctx.stop_flag=true）→ runner 跳转
- music — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：模块委托 music_room.show（UI 全链：solid texture/render_text/get_resolution）
  - evidence：scripts/kag/commands/system.lua:343-345 → scripts/music_room.lua:21 create_solid_texture + :168 render_text + :172 get_resolution
- nameplate — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：同文件私有工具函数 _renderNameplate 内 layers./backend. 直呼——_ 前缀私有函数调用图盲区（v4 B 类）
  - evidence：scripts/kag/commands/text.lua:418（handler）→ :431 _renderNameplate（layers.ensure(_nameplate,3)+backend.create_solid_texture+backend.render_text+layers.mark_dirty）
- notify — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：toast.show 模块表调用，toast 模块内 backend.render_text + create_solid_texture + _toast_bg layer——间接真实触达（角标 toast 可观察）。
  - evidence：scripts/kag/commands/system.lua:648-677 → toast.show → scripts/toast.lua:41 backend.render_text + :14 create_solid_texture + :36 _toast_bg layer
- play — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：模块委托链 kag.commands.audio playbgm/playse/playvoice→backend.audio_*
  - evidence：scripts/kag.lua:467-500 play（路由到 audio 模块）→ scripts/kag/commands/audio.lua playbgm:109/playse/playvoice → backend.audio_play
- playstop — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：模块委托链 audio.stopbgm→backend
  - evidence：scripts/kag.lua:427-432 playstop（→ audio.stopbgm）→ scripts/kag/commands/audio.lua stopbgm → backend.audio_stop
- postprocess — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：同文件工具函数 apply_postfx→backend.set_postfx/clear_postfx/is_postfx_supported——GPU postfx 可观察（同文件工具链盲区；ctx._postfx bookkeeping NOT-WIRED 注记属持久化缺口）
  - evidence：scripts/kag/commands/vfx.lua:507-515 → :230-271 apply_postfx（backend.set_postfx :258 / clear_postfx :232 / is_postfx_supported :236）
- preload — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t116 复核：同文件工具链——load_texture/load_audio 内 backend 直呼（真实资源预加载+占位符），扫描器漏检=同文件工具函数（局限8）
  - evidence：scripts/kag/commands/resource.lua:154-217（handler）→ :78-107 load_texture（backend.load_texture :97 / backend.load_texture_async :85）+ :113-131 load_audio（backend.audio_play/audio_stop :120/123）+ scene→flow.load_scene :201
- pt — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t116 复核：ctx.text_speed 状态写+注释自证消费点（kag_runner 每帧读取推进 reveal），textspeed 同款模式
  - evidence：scripts/kag/commands/text.lua:1152-1154（ctx.text_speed=params.speed）+ :1156-1170（注释自证 kag_runner.lua 消费）
- quake — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：模块委托 vfx.quake（修正绑定注记：standalone [quake] 不再跑 shake）
  - evidence：scripts/kag.lua:421-424 → scripts/kag/commands/vfx.lua:376-378 → scripts/vfx.lua:28 VFX.quake
- r — Observable=VERIFIED（位置级） · PlatformTested=? · Packaged=?
  - reason：t119 复核：别名链 KAG.r=KAG.l or KAG.br → l/br（位置级）
  - evidence：scripts/kag.lua:288（KAG.r = KAG.l or KAG.br）→ l/br 位置级链
- replay — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：模块委托链 replay.load→state.mode=playback→kag_runner replay.tick 回放推进（可观察）
  - evidence：scripts/kag/commands/system.lua:579 → scripts/replay.lua:140 load（mode=playback）→ scripts/kag_runner.lua:441/443 replay.tick(delta_ms, click_cb)
- rollback — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：模块链——kag_runner.rollback() token 级快照弹出+重跑（下一次渲染反映，可观察倒带；blocking=true 契约）。
  - evidence：scripts/kag/commands/system.lua:389-397 → scripts/kag_runner.rollback()（快照恢复+重跑）
- ruby — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t116 复核：TextScene.add_ruby→draws→render 循环 backend.render_ruby，注音标注可观察
  - evidence：scripts/kag/commands/text.lua:1034-1059 → scripts/kag/text_scene.lua:179-214 add_ruby + :250-253 render_backend.render_ruby
- s — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t116 复核：别名链=wait 实现（KAG3 s 字符快捷等待默认 250ms），delay 同款
  - evidence：scripts/kag.lua:303-308（require kag.commands.system.wait, ms 默认 250）→ scripts/kag/commands/system.lua:50-83 wait
- saveload — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t116 复核：saveload_menu.show UI 覆盖层→结果 save/load 真实读写；headless pcall 降级注记
  - evidence：scripts/kag/commands/save.lua:495-521（:498-511 headless 降级；:512 SaveLoad.show；:515-517 SaveCommands.save/load）
- select — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：语法糖组合——select no-op 开块（契约 blocking=false 设计如此），sel=button、endselect=endbutton 别名赋值，选择块完整语义=button/endbutton 链。
  - evidence：scripts/kag/commands/text.lua:1484-1486（开块）+ :1488 sel=button + :1491 endselect=endbutton → button/endbutton 链（见 button 条目）
- shake — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：模块委托 vfx.shake→VFX.shake（层动画）
  - evidence：scripts/kag.lua:417-420 → scripts/kag/commands/vfx.lua:372-374 → scripts/vfx.lua:82 VFX.shake
- skip — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：ctx.skip_mode 状态写 + kag_runner 明确消费点（auto-advance/seen-skip）——状态写+消费点模式（textspeed 同款）。
  - evidence：scripts/kag/commands/text.lua:1081-1097（ctx.skip_mode 切换，seen off-toggle 审计修复注记）→ scripts/kag_runner.lua:467/482-483（消费）
- sma_anim — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：sma.play_anim→actor 状态→sma.render（binding().draw_mesh）模块内状态-消费闭环；测试 0 引用但消费链真实
  - evidence：scripts/kag/sma.lua:723-731 → :601 sma.play_anim → :540-556 sma.render（binding().draw_mesh，t117 已核）
- sma_ik — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：sma.set_ik→actor IK 字段→sma.render 消费（绑定接口渲染）
  - evidence：scripts/kag/sma.lua:733-739 → :630 sma.set_ik → sma.render:540-556 消费
- sma_play — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t117 复核：绑定接口链 sma.spawn→sma.render binding().draw_mesh（GPU 网格渲染）；扫描器盲区 D 类（binding. 非 backend./layers. token）
  - evidence：scripts/kag/sma.lua:712 sma_play → :392 sma.spawn（ctx.sma_actors 状态）→ :540-556 sma.render binding().draw_mesh(handle,view,texId,...)
- sma_stop — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：sma.despawn 内 binding().destroy_mesh 绑定接口直呼——网格销毁可观察（绑定接口链盲区）
  - evidence：scripts/kag/sma.lua:747-749 → :442-452 sma.despawn（binding().destroy_mesh :448-449）
- sma_variant — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：sma.set_variant→actor variant/tex→sma.render 消费
  - evidence：scripts/kag/sma.lua:741-745 → :652 sma.set_variant → sma.render:540-556 消费
- textspeed — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t110 复核：apply_text_cps 写 ctx.text_speed（注释自证 real read point kag_runner），kag_runner.update 揭示速率消费——状态写+明确消费点，字符揭示速度变化可观察。
  - evidence：scripts/kag/commands/text.lua:1214-1216（handler）→ :1173-1198 apply_text_cps（ctx.text_speed=floor(1000/cps)）→ scripts/kag_runner.lua update()（揭示速率消费，reference :446-455）
- tween — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：工具函数链——resolve_layer/step_tween/apply_step 内 layers.move_layer/set_layer_opacity/mark_dirty，真实图层属性动画（blocking Operation；wait=false 由 kag_runner.update 驱动）。
  - evidence：scripts/kag/commands/tween.lua:201-254（handler）→ :58-62 resolve_layer（layers.get/find）+ :73-83 step_tween/apply_step（layers.move_layer/set_layer_opacity/mark_dirty）+ :165-186 update 驱动
- typewriter — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t101 全链证据：引擎侧逐字揭示（kag_runner.lua update() 计算 shown=floor(reveal.elapsed/speed) 并写入 text_scene reveal_chars，见 commands/text.lua:1235-1239 注释引用的 :446-455；text_scene.lua render() 按 reveal 截断每线条形 draw :232-266）——reveal 非 0 时字符逐个可见，语义闭环。
  - evidence：scripts/kag_runner.lua（update 揭示推进，参考 commands/text.lua:1235-1239）；scripts/kag/text_scene.lua:232-266（reveal 截断）；引擎 C++ TextRenderer（字形增量渲染，t101 已核）
- typewriter_sound — Observable=? · PlatformTested=? · Packaged=? · Status=EXPERIMENTAL
  - reason：t119 判级：WRITE-ONLY 声音配置——源码自证（text.lua:1226-1232 nothing plays a sound when a character is revealed / must not be described as working），配置三键全仓无读者，Tested=0
  - evidence：scripts/kag/commands/text.lua:1286（TextCommands.typewriter_sound = TextCommands.typewriter）→ :1272-1285 写入 ctx.typewriter_sound/_interval/_volume；无读者（:1226-1232 自证）
- vibrate — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t113 复核：委托链——trans.vib（transition.lua:455-476 layers.get_layer(message)+mark_dirty 消息层抖动）+ blocking 300ms。
  - evidence：scripts/kag/commands/vfx.lua:495-500 → kag.commands.transition trans.vib（transition.lua:455-476）
- voice — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：模块委托 audio.playvoice（schema.coerce play 迁移条件注记）→backend.audio_play（voice 轨）
  - evidence：scripts/kag.lua:433-440 → scripts/kag/commands/audio.lua playvoice → backend.audio_play
- voice_wait — Observable=VERIFIED · PlatformTested=? · Packaged=?
  - reason：t119 复核：模块委托 audio.voice_wait（CLOSED 链）→等待语音完成+点击跳过
  - evidence：scripts/kag.lua:297-299 → scripts/kag/commands/audio.lua voice_wait

## 人工判级（范围外能力）

| 能力 | 判级 | 证据 | 备注 |
|---|---|---|---|
| letter_spacing | VERIFIED（位置级） | t105 接线：scripts/kag/text_scene.lua add_wrapped_spans 逐字符 draw（commit 33fb6b19）+ tests/scripts/test_text_markup.lua 9h/9i 共 9 断言（x 序列 100/116、段边界 158、多字节 127、typewriter×spacing） | 位置级已按布局口径（measure_character：rawWidth*scale+spacing）精确复现；像素级视觉待 M4 GPU 冒烟确认。布局级（wrap/measure）t101 已核真。 |
| SwipeDown | WIRED | native 消费方已接（commit 4c074c7b：Engine 钩子 _KAG_onKeySpace + scripts/kag_demo_entry.lua 实装 + InputRouter/MobileAdapter 接线；测试 +6：test_history.lua/test_sandbox.lua） | 缺省钩子已下沉 kag 运行时（t125/t130 M-F1）：scripts/kag.lua:553-567 defaultKeySpace（handler 级 pcall require layers + type 守卫，:561-562；layers.get(message) 可见性切换 :563-566）经 KAG.gesture_defaults 导出（:586-589）+ 全局安装 _KAG_onKeySpace（:591-592，first-definition-wins），Engine 钩子路由 SDLK_SPACE；kag_runner 每帧 overlay pump（t127 M-F2，kag_runner.lua:467-481 驱动 ctx._gesture_history_co 死槽清除）。入口覆写优先（kag_demo_entry.lua 为官方覆写示例）——native 缺省行为与 web player 层全局手势（web/main.mjs:634-647）平权；真机手势 E2E 仍 hardware-gated（M4）；per-game opt-in 语义已由运行时缺省取代。 |
| SwipeUp | WIRED | native 消费方已接（commit 4c074c7b：Engine 钩子 _KAG_onKeyPageUp + scripts/kag_demo_entry.lua 实装 + InputRouter/MobileAdapter 接线；测试 +6） | 缺省钩子已下沉 kag 运行时（t125/t130 M-F1）：scripts/kag.lua:569-575 defaultKeyPageUp（input_focus 守卫 + ctx._gesture_history_co 单飞协程 :571-574 + kag.commands.system.history）+ KAG.gesture_defaults:586-589 + _KAG_onKeyPageUp 全局安装 :591-592（first-definition-wins），Engine 钩子路由 SDLK_PAGEUP；kag_runner overlay pump（kag_runner.lua:467-481）。入口覆写优先（kag_demo_entry.lua 官方示例）——与 web player 层（main.mjs:634-647）平权；真机 E2E hardware-gated（M4）。 |
| LongPress | VERIFIED | t101 复核 file:line 链（SDL_EVENT_FINGER_DOWN 长按检测 → InputRouter/驱动消费；详见 t101 task output） | t101 全链证据在案；native 手势链已接。 |
| Pinch | VERIFIED | t101 复核 file:line 链（双指缩放 → 驾驶消费；详见 t101 task output） | t101 全链证据在案。 |
| TwoFingerTap | VERIFIED | t101 复核 file:line 链（详见 t101 task output） | t101 全链证据在案。 |
| ThreeFingerHold | VERIFIED | t101 复核 file:line 链（详见 t101 task output） | t101 全链证据在案。 |

## EXPERIMENTAL

- live2d_expression - 人工覆盖状态 EXPERIMENTAL（能力存在但无消费方/无真实测试面）；机器判级：PARTIAL（合约内）
  - reason：t119 判级：写 ctx.live2d[model].expression 状态；LIVE2D=OFF 无消费+Tested=0——feature-gated
  - evidence：scripts/kag/commands/character.lua:189-197
  - note：同 live2d_motion（M4 特性矩阵 Live2D 行）。
- live2d_lip_sync - 人工覆盖状态 EXPERIMENTAL（能力存在但无消费方/无真实测试面）；机器判级：PARTIAL（合约内）
  - reason：t119 判级：写 ctx.live2d[model].lip_sync 状态；LIVE2D=OFF 无消费+Tested=0——feature-gated
  - evidence：scripts/kag/commands/character.lua:200-207
  - note：同 live2d_motion（M4 特性矩阵 Live2D 行）。
- live2d_motion - 人工覆盖状态 EXPERIMENTAL（能力存在但无消费方/无真实测试面）；机器判级：PARTIAL（合约内）
  - reason：t119 判级：handler 仅写 ctx.live2d[model].current_motion 状态；本构建 CAESURA_LIVE2D=OFF（NullAnimation）无消费方+Tested=0——feature-gated
  - evidence：scripts/kag/commands/character.lua:177-186
  - note：CAESURA_LIVE2D=ON 构建下由 Live2D 后端消费并按 M4 特性矩阵重判。
- typewriter_sound - 人工覆盖状态 EXPERIMENTAL（能力存在但无消费方/无真实测试面）；机器判级：PARTIAL（合约内）
  - reason：t119 判级：WRITE-ONLY 声音配置——源码自证（text.lua:1226-1232 nothing plays a sound when a character is revealed / must not be described as working），配置三键全仓无读者，Tested=0
  - evidence：scripts/kag/commands/text.lua:1286（TextCommands.typewriter_sound = TextCommands.typewriter）→ :1272-1285 写入 ctx.typewriter_sound/_interval/_volume；无读者（:1226-1232 自证）
  - note：建议接线点：kag_runner reveal 推进处（text.lua:1235-1239 已指向 :446-455）读 _interval/_volume。

## 可疑翻转清单（v4 保守维持；含队长裁决）

- palette — v3 PARTIAL → v4 CLOSED；**已裁决：维持 PARTIAL**（理由与 v5 候选见下）
  - reason：v4 穿透命中的 backend.set_palette 为幻影绑定——palette.lua:10-11 自证「C++ 侧无 LUT 绑定，set_palette/destroy_texture 未接线（src 与 web bridge 均无）」；真实效果面仅 backend.load_image（LUT 图加载，palette.lua:44）。apply/clear 经 lut_available() 守卫降级为可见 no-op（palette.lua:14-24）——全链半程：加载真实/应用未接线，按裁决维持 PARTIAL。v5 候选：模块穿透落点的 backend.* 名须对照真实绑定面校验（防幻影绑定翻绿）。

## UNWIRED

（无）

## PARTIAL

- add - scripts/kag/commands/math.lua:121；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- assert - scripts/kag/commands/system.lua:506；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- auto - scripts/kag/commands/text.lua:1111；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- br - scripts/kag.lua:212；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- button - scripts/kag/commands/text.lua:1340；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- cps - scripts/kag/commands/text.lua:1218；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- dec - scripts/kag/commands/math.lua:129；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- delay - scripts/kag.lua:337；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- div - scripts/kag/commands/math.lua:124；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- emb - scripts/kag/commands/system.lua:92；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- endbutton - scripts/kag/commands/text.lua:1374；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- ending - scripts/kag/commands/system.lua:365；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- endselect - scripts/kag/commands/text.lua:1490；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- eval - scripts/kag/commands/system.lua:179；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- fadeout - scripts/kag.lua:371；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- history - scripts/kag/commands/system.lua:231；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- hr - scripts/kag.lua:217；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- i18n - scripts/kag/commands/system.lua:691；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- inc - scripts/kag/commands/system.lua:471；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- mod - scripts/kag/commands/math.lua:125；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- mul - scripts/kag/commands/math.lua:123；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- music - scripts/kag/commands/system.lua:343；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- nameplate - scripts/kag/commands/text.lua:418；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- notify - scripts/kag/commands/system.lua:648；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- palette - scripts/kag/commands/vfx.lua:451；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- pt - scripts/kag/commands/text.lua:1152；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- quake - scripts/kag.lua:421；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- random - scripts/kag/commands/system.lua:528；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- replay - scripts/kag/commands/system.lua:579；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- rollback - scripts/kag/commands/system.lua:389；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- s - scripts/kag.lua:303；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- saveload - scripts/kag/commands/save.lua:495；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- saveplace - scripts/kag/commands/save.lua:550；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- select - scripts/kag/commands/text.lua:1484；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- set - scripts/kag/commands/system.lua:453；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- shake - scripts/kag.lua:417；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- skip - scripts/kag/commands/text.lua:1081；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- sma_anim - scripts/kag/sma.lua:723；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- sma_ik - scripts/kag/sma.lua:733；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- sma_play - scripts/kag/sma.lua:712；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- sma_stop - scripts/kag/sma.lua:747；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- sma_variant - scripts/kag/sma.lua:741；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- sub - scripts/kag/commands/math.lua:122；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- textspeed - scripts/kag/commands/text.lua:1214；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- typewriter - scripts/kag/commands/text.lua:1272；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- unlock - scripts/kag/commands/system.lua:399；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- voice_off - scripts/kag/commands/text.lua:1125；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- voice_wait - scripts/kag.lua:297；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- wait - scripts/kag/commands/system.lua:50；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- waitclick - scripts/kag.lua:312；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）
- waitforclick - scripts/kag.lua:388；处理器体未以调用形触达效果面（v4 一跳穿透下仍无命中；注释/字符串已剥离）

## EXTRA

- Bezier - scripts/kag/commands/transition.lua:606；B:api-helper-export（已注册但无合约条目）
- LUTCache - scripts/kag/commands/transition.lua:605；B:api-helper-export（已注册但无合约条目）
- call - scripts/kag.lua:512；A:user-command-missing-contract（已注册但无合约条目）
- capture_state - scripts/kag/commands/save.lua:227；B:api-helper-export（已注册但无合约条目）
- clear - scripts/kag.lua:349；A:user-command-missing-contract（已注册但无合约条目）
- clearscreen - scripts/kag.lua:209；A:user-command-missing-contract（已注册但无合约条目）
- ct - scripts/kag.lua:352；A:user-command-missing-contract（已注册但无合约条目）
- end - scripts/kag.lua:86；A:user-command-missing-contract（已注册但无合约条目）
- endform - scripts/kag.lua:360；A:user-command-missing-contract（已注册但无合约条目）
- endmacro - scripts/kag.lua:242；A:user-command-missing-contract（已注册但无合约条目）
- endtag - scripts/kag.lua:356；A:user-command-missing-contract（已注册但无合约条目）
- erasemacro - scripts/kag.lua:243；A:user-command-missing-contract（已注册但无合约条目）
- flush_cache - scripts/kag/commands/resource.lua:254；B:api-helper-export（已注册但无合约条目）
- g - scripts/kag.lua:363；A:user-command-missing-contract（已注册但无合约条目）
- get_texture - scripts/kag/commands/resource.lua:226；B:api-helper-export（已注册但无合约条目）
- has_pending_transition - scripts/kag/commands/resource.lua:308；B:api-helper-export（已注册但无合约条目）
- is_loaded - scripts/kag/commands/resource.lua:235；B:api-helper-export（已注册但无合约条目）
- is_pending - scripts/kag/commands/resource.lua:244；B:api-helper-export（已注册但无合约条目）
- jump - scripts/kag.lua:501；A:user-command-missing-contract（已注册但无合约条目）
- macro - scripts/kag.lua:241；A:user-command-missing-contract（已注册但无合约条目）
- preload_transition - scripts/kag/commands/resource.lua:280；B:api-helper-export（已注册但无合约条目）
- promote_transition_slot - scripts/kag/commands/resource.lua:294；B:api-helper-export（已注册但无合约条目）
- push_backlog - scripts/kag/commands/text.lua:333；B:api-helper-export（已注册但无合约条目）
- relocalize_backlog - scripts/kag/commands/text.lua:1513；B:api-helper-export（已注册但无合约条目）
- relocalize_page - scripts/kag/commands/text.lua:1571；B:api-helper-export（已注册但无合约条目）
- return_to_caller - scripts/kag.lua:528；A:user-command-missing-contract（已注册但无合约条目）
- se - scripts/kag.lua:443；A:user-command-missing-contract（已注册但无合约条目）
- sel - scripts/kag/commands/text.lua:1488；B:api-helper-export（已注册但无合约条目）
- showtext - scripts/kag.lua:206；A:user-command-missing-contract（已注册但无合约条目）
- update - scripts/kag/commands/tween.lua:165；B:api-helper-export（已注册但无合约条目）
- wait_click - scripts/kag.lua:323；A:user-command-missing-contract（已注册但无合约条目）

## 私有辅助（_ 前缀，注册但不属命令面）

- _postfx - vfx.lua:274
- _relocalizeCC - text.lua:1554
- _relocalizeChoices - text.lua:1529
- _renderNameplate - text.lua:431
- _safeScenePath - save.lua:30

## 数据与判级局限（v4）

1. **Consumed 为调用形文本启发式**：注释与字符串字面量先被剥离（strip_lua 状态机），要求 backend./layers./kag. 的 <ident>( 调用形，或 ctx.tf. / ctx.tf[ / ctx.tf= 字段/赋值。仍可能低估（经本地别名或工具函数间接调用时本体不含直接调用；如 palette 命令经 palette 模块间接生效——如实归 PARTIAL），也可能高估（kag. 自派发计入）。脚本不执行 Lua，无法做数据流分析。
2. **Dispatched 为静态解析**：commands 导出表函数/赋值键 + kag.lua 显式映射与 function KAG.x 定义 + sma_commands 子表。jump/call/endmacro 等直接 API 计入 EXTRA(api-alias)；[jump]/[if] 等 token 的流控处理由 scheduler.lua 编译期内联；两条轨道并存，本扫描器按注册键计 Dispatched。
3. **Tested 为原始引用计数**：tests/scripts/*.lua 与 web/*.test.js 中 [<name> 或 kag.<name> 出现次数，不区分断言与非断言上下文（注释/数组/字符串也算）。
4. **Observable / Platform Tested / Packaged 首版为 ?**，可由 overrides JSON 人工覆盖（⚠ 标记）；平台运行矩阵/打包验证由 M4/release 验证工件补证。
5. 判级只依赖命令名静态匹配；同名异构（如 vfx 的 flash 与 transition 的 flash）以注册表实际键为准。导出表引用的子表（如 TransCommands.Bezier = Bezier）经 pairs() 一并注册为调度键——EXTRA(subtable-key)，非用户命令面。
6. 合约计数以 command-contracts.md 的 ### 条目数为准（表头标注 134 须一致）。
7. overrides JSON 的 commands 键必须落在已知命令名集合内；未知键被响亮拒绝（exit 非 0），绝不静默忽略。
8. **v4 已修复（历史注记保留）**：v3 判据只扫 handler 直接体——同文件工具函数/委托链内的效果面调用（t110-t119 五批人工核真 18+ 例：layout/layout_slot/tween/vibrate/nameplate 的工具函数链、模块表委托 toast.show/VFX.flash/HistoryUI.show 等）不被捕获；v4 一跳穿透（同文件 local + require()d 模块函数）已覆盖该盲区。仍存在的判定噪声：跨两跳以上的链（工具函数再调工具函数）、绑定接口（binding().draw_mesh 类——sma_play 等经人工证据层覆盖）、rawset(ctx.tf, ...) 形态（判据边缘）。

## 复现

```
python scripts/capability_closure.py
```

