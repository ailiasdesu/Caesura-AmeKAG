# KAG3 作品迁移指南（kag3-migration）

> 生态入口：把吉里吉里（KiriKiri）/KAG3 停更引擎的完整作品——脚本、图片
> （.tlg）、音频、配置——批量迁移到 Caesura (AmeKAG) 项目。本文是**端到端
> 迁移路径编排**（资产 → 脚本 → 音频 → 验证）；单步 .ks 转换的规则与用法
> 详见 [kag3-import.md](./kag3-import.md)，不要在这里重复。

## 0. 前提与定位

- **XP3 不是运行时目标**（`docs/plans/2026-08-12-004-generation-gap-roadmap.md`
  明确：不支持 XP3，归档代差由 CARC/AES-256-GCM/Ed25519 承担）。因此
  **R111-A（.xp3 解包器）与 R111-B（.tlg 解码器）是迁移期工具**，只在本
  路径的解包/转码步骤使用；迁移完成后运行时只读 CARC 或裸目录资产。
- 引擎资源根：`main.cpp` 向上探测到含 `assets/` 的目录并 chdir 过去，所有
  `storage=`/`path=`/`file=` 均为**相对游戏根**的路径；场景固定位于
  `assets/script/*.ks`（scheduler 的 `is_safe_scene_path` 要求
  `^assets/script/` 前缀 + `.ks` 后缀，禁止 `..` 穿越——见
  `scripts/scheduler.lua`）。
- 迁移目标目录规范（推荐）：`assets/script/`（脚本）、`assets/images/`
  （bg/fg/ui）、`assets/audio/`（bgm/se/voice）、`assets/video/`。
  若 KAG3 归档内已是 `bg/`、`fg/`、`sound/` 之类的布局，建议整体搬到
  `assets/migrated/` 下**保留相对关系**，再经由第 ⑤ 步路径重写衔接。

## 1. 迁移路径总览

```
KAG3 作品（game.xp3 及拼接进 exe 的归档，含 .ks + .tlg/.jpg + .ogg/.wav/.mp3 + 配置）
  │  ① xp3 解包  [R111-A，迁移期工具]
  ▼
解包目录树（保留归档内相对路径：scenario/ image/ sound/ video/ …）
  │  ② tlg → png  [R111-B，迁移期工具]
  ▼
引擎可解码图片（.png，RGBA；tlg 的 alpha 合成到透明通道）
  │  ③ 音频格式映射（ogg/wav/mp3/flac 直用；其余转码）
  ▼
assets/migrated/…（按引擎目录规范重排，或原样搬入后重写路径）
  │  ④ .ks 转换  [kag3_import --strict -o out/]
  ▼
assets/script/*.imported.ks（Neo-Genesis 格式）
  │  ⑤ 资产路径重写（storage= 前缀——kag3_import 扩展点，见 §4）
  ▼
assets/script/*.ks（storage 指向 assets/migrated/…）
  │  ⑥ 验证（ks_check 静态门禁 + 引擎播放/headless 冒烟）
  ▼
可运行的 Caesura 项目（可再经 carc_pack 打包为 CARC）
```

## 2. 分步说明：输入 / 输出 / 工具 / 已知限制

### 步骤 ①：xp3 解包（R111-A）

| | |
|---|---|
| **输入** | `game.xp3`（可能为多归档：`arc1.xp3`/`voice.xp3`…，或拼接进游戏 exe 的归档） |
| **输出** | 解包目录树，**保留归档内相对路径**（KAG3 的 `storage=` 按这些相对路径引用资产） |
| **工具** | R111-A 原型（本引擎）；兜底可用社区工具（xp3 解包器） |
| **限制** | ① 加密插件作品无密钥不可解——如实报告，不假装成功；② xp3 索引按**路径哈希**存储（kag3_import 的 `--carc` 模式已处理同类问题：调用者须知道场景相对路径）；③ 文件名编码常为 Shift-JIS，解包时建议转 UTF-8（Windows 本地化差异）；④ 拼接进 exe 的归档先剥离 exe 再解；⑤ 解包树内 `.tjs` 脚本与 `.ks` 并存，`[iscript]` 块的 TJS 依赖链需在 ④ 报告后人工评估 |

### 步骤 ②：tlg → png（R111-B）

| | |
|---|---|
| **输入** | `.tlg`（TLG5.0/5.1/6.0 等变体；自带 alpha 通道） |
| **输出** | `.png`（RGBA，透明通道 = tlg alpha；尺寸/命名保持，扩展名 `.tlg`→`.png`） |
| **工具** | R111-B 原型（本引擎）。**引擎 stb_image 不解码 tlg**（`docs/guides/asset-pipeline.md` 支持清单无 TLG），所以这是硬性前置 |
| **限制** | ① tlg 变体压缩算法不同，解码器需逐变体覆盖并报告未知变体；② alpha 合成（tlg 的 4 层 alpha）须正确折叠进 RGBA，转错会导致透明边缘发黑/发白；③ jpg 背景无需转（引擎原生支持）；④ 质量无损目标，禁用有损二次压缩 |

