# 《单程回信》
# The One-Way Reply
### Caesura (AmeKAG) 引擎示例游戏 — 设计文档（DESIGN）

> 版本：v1.0　|　日期：2026-08-16　|　用途：《AmeKAG 示例作品》（引擎「最强形态」验证作品 + 生态种子）
> 本文档只描述**设计与结构**，不包含 .ks 脚本本体——脚本在后续轮次按本设计逐一填充。
> 目标：15–20 分钟完整可玩的中型 VN，展示引擎全部能力（118 命令 / 79 能力基线）。

---

## 1. 一页摘要

| 项 | 内容 |
|---|---|
| **标题** | 《单程回信》(The One-Way Reply) |
| **类型** | 现代校园 · 温情悬疑 · 短篇多结局 VN |
| **世界观(一句话)** | 高中阁楼里有一台只收发出的信、回信却总晚到十七年的旧邮箱。 |
| **主角** | 玩家视角（第一人称转学生，未具名、无立绘） |
| **角色** | **澪（Mio）**——负责旧邮箱的 2 年生少女，神秘安静；
　　　　　**潮（Ushio）**——公民课老师 / 旧楼管理员，知晓邮箱来历，提供关键线索（次要角色，SMA 信使幻象/旁白可借用其形象） |
| **场景数** | **6 个主场景 + 1 个结局分叉段 + credits**（共 8 个流程节点） |
| **结局** | **3 个**：真结局（归零）、好结局（同行）、普通结局（守约），均有 [ending] 解锁 |
| **主线长度** | 单局约 15–18 分钟；含 2 个存档点、2 次玩家选择、1 次计时选择、1 段 SMA 小游戏融合 |
| **复用基础** | 复用 demo/example_game/ 现有 story.ks 模式（ch/select/ending/scroll）；
　　　　　复用 demo/assets 全部现有 PNG + 音频；SMA 复用 demo/sma_demo.ks 驱动模式 |

---

## 2. 故事大纲

### 2.1 剧情梗概

转学的第一天，主角在旧教学楼的阁楼里遇见了澪——一个总在给「没有任何收件人的信箱」投信的少女。
主角出于好奇写信一试，次日收到一封落款日期是 **十七年前** 的回信。

两人开始隔着十七年的时间互相通信：澪在「今天」收到主角寄往过去的信，主角则在「今天」收到澪十七年前写下的回信。
信里拼凑出一个悬案——十七年前旧楼曾有一位学生在毕业前夜失踪，从此这间阁楼的邮箱开始「单程回信」。

真相藏在最后三封信里：失踪的学生其实是 **澪的姐姐**，而澪一直想借邮箱把一封「迟到十七年的道歉信」送回过去，让自己当年没能开口的话抵达答案。

玩家的选择决定这封跨时间的信最终**能否抵达**，以及回到哪一边的结局。

### 2.2 主题与基调

- **主题**：未说出口的话、时间的单向性、告别的勇气。
- **基调**：从校园日常的轻快 → 阁楼吊诡的氛围 → 追寻真相的悬念 → 雨夜告白的温柔 → 结局的释然。
- **情绪弧线**：好奇 → 疑惑 → 靠近 → 揪心 → 决意 → 释怀。

### 2.3 角色设定

| 角色 | 定位 | 关键动机 | 表现手法 |
|---|---|---|---|
| **主角**（玩家，无立绘） | 转学生，玩家自我代入 | 想弄懂邮箱的秘密 / 想帮澪 | 第一人称视角；对话用系统名「你」或留白 |
| **澪 Mio** | 女主角 / 信使 | 想替姐姐送出道歉信、解开十七年心结 | girl_uniform.png 作占位立绘（复用）；SMA 变体换装 |
| **潮 Ushio** | 老师 / 管理员（次要） | 守护旧楼与邮箱的秘密 | 以「信里描述 + 阁楼暗影」形式存在；SMA 信使幻象可借用其剪影 |

---

## 3. 场景流程表（场景 → 关键命令 → 分支/结局）

