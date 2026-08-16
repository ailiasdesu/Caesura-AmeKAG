# 示例游戏《单程回信》资产审计与 i18n 键预留

> 审计对象：`demo/assets`、`assets/land/{zh,en,ja}.lua`
> 依据：`demo/example_game/DESIGN.md` §5 资产需求清单
> 审计性质：只读审计 + 键名建议（不改动 `demo/assets` 与 lang 文件；`docs/guides/sample-game-assets.md` 为本次产出）

---

## 1. demo/assets 实际内容（无遗漏）

`demo/assets/` 下全部文件：

| 路径 | 状态 |
|---|---|
| `demo/assets/sma/hero.json` | 存在（SMA 信使幻象骨架，场景 5 复用） |
| `demo/assets/sma/template.json` | 存在（SMA 模板） |
| `demo/assets/sma/_broken_example.json` | 存在（官方损坏样例，引擎容错演示用，**非资产**，勿在游戏内引用） |

引擎目录 `assets/`（DESIGN §5 直接复用路径均在此，经逐一 `test -f` 验证）：

| 路径 | 状态 |
|---|---|
| `assets/bg/classroom.png` | EXISTS |
| `assets/bg/hana.png` | EXISTS |
| `assets/fg/girl_uniform.png` | EXISTS |
| `assets/bgm/daily.wav`（另有 `daily.ogg`） | EXISTS |
| `assets/se/click.wav`（另有 `click.ogg`） | EXISTS |
| `assets/voice/line01.wav`（另有 `line01.ogg`） | EXISTS |
| `assets/fonts/NotoSansCJKsc-Regular.otf` | EXISTS |
| `demo/assets/sma/hero.json` + `template.json` | EXISTS |
| `demo/minigame_scene.json` | EXISTS |
| `assets/lut/night.png` | **MISSING**（设计占位） |

> 注：`assets/` 与 `demo/assets/` 是两个目录：引擎通用资产在 `assets/`，示例游戏专属（SMA 骨架）在 `demo/assets/sma/`。

---

## 2. DESIGN §5 资产对照表（可复用 / 缺失降级）

### 2.1 直接复用（全部存在，零新增）✓

| 清单项 | 实际路径 | 复用用途 |
|---|---|---|
| classroom.png | `assets/bg/classroom.png` | 教室/阁楼室内（场景 1、4） |
| hana.png | `assets/bg/hana.png` | 阁楼光影/雨夜外部（场景 2、5、6） |
| girl_uniform.png | `assets/fg/girl_uniform.png` | 澪占位立绘 + 潮闪现（场景 1/3/4/5） |
| BGM | `assets/bgm/daily.wav|ogg` | 日常 BGM（场景 0–2；5 淡出切氛围） |
| SE | `assets/se/click.wav|ogg` | 点击/笔墨/信纸/雷（降噪复用） |
| VOICE | `assets/voice/line01.wav|ogg` | 澪第一句语音占位（场景 1） |
| 中文字体 | `assets/fonts/NotoSansCJKsc-Regular.otf` | 场景 0 [font] 指定 |
| SMA 骨架 | `demo/assets/sma/hero.json` + `template.json` | 场景 5 信使幻象 |
| 3D 参考 | `demo/minigame_scene.json` |（可选）转 3D 场景时 |

### 2.2 设计占位 → 安全降级（6 项，全部 MISSING，按安全降级处理）⚠

| # | 设计占位 | 缺失处 | 落点 | 降级策略（代码/命令） | 风险 |
|---|---|---|---|---|---|
| 1 | 澪独立立绘（多表情/姿态） | 仅 1 张 girl_uniform | 场景 1/3/5 | `[csp]` 用 girl_uniform + `[sprite_fade]`/字色/表情文字差替代；预留表情槽位 normal/happy/pain/resolve，后续补 PNG | 低 |
| 2 | 背景变体（夜色阁楼/雨夜走廊/天台） | 仅 classroom/hana | 场景 2/5/6 | `[palette effect=night]` + `[blur]`/雨粒子改造现有 classroom/hana | 低（palette 有守卫） |
| 3 | CG（3 张关键剧情） | 无 | 结局 6a/b/c | `[bg]` + `[trans]` 合成背景 + 前景裁切 + `[unlock type=cg]` 用合成图 | 低 |
| 4 | LUT（assets/lut/night.png） | MISSING | [palette] | **已确认安全**：`palette.lua set_night_mode()` 缺 LUT 文件时 `palette.clear()` 回退中性，不崩 | 零 |
| 5 | 雨声/雷 SE | 无专属 | 场景 5 | `assets/se/click.wav` 降噪复用做雨声；雷用 `[flash]`+`[quake]`+`[vib]` 无音频也成立 | 低 |
| 6 | 雨夜 BGM | 无 | 场景 5 | `daily.wav` 低音量循环 + `[setbgmvolume]`/`[fadebgm]` | 低 |

> **关键降级保障**：`palette.lua`（round 72/82 加固）所有 LUT 操作（load/apply/clear/unload）都先过 `lut_available()` 守卫，缺 C++ 绑定或缺 LUT 文件一律输出可见 notice 转 no-op，绝不在场景中途 nil crash。→ 第 4 项（LUT）零风险，`[palette effect=night]` 可直接在 .ks 里安全使用。

---

## 3. lang 词典审计（zh / en / ja）

### 3.1 现有键结构