### 步骤 ③：音频格式映射

| | |
|---|---|
| **输入** | KAG3 音频 `.ogg` / `.wav` / `.mp3`（常见）及罕见格式 |
| **输出** | 同格式直用，或转码 `.wav`（无损）/`.ogg`（有损） |
| **工具** | 无（引擎 SoLoud 原生支持 WAV/FLAC/MP3/OGG，见 asset-pipeline.md） |
| **限制** | ① **ogg/wav/mp3/flac 零转换直用**——这是 KAG3 与引擎的天然重合点；② `.mid`（KAG3 可播 MIDI）引擎不支持，转 wav/ogg；③ 其他专有格式需先转码；④ 引擎 3 总线（BGM 1 / Voice 1 / SE 多），迁移时按用途归位（`audio/bgm|voice|se`） |

### 步骤 ④：.ks 转换（kag3_import）

见 [kag3-import.md](./kag3-import.md) 全量规则，此处只列与本路径相关的要点：

| | |
|---|---|
| **输入** | 解包后的 `.ks`（建议先完成 Shift-JIS→UTF-8） |
| **输出** | `<原名>.imported.ks`（`-o outdir` 时写入 outdir） |
| **工具** | `lua scripts/kag3_import.lua --strict -o <outdir> <scene.ks> ...` |
| **限制** | 已知 gap（round 75/84/96 记录）：宏体命令参数位 `&who` 不转 `%who%`；`[iscript]` TJS 块保留原样需人工移植到 Lua；`[chara_show]`/`[motion]`/`[btndef]` 等按 `SUGGESTIONS` 报告；别名+显式共存不 dedup（last-wins） |

> **场景路径**：转换后 `.ks` 的**存储位置**与 `.ks` 内部 `storage=` 是两件
> 事——场景文件本身必须放 `assets/script/`（跨场景 `[jump]/[call]/[link]`
> 只解析 `assets/script/<target>.ks`）；`storage=` 指资产（背景/立绘/音频），
> 由第 ⑤ 步处理。把整个 `outdir` 指向 `assets/script/` 即可。

### 步骤 ⑤：资产路径重写（storage= 前缀）——扩展点

**现状（已核实源码）**：kag3_import 当前**原样输出** `storage=`/`path=`/`file=`
的值。`processScene` 的 `rewrite_param`（`scripts/kag3_import.lua` 320-346 行）
只做两件事：按 `PARAM_ALIASES` 重命名参数键（如 `var`→`name`），以及对
`TEXT_PARAMS`（text/name/caption/title/msg）做 `&var` 转换、对
`EXPR_PARAMS`（exp/expr/cond）做 TJS→Lua 翻译；`storage` 类参数值
**逐字保留**（含引号/裸值两种形态）。测试 `test_kag3_import.lua` 也锁定
了 `[preload path="bg/01.png"]` 输出保持原样。

→ 因此 "资产搬进 `assets/migrated/` 后要让旧的 `storage="bg/01.png"` 变成
`storage="assets/migrated/bg/01.png"`" 这一步目前**必须人工或外部脚本**完成，
是天然扩展点：**为 kag3_import 增加 `--asset-prefix <prefix>`**（方案见 §4）。

### 步骤 ⑥：验证

| | |
|---|---|
| **ks_check 静态门禁** | `lua scripts/ks_check.lua assets/script/*.imported.ks` → 0 violations（参数类型/必填/枚举 + 跨场景跳转目标解析） |
| **file 型参数存在性** | `[bg]/[fg]/[playbgm]` 等 `storage` 声明为 schema `file` 类型，**运行时**校验路径存在（`kag/schema.lua` `file` 分支：拒绝空/穿越/绝对路径 + `ctx.resolve_file` 存在性检查）；ks_check 本身不提供 resolve_file，所以资产缺失在**运行/编辑器**时暴露 |
| **引擎播放冒烟** | 参照 `scripts/verify_sample_game.sh` + `tests/scripts/sample_game_headless.lua` 模式（mock callable 绑定：`is_voice_playing`/`is_bgm_playing` 必须 false 防 [playvoice]/[playbgm] 死等）逐场景跑到 DONE |
| **三总线音频** | `playbgm`/`playvoice`/`playse` 各发一条，确认解码/总线路由正常 |
| **CARC 打包（可选）** | 验证通过后用 `tools/carc_pack` 把 assets 打包，`kag3_import --carc` 模式可用来抽查归档内场景 |

## 3. 端到端样例：海市蜃楼之馆风格场景（虚构路径）