> 图例：[cmd] = 该场景**必须展示的关键命令**（标注「新增」= 相对现有 demo 的补强面）。
> 主事件流用 *label 标签；所有剧本落在最终 .ks 的 8 个流程节点。

### 场景 0 — 标题/开场（约 0.5 分）
- **目的**：冷启动、建立基调、演示系统 UI 首屏。
- **关键命令**：[bg] [playbgm] [font] [pt] [ch] [p] [wait]
- **分支**：无（单线 → 场景 1）。

### 场景 1 — 晨光教室（约 2.5 分）
- **目的**：角色登场、日常铺垫、i18n 中英热切换首次展示、**存档点 ①**。
- **关键命令**：[bg classroom] [fg] [position layer=fg pos=right] [playvoice] [playse]
  [ch name=澪 sprite=girl_uniform voice] [sprite_move] [sprite_fade]（澪登场滑入+淡出强调）
  [i18n language=en] / [i18n language=zh]（中英热切换 + {settings}/{items} 复数 {other} 展开）
  [save slot=1]　[notify msg]（「自动存档已完成」吐司）
- **分支**：无（单线 → 场景 2）。

### 场景 2 — 阁楼与旧邮箱（约 2.5 分）
- **目的**：吊诡氛围营造、道具设定、表达式/循环首秀。
- **关键命令**：[bg hana] [trans method=dissolve] [blur amount=..]（阁楼呼吸感）
  [particles]（尘埃光点，rate/size/speed）[vib]
  [set f.clues=0] [eval exp=...] [for]/[endfor]（读取信箱格子的三封信循环）
  [ruby]（澪名字/稀有字注音）[text speed=..] 变体
- **分支**：无（单线 → 场景 3）。

### 场景 3 — 第一封单程回信（约 2.5 分）
- **目的**：超自然设定揭示、玩家**第一次选择**（决定后续信任线）。
- **关键命令**：[select]/[sel target=*... text=...]/[endselect]（3 选）
  [switch exp=..]（按选择存 f.action）[if]/[else]/[endif]
  [wait ms] [delay ms]
- **分支点 A**：
  - *probe（追问信里时间戳）→ 信任 +1
  - *poke（试探澪）        → 信任持平
  - *drop（把信放回）      → 信任 -1（通向普通结局的伏笔）
- **标签汇合**：*post_choice（三线汇合 → 场景 4）。

### 场景 4 — 追查·十七年前（约 3 分）
- **目的**：推进线索、信任值驱动差分文本、**存档点 ②**。
- **关键命令**：[set f.trust=...] / [if f.trust>=..] 差分
  [add]/[sub]（线索/信任累计）　[while]/[endwhile]　[eval]
  [preload path=hana]　[csp]/[csl]/[csd]（潮老师立绘闪现）
  [gallery]/[unlock]（解锁线索 CG）　[chapter]（章节选择 UI）
  [save slot=2]
- **分支点 B**（信任阈值 + 玩家直觉二选，双层）：
  - 信任 ≥ 2 且选「相信澪」→ *truth_lead（通真结局）
  - 其余 → *companion_lead（通好结局候选）
- **标签汇合**：*post_investigate → 场景 5（分歧段仍共享雨夜高潮主体，仅结局段不同）。

### 场景 5 — 雨夜·真相（高潮，约 4 分）
- **目的**：节奏最紧张处，视觉/音频/动效全开，**SMA 小游戏融合**。
- **关键命令**：
  [flash] [quake] [shake] [vib] [vibrate]（雷雨动效）
  [trans method=crossfade]　[fade]/[layfade]（层淡入淡出）
  [playbgm]（雨夜低音）[playse]（雷）[stopbgm]
  [scroll]（姐姐的日记旁白）[nameplate]
  **SMA 融合**：[sma_play]/[sma_anim]/[sma_variant]/[sma_ik]/[sma_stop]
    —— 在阁楼暗处驱动一个「信使幻象」（骨架剪影，idle → wave → 眼眶变体 → 手臂指向）作为轻量交互锚点；
    若用 SMA 驱动器占位，则该段退化为纯演出等待 + [notify] 说明。
  [until]/[wait ms]　[ai_dialog fallback]（可选：高光台词接 LLM，缺后端时用 fallback 兜底）