- **三个文件（zh.lua / en.lua / ja.lua）均为顶层裸表**，键命名 snake_case。
- 三类键：
  1. **UI 菜单键**：`title_screen / new_game / continue / load_game / settings / gallery / music_room / back / yes / no / save / load` …（26 个标量键，三层齐全）
  2. **插值键**：`{settings}`（菜单文本，走 `i18n.t`）；`{items}`（复数变体表）——见 3.3
  3. **复数表**：`items = { one="...", other="..." }`（CLDR one/other；zh/ja 无复数区分恒走 `other`）
  4. **`lines` 逐行翻译表**（内容寻址键 `"<scene>.<fnv1a(原文)>"`）——目前只在 ja.lua，为 ks_i18n.lua 自动生成的翻译模板

### 3.2 跨语言一致性检查（→ 无加载错误）

逐键比对三层：

- **26 个 UI 标量键** zh/en/ja **全部 ✓ 存在**，无缺语言。
- **`items` 复数表**三层均在，且 each 均含 `one`+`other` 子键（en 两形 different；zh/ja 两形相同，符合无复数区分语言）。
- **唯一跨语言差异**：`lines` 表仅 ja.lua 有，zh.lua / en.lua 没有。→ 这是**设计预期**：ja 是 `scripts/ks_i18n.lua --dir demo --lang ja` 生成的翻译模板；zh/en 是手工维护的 UI 字典。i18n.lua 的 `available()`/加载器遇不到 `lines` 会正常当普通键处理，**不会导致加载错误**。

### 3.3 {settings}/{items} 插值键用法（已验证）

- `{settings}`：UI 菜单键（=“设置”），`story.ks.new` 与 `tutorial_14_flow_timing.ks` 均用 `[ch text="...{settings}"]` 演示 `{key}` 展开。
- `{items}`：复数变体表，`i18n.translate("items", {n=N})` 取 one/other；无 n 参数时落 `other`。
- 建议示例游戏沿用 `{settings}`/`{items}`（已注册，语义稳定），不必新造。

---

## 4. 示例游戏 i18n 键预留建议（`owr_` 前缀，避免冲突）

> 命名理由：现有键无命名空间（`title_screen` 等被 UI/其他 demo 共用）。示例游戏专属 UI 键加 **`owr_`**（O**W**ay **R**eply 首字母）前缀隔离，避免与既有键及未来其他作品键冲突。**所有 `owr_*` 同时三层（zh/en/ja）补齐**。

### 4.1 UI / 系统键（三层预留）

| 键 | 中文 | English | 上下文 |
|---|---|---|---|
| `owr_title_sub` | 单程回信 | The One-Way Reply | 标题副标/开场 |
| `owr_mailbox_open` | 打开信箱 | Open the mail slot | 阁楼邮箱交互提示 |
| `owr_locket_inscription` | 十七年 | seventeen years | 锁扣铭文（道具） |
| `owr_letter_read` | 阅读信件 | Read the letter | 信件 UI 按钮 |
| `owr_letter_seal` | 封缄 | Seal | 结尾封缄操作 |
| `owr_trust_high` | 信任渐深 | A growing trust | 信任值差分文本标签 |
| `owr_trust_low` | 疑窦丛生 | A flicker of doubt | 信任值差分文本标签 |
| `owr_ending_zero_hour` | 归零 | Zero Hour | 真结局名 |
| `owr_ending_companion` | 同行 | Companion | 好结局名 |
| `owr_ending_promise` | 守约 | Promise | 普通结局名 |
| `owr_clue_count` | 收集线索 {n} 条 | Clues gathered: {n} | `{n}` 插值计数 |

### 4.2 角色/台词行级键（走 `lines` 表，非顶层键）

主叙事对白不应塞进顶层满 dict，应用 `lines` 内容寻址表做逐行翻译（与 ja.lua 同机制）：

| 键（lines） | 中文 | English | 上下文 |
|---|---|---|---|
| `"story.ks:<fnv1a(澪)>"` | 我在等一封回信。 | I'm waiting for a reply. | 澪开场台词 |
| `"story.ks:<fnv1a(潮)>"` | 那邮箱……只会发出，不会收到。 | That mailbox only sends, never receives. | 潮关键线索 |
| `"story.ks:<fnv1a(姐姐)>"` | 拜托了，替我送到十七年前的夜里。 | Please, carry it to the night seventeen years ago. | 姐姐日记旁白 |

> 内容寻址键由 `scripts/ks_i18n.lua --dir demo/example_game --lang en|zh` 自动生成占位，脚本完成后即可增量补译文（`--update` 保留已填部分）。**不要手工伪造哈希键**。

---

## 5. lang 文件损坏/缺失评估（结论：无需修复）

- **无实际损坏**：zh/en/ja 语法均合法（`luac`/加载器接受），三层标量键与 `items` 齐全。
- **不构成加载错误**：`lines` 仅 ja 有属预期（工具生成模板 vs 手工字典），引擎加载器不依赖。
- **唯一建议**（非阻断）：若希望 en.lua 也具备行级翻译能力，可在填充示例游戏 .ks 后运行 `lua scripts/ks_i18n.lua --dir demo/example_game --lang en --update`，把 `lines` 模板并入 en.lua。此为增强项，非缺陷。

---

## 6. 结论摘要

1. **9 项可直接复用资产全部存在**，`assets/` 计数核对无缺失。
2. **6 项设计占位全部走安全降级**，其中 LUT 一项由 palette.lua 守卫已确认零风险；另 5 项用现有资产 + 命令组合降级（详见 §2.2 表）。
3. **i18n 三层键无跨语言缺失**，无加载错误；建议新增键用 `owr_*` 前缀隔离（§4.1 表）。
4. **lang 文件无需修复**；仅建议后续 `--update` 增强 en 行级翻译（可选）。

> 本次为只读审计，未改动 `demo/assets` 与 `assets/lang/*`，未执行 git 提交。