> 以下作品《胧夜馆》为**虚构样例**，目录/文件名仅为演示 KAG3 常见布局
> （`scenario/`、`image/`、`sound/`、`voice/`）。

### 3.1 源作品布局（xp3 归档内）

```
mirage.xp3
├── scenario/start.ks            ; 开场场景
├── scenario/chapter1.ks
├── image/bg/courtyard.tlg       ; 背景（tlg）
├── image/char/jeanne_n.tlg      ; 立绘（tlg，带 alpha）
├── image/char/jeanne_s.tlg
├── image/ui/dialog_frame.png    ; UI 已是 png，直用
├── sound/bgm/memory_of_mirage.ogg
├── sound/se/door_creak.wav
├── voice/jeanne_0001.ogg
└── system.ks                    ; KAG3 启动/配置场景
```

### 3.2 迁移清单（按步骤）

| # | 动作 | 命令 | 预期输出 / 关注点 |
|---|---|---|---|
| ① | 解包 | `R111-A mirage.xp3 -o unpacked/`（原型） | `unpacked/scenario|image|sound|voice/…` 相对路径保留；`system.ks` 一并解出 |
| ② | 转码 | `R111-B unpacked/ -o unpacked/ --tlg-to-png`（原型） | `courtyard.png`、`jeanne_n.png`、`jeanne_s.png`；记录 tlg 变体清单 |
| ③ | 音频 | 无需转换 | `memory_of_mirage.ogg` / `door_creak.wav` / `jeanne_0001.ogg` 直用 |
| ④ | .ks 转换 | `lua scripts/kag3_import.lua --strict -o assets/script/ unpacked/scenario/start.ks unpacked/scenario/chapter1.ks` | `assets/script/start.imported.ks`；`chara_show`/`motion` 报告 + 建议；`iscript` 块行号清单 |
| ⑤ | 路径重写 | `--asset-prefix assets/migrated/`（扩展后） | `[bg storage="image/bg/courtyard.tlg"]` → `[bg storage="assets/migrated/image/bg/courtyard.png"]`；见 §4 |
| ⑥a | ks_check | `lua scripts/ks_check.lua assets/script/*.imported.ks` | 0 violations；`[palette]` 冲突提示（advisory）人工过目 |
| ⑥b | 播放冒烟 | headless runner 逐场景 DONE | `[unlock type=cg]`/`[save slot]` 等阻塞交互按 headless 契约 mock |

### 3.3 迁移后 start.ks 片段（转换 + 重写后）

```ks
*start
[bg storage="assets/migrated/image/bg/courtyard.png"]
[fg storage="assets/migrated/image/char/jeanne_n.png" layer="fg"]
[playbgm storage="assets/migrated/sound/bgm/memory_of_mirage.ogg" loop="true"]
[ch name="JEANNE" text="欢迎来到胧夜馆。%f.turn% 夜目の客…"]
; KAG3 时代的 [chara_show name="jeanne" storage="image/char/jeanne_n.tlg"]
; 已在 ④ 报告为 UNSUPPORTED，人工改写为上方 [fg storage=…] +（可选）[sprite_fade]
[playvoice storage="assets/migrated/voice/jeanne_0001.ogg"]
[jump target=*chapter1]
```

> 注意其中两处典型的"人工复核"点：① `chara_show → [fg]` 改写；② 文本里
> 的 `&f.turn` 已被 ④ 自动转为 `%f.turn%`。若原作在 `system.ks` 里用
> `[macro]` 定义了大量全局宏，确认 ④ 输出的宏体参数转换（`&N`→`%N%`）后，
> 宏跨场景共享是 KAG3 兼容语义（round 99 裁决），无需额外处理。

## 4. kag3_import 扩展评估：`--asset-prefix`（可选小改动）

### 4.1 现状核查结论（本指南依据）

- `scripts/kag3_import.lua` **当前没有任何路径重写**：`storage=`/`path=`/`file=`
  参数值（引号与裸值两种形态）逐字保留；`rewrite_param` 只处理
  TEXT_PARAMS/EXPR_PARAMS 的值与 PARAM_ALIASES 的键。
- 测试锁定：`test_kag3_import.lua` 的 `preload path="bg/01.png"` 保持原样
  （round 71 断言）；`csp/csl left/top→x/y`、`add var→name` 证明别名只改键。
- 结论：**路径重写（storage= 前缀）是干净的扩展点**，不触碰现有转换语义。

### 4.2 方案（若实施）

**接口**：`lua scripts/kag3_import.lua --asset-prefix <prefix> [-o outdir] <scene.ks> ...`