- **分支**：**玩家第二次选择（计时选择点）**——在雨声计数内决定「把最后一封信投出 / 撕掉 / 保留」。
  [until]/[button cond]（条件选择：某些选项仅在信任≥2 时出现）→ 三结局分流。

### 场景 6a / 6b / 6c — 三结局（各约 1 分）
- **目的**：兑现分支、[ending] 解锁、closing。
- **关键命令**：[ending id=xxx name]　[bg]　[scroll]　[gallery]　[history]
  [rollback]（提示可回看）[stopbgm]　[ch] 收尾台词
- **结局 A · 真结局「归零 (Zero Hour)」**：信抵达十七年前的夜里，姐姐没再失踪；澪在「今天」醒来看见主角——时间线归零。**（[ending id=zero_hour]）**
- **结局 B · 好结局「同行 (Companion)」**：信找回，但答案留在「今天」——澪与主角一起看完日记，把邮箱封存，约定不再寄往过去。**（[ending id=companion]）**
- **结局 C · 普通结局「守约 (Promise)」**：信被留下/撕掉，邮箱从此沉默；十七年前的谜仍是谜，主角与澪成为普通朋友。**（[ending id=promise]）**

### 场景 7 — Credits（约 0.5 分）
- **目的**：收尾、能力自述、结束。
- **关键命令**：[cl] [scroll] × 3　[history]　[wait]　[end]

---

## 4. 能力展示清单（必须全覆盖）

> 标 ✓ = 本设计已安排；每项都给出**落点场景 + 具体命令/方式**。此项是验收依据。

### 4.1 表现层（文本/立绘/背景/音乐/语音）
- [x] **文本系统**：[ch]/[text]/[font face size]/[pt]/[textspeed]/[cps]/[ruby]/[nvl]/[nameplate]（场景 0–2、4）
- [x] **立绘（fg/角色层）**：[fg] [position] [csp]/[csl]/[csd] [sprite_move]/[sprite_fade]/[sprite_scale]/[sprite_swap]（场景 1、3、4 潮闪现）
- [x] **背景层**：[bg] [cl] [image]（场景 0–7）
- [x] **音乐（三总线 BGM）**：[playbgm] [bgm] [play bus=bgm] [setbgmvolume] [fadebgm] [xfadebgm] [stopbgm] [waitbgm]（贯穿）
- [x] **音效 SE**：[playse] [setsevolume] [stopse] [waitsound]（雷、笔、门声；场景 1、5）
- [x] **语音 VOICE**：[playvoice] [ch voice=] [voice_wait] [stopvoice] [voice_off]（场景 1 澪台词 + 5 主台词）

### 4.2 交互与流程
- [x] **分支选择**：[select]/[sel]/[endselect]（场景 3、5）；[button]/[endbutton] + [button cond]（场景 5 条件选项）
- [x] **存档点**：[save slot=1]（场景 1）、[save slot=2]（场景 4）；[saveload mode=save|load]/[load]/[listsaves]/[loadplace]/[saveplace]（玩家自助）
- [x] **控制流（表达式分支）**：[if]/[else]/[endif]、[switch]/[case]/[default]/[endswitch]、表达式插值（${expr}/$tbl.key/%tbl.key%）（场景 3、4）
- [x] **循环**：[for]/[endfor]（场景 2）、[while]/[endwhile]（场景 4）；[add]/[sub]/[mul]/[div]/[mod]/[dec]/[inc]（线索累计）
- [x] **计时/等待**：[wait ms] [delay ms] [until] [waitclick] [waitforclick]（场景 3、5）
- [x] **跳转/调用/返回**：[jump target=*] [goto] [call] [link] [return]（跨场景结构）
- [x] **多结局**：[ending id name]+（场景 6a/b/c）+ 画廊 [gallery]/[unlock type=cg/music]
- [x] **backlog / 跳过 / 自动**：[history]（场景 2、7）+ [skip mode=seen] [auto] + 引擎 Ctrl 按住跳过 / A 自动（entry UI，复用 demo/entry.lua 模式）