**语义**：对每条命令参数中**取值是资产路径**的参数（一个新增 `FILE_PARAMS`
集合：`storage`、`path`、`file`——覆盖 `[bg]/[fg]/[image]/[playbgm]/[playse]/
[playvoice]/[video]/[preload path|storage]` 等），在其值前拼接 `<prefix>`：
`bg/01.png` → `assets/migrated/bg/01.png`。

**幂等/安全规则**（必须写进测试）：
1. 值已以 `<prefix>` 开头 → 跳过（幂等，防 `-o` 目录内重复转换粘前缀）；
2. 值已是绝对路径或以 `assets/`、`/`、盘符开头 → 跳过（不破坏合法形式）；
3. **只改参数值，绝不动场景跳转目标**（`[jump target=*label]` 内的 `*label`
   与 `assets/script/` 场景路径不属于资产路径，`FILE_PARAMS` 不含 `target`）；
4. 含空格/引号的路径：引号形态在引号内拼前缀；裸值形态仅在值不包含会破坏
   tokenize 的字符时拼（裸值 charset 现状是 `[%w_%.%-+]+`，路径含 `/` 时 KAG3
   本就应加引号，前缀拼接按原形态保持）；
5. 空值/缺失 → 不动。

**改动面**（全部在 `scripts/kag3_import.lua` + 测试 + 文档，**零 C++ / 零
接口 / 零注册清单改动**，无耦合风险，符合 AGENTS.md 工具模块边界）：

| 文件 | 改动 | 量级 |
|---|---|---|
| `scripts/kag3_import.lua` | ① `FILE_PARAMS` 表（约 6 键）；② `rewrite_param` 增加前缀分支（返回 `eparam=..q..prefix..pval..q`）；③ CLI 解析 `--asset-prefix`（复用现有 arg 循环）；④ report 增 `prefixed_paths` 计数 + printReport 一行 | ~25 行 |
| `tests/scripts/test_kag3_import.lua` | 新增断言：引号形态前缀、裸值形态前缀、幂等跳过、绝对路径跳过、`*label` target 不动、preload path 前缀、无前缀时原样（回归） | ~8 条 |
| `docs/guides/kag3-import.md` | 增 `--asset-prefix` 一节 | ~10 行 |
| `docs/guides/kag3-migration.md` | §3 清单已引用（同步最终 CLI 形态） | ~2 行 |

**备选方案（不实施代码改动也能走通）**：
- **A. 布局对齐**：解包后直接把目录树排成引擎默认布局（`assets/images/…`、
  `assets/audio/…`），若 KAG3 `storage=` 相对路径恰好与目标布局一致则零改写；
  不一致时需搬运目录而非改脚本——适合路径无冲突的作品。
- **B. mods 机制**：把迁移资产放 `mods/<名称>/` 镜像布局并 `mods.enable`，
  `mods.resolve` 运行时优先命中（`scripts/mods.lua`）——适合"不改原 .ks"
  的分发场景（汉化/高清补丁），但启动链需注册 mod。
- **C. 引擎加资源根搜索列表**（改 C++ resource 解析 + `resolve_file`）——
  改动面最大，**不推荐**作为第一步；若未来需要多作品合流再议。

**建议**：改造成本极低（纯 Lua ~25 行 + 8 断言），收益直接（迁移路径第 ⑤ 步
从"人工/外部脚本"变为内建），**建议实施**——由主代理拍板；本指南已把方案与
改动面评估完毕。

## 5. 配置与存档迁移（一次性说明）

- **KAG3 配置**（`system.ks` / 存档目录内的 ini）：`system.ks` 按第 ④ 步
  作为普通场景转换；KAG3 的窗口尺寸/字体等写在 TJS 常量里，引擎用
  `config.lua` 管理，需人工对齐（通常 1280×720 + 默认字体即可起步）。
- **存档不兼容**：KAG3 槽位存档格式与引擎 slot-based 存档完全不同，**不能
  迁移**；作品发布时旧档作废，或提供"新游戏"路径。
- **按键/操作设置**：KAG3 的 `config.ks` 按键绑定由引擎 input 模块
  （固定映射）取代，无需迁移。

## 6. 工具与文档索引

| 工具/文档 | 位置 | 状态 |
|---|---|---|
| xp3 解包器 | R111-A 原型 | 开发中（迁移期工具，非运行时） |
| tlg 解码器 | R111-B 原型 | 开发中（迁移期工具，非运行时） |
| .ks 转换器 | `scripts/kag3_import.lua` | 可用；`--asset-prefix` 为待裁决扩展 |
| 静态校验 | `scripts/ks_check.lua` | 可用（CI 门禁） |
| 资源格式 | `docs/guides/asset-pipeline.md` | 权威 |
| 播放验证模式 | `scripts/verify_sample_game.sh` + `tests/scripts/sample_game_headless.lua` | 可用范例 |
| 打包 | `docs/guides/carc-packaging.md` + `tools/carc_pack` | 可用 |