### 4.3 效果（转场/粒子/后处理）
- [x] **转场**：[trans method=dissolve]（场景 2）、[trans method=crossfade]（场景 5）
- [x] **粒子**：[particles]（场景 2 尘埃、场景 5 雨）
- [x] **后处理占位 / 滤镜**：[blur]（场景 2）、[palette effect=day|night|toggle]（场景 2 夜间 / 5 雨夜）、[fade]/[layfade]（层淡入淡出）、[camera]（镜头微移）
- [x] **动效**：[flash] [vib] [vibrate] [quake] [shake]（雷雨 / 情绪强调）

### 4.4 i18n（多语言）
- [x] **中英热切换**：[i18n language=en|zh] {key} 字典展开 + {items} 复数变体 {other}（场景 1，首尾各一次）
- [x] **字典文件**：默认 assets/lang/zh.lua；加 en.lua 完整行级译文（后续补；本设计只保证 zh 英文混排可用）

### 4.5 小游戏（SMA 融合）
- [x] **SMA 骨架机制作**：[sma_play] [sma_anim] [sma_variant] [sma_ik] [sma_stop]（场景 5 信使幻象；复用 demo/sma_demo.ks 与 demo/sma_demo_driver.lua 模式）
- [ ] **（可选扩展）**：若轮次余量足，可把「对准阁楼天线投出信」做成 2-bone IK 拖拽计时小游戏（[sma_ik] + [until] 判定）；缺资源则记录为占位、演出退化为纯等待。

### 4.6 系统 / 混合脚本
- [x] **KAG+Lua 混合**：[eval] [iscript]…[/endscript] [emb]（场景 4 example_game 回调做线索计数，复用 example_game/entry.lua 的 letters_read 模式）
- [x] **资源预加载**：[preload path=assets/bg/hana.png]（场景 4 切图无卡顿）
- [x] **AI 高光（可选降级）**：[ai_dialog fallback=...]（场景 5 最后一句台词，LLM 后端缺失时用 fallback 兜底）
- [x] **宏复用**：[macro first_letter args=...]（场景 2–3 读多封信，参数化占位符，复用 example_game scene_intro 宏模式）

---

## 5. 资产需求清单

### 5.1 直接复用（repo 现有，零新增）
| 路径 | 用途 |
|---|---|
| assets/bg/classroom.png | 教室 / 阁楼室内（场景 1、4） |
| assets/bg/hana.png | 阁楼光影 / 雨夜外部（场景 2、5、6） |
| assets/fg/girl_uniform.png | 澪占位立绘（场景 1、3、5）+ 潮闪现（场景 4） |
| assets/bgm/daily.wav | 日常 BGM（场景 0–2；场景 5 可淡出切氛围） |
| assets/se/click.wav | 点击 / 笔墨 / 信纸、雷（配合震动用同一素材降噪） |
| assets/voice/line01.wav | 澪第一句语音占位（场景 1） |
| assets/fonts/NotoSansCJKsc-Regular.otf | 中文字体（场景 0 [font] 可指定，复用 full_pipeline 的 CJK 演示） |
| demo/assets/sma/hero.json + template.json | SMA 信使幻象骨架（场景 5） |
| demo/minigame_scene.json |（可选 3D 小游戏参考，若转 3D 场景） |

### 5.2 需要新增/设计占位（后续填充或用 create_solid_texture 运行时注入）
| 资产 | 缺失处 | 落点 | 建议 |
|---|---|---|---|
| 澪的**独立立绘**（多表情/多姿态） | 目前仅 girl_uniform 一张 | 场景 1/3/5 | 设计占位：定义表情槽位（normal/happy/pain/resolve），后续补 PNG；缺图时用 [csp] 现有图 + [sprite_fade] 顶格 |
| **背景变体**（夜色阁楼、雨夜走廊、天台） | 仅 classroom/hana | 场景 2/5/6 | 设计占位：标注各需要一张；缺图时 [palette effect=night] + [blur] 改造现有图 |
| **CG**（关键剧情立绘） | 无 | 结局 6a/b/c | 设计占位：3 张 CG（[unlock type=cg]）；缺图暂时用合成背景+前景裁切 |
| **LUT**（assets/lut/night.png） | 无 | [palette] | 缺失时 palette.lua 已有守卫自动回退（见 tutorial_13），零风险 |
| **雨声/雷 SE** | 无专属 | 场景 5 | assets/se/click.wav 降噪复用或后续补 2 个短 ogg |
| **雨夜 BGM** | 无 | 场景 5 | 现有 daily.wav 循环低音量即可；后续补 1 首 |

> **i18n**：assets/lang/zh.lua 已有 UI 键；en.lua 需补行级（scene 台词）译文（后续轮）。ja.lua 为自动生成占位，暂不承诺日文质量。

> **SMA**：hero.json 骨骼编号需与驱动器约定（root/body/head + 右臂 2-bone chain），复用 sma_demo_driver 的 spawn/blend/variant/ik 顺序即可，无需新资产。

---

## 6. 预计时长分配表（总 15–20 分钟）

| 场景 | 环节 | 预计时长 | 备注 |
|---|---|---|---|
| 0 | 标题/开场 | 0.5 分 | BGM+立绘冷启动 |
| 1 | 晨光教室 | 2.5 分 | 立绘登场 + i18n + 存档① |
| 2 | 阁楼与旧邮箱 | 2.5 分 | 氛围/粒子/循环读信 |
| 3 | 第一封单程回信 | 2.5 分 | 第一次选择（分支 A） |
| 4 | 追查·十七年前 | 3 分 | 差分 + 循环 + 存档② |
| 5 | 雨夜·真相（高潮） | 4 分 | 全动效 + SMA + 计时选择 |
| 6a/b/c | 三结局 | 1 分 | [ending] + 画廊解锁 |
| 7 | Credits | 0.5 分 | scroll + history |
| **合计** | — | **≈ 17.5 分** | 15–20 区间内，留余量 |

> 时长按「正常速度逐字 + 手动点击推进」估算；slow/fast 阅读与 [skip mode=seen] 会改变实际时间。

---

## 7. 文件结构规划（供后续轮次填充 .ks）

```
demo/example_game/
├── DESIGN.md                 # 本文档
├── README.md                 #（更新：指向新作品，列出三结局与能力清单）
├── entry.lua                 #（复用/增强：history/toast/Ctrl-skip/A-auto + example_game 回调）
├── story.ks                  #（主体；或按场景拆 multi-scene：ch0.ks … ch7.ks + entry 切场景）
└── sma_minigame/             #（可选：SMA 融合驱动，复用 sma_demo_driver 模式）
```

- **场景组织建议**：单文件 story.ks 体积可控（<1000 行）时保持单文件最简；若需跨场景 [call]/[jump] 演示，可拆 scene_*.ks（引擎支持跨场景跳转，round 97-99 已加固预算与循环栈）。
- **启动方式**：`lua demo/example_game/entry.lua`（与现状一致），运行路径可从仓库根或 build 输出目录。
- **校验**：每轮 `lua scripts/ks_check.lua demo/example_game/<file>.ks`（契约静态校验）零警告 + ks_bake 编译 + Web 播放器跑通。

---

## 8. 验收标准（后续填充完成时核对）

1. 单局可完整从标题 → 至少一条路线 → 对应结局 → credits，约 15–18 分。
2. 三条路线 / 三结局全部可达且 [ending] 各解锁一次。
3. 第 4 节能力清单全项均有**可观测落点**（不是文档承诺、是实际命令执行）。
4. demo/example_game/*.ks 通过 ks_check 静态契约校验（零警告）+ Web 播放器零错误。
5. assets 全部复用 repo 现有，或新增资产以「设计占位」标注、缺图时走安全降级。

