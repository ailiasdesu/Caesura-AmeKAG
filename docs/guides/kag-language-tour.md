# KAG Neo-Genesis 语言教程

> 面向新作者的完整语法指南。KAG Neo-Genesis 是脱胎于 KAG3 的全新一代
> 现代化标签语法——保留 `[标签 参数=值]` 形态，但以声明式契约、Lua
> 语义、可调试性和现代工具链重建。配套：`getting-started.md`（快速
> 上手）、`command-contracts.md`（118 命令权威契约）。

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
[goto target=*ending]           ; [goto] 是 [jump] 的严格别名（KAG3 兼容）
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

**调用与嵌套**（round 75 深度感知）：

- **嵌套调用**：宏体内可调用其他宏（静态安全时递归内联，参数深拷贝）：
  ```kag
  [macro wrap args="line"]
  [ch text="%line%"]
  [endmacro]
  [macro twice args="line"]
  [wrap line="%line%"]
  [wrap line="%line%"]
  [endmacro]
  [twice line="你好"]           ; 展开为两次 [ch]
  ```
- **嵌套定义**：体分析深度感知——`[macro inner]...[endmacro]` 对归入**外层**体
  （round 75 修复，旧版在第一个 `[endmacro]` 就截断产生残缺体）。含嵌套定义的
  外宏被保守标记为**动态**（运行时可能重定义名字），不做编译期内联。
- **静态安全内联**：定义在流程深度 0、调用前出现、未被 `[erasemacro]` 擦除、
  未重定义的宏，在**编译期**内联（行为与运行时 splice 完全一致）；定义在
  `[if]`/`[while]`/`[for]`/`[select]` 分支内、或任一处 `[erasemacro name]`、
  或重定义过的宏 → **动态**，运行时展开。自递归宏
  （`[macro m][m][endmacro]`）无法内联，编译/运行预算（1000 次展开）快速失败。
- **`[erasemacro name]`**：擦除宏定义。

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
[vfx postfx="bloom" strength=0.5 amount=0.8]  ; PostFx chain (bloom/vignette/lut/softblur/none)
[vfx postfx="vignette" strength=0.6 radius=0.5 rgb=255,128,64]
[vfx postfx="none"]    ; 关闭全部后处理
[shake]
```

## 12. 调试与工具

- **静态校验**：`lua scripts/ks_check.lua scene.ks`（契约检查，CI 门禁）
- **语言服务**：IDE 内补全/悬停/诊断（118 契约驱动）；`Ctrl+点击` 在
  `[jump]/[call]/[link]` 目标与 `*label` 定义间跳转（goto-definition），
  右键"查找所有引用"高亮全部导航点（references）
- **确定性执行**：`require("kag.determinism").run_scene(...)` 无 GPU 跑场景
- **导入器**：`lua scripts/kag3_import.lua --carc game.carc --path assets/script/main.ks`
  （KAG3 存量脚本迁移）

## 13. 内联文本标记（Neo-Genesis；Ren'Py `{...}` 对齐）

`[ch]` / `[text]` 消息内可直接嵌着色标记，逐段渲染：

```kag
[ch text="普通文字{color=#ff0000}红色强调{/color}回到默认"]
[text text="{color=#00ff00}绿色{/color}与{color=#0000ff}蓝色{/color}混排"]
```

规则：

- `{color=#RRGGBB}` … `{/color}`：切换/恢复文字颜色（可嵌套，内层优先）
- `{size=N}` … `{/size}`：字号（px，相对当前字体缩放渲染，影响换行布局，
  可嵌套）
- `{b}` … `{/b}`：合成粗体（字形双 pass 偏移渲染）
- `{i}` … `{/i}`：斜体（字形顶边剪切偏移渲染，倾斜约 0.18× 字高；
  与 `{b}` 可叠加）
- `{s}` … `{/s}`：删除线（字形中线横向实心条，宽=字形步进、高≈10% 字高；
  与 `{b}`/`{i}`/`{color}` 可叠加）
- `{color=RRGGBB}`（无 `#`）同样合法
- 未知 `{标签}` 按字面文本显示，不影响内容
- 回滚/历史/字幕使用去除标记后的纯文本；打字机逐字揭示按可见字符计数

## 14. NVL 模式（全屏累积文本；Ren'Py NVL 对齐）

默认 `[ch]` / `[text]` 把文字画在底部消息窗口，逐行**替换**。NVL 模式改为
全屏文本块：每行**追加**到上一行下方，直到翻页。

```kag
[nvl]                    ; 进入 NVL 模式（起始于屏幕上方）
[ch name="Ame" text="第一行：这是全屏叙事文本。"]
[ch name="Ame" text="第二行：追加在第一行下方。"]
[nvl clear]              ; 翻页：清空整页，下一行回到页首

[nvl]                    ; 重新进入
[text text="旁白也可以累积。"]
[p]                      ; 等待点击后翻页（等价于 [nvl clear]）
[nvl off]                ; 退出 NVL，回到底部消息窗口
```

规则：

- `[nvl]` 进入（或重置页首）；`[nvl clear]` 清页；`[nvl off]` 退出
- 说话人前缀格式可配：`[nvl prefix="%s："]`（`%s` = 说话人名字，
  默认 `「%s」：`；设置后持续到下次修改；名字中的 `%` 安全）
- 累积行位于屏幕上方（`x=48, y=160` 起），占满整行宽度；说话人以
  **行首内联前缀**「Name」： 显示（Ren'Py NVL 风格，颜色取
  `nameplate_style.text_color`，随行换行；空消息时回退为行内独立标签）
- `[p]` 在 NVL 模式下等价于 `[nvl clear]`：等待点击后翻页
- 打字机逐字揭示**只动画当前追加的一行**，之前的行保持完整显示
- NVL 游标复用 `text_state` 的 cursor，存档/读档/回滚自动恢复当前页与位置

## 15. 骨骼网格动画（SMA；E-mote 类动画）

引擎内置骨骼网格动画管线（`docs/design/skeletal-mesh-animation.md`）：
骨骼层级 + 顶点权重（每顶点 ≤2 根骨骼）+ 关键帧动画（rot/scale/offset
线性插值）。数据为 JSON，经 `sma.register(name, data)`（Lua/[iscript]
侧）注册：

```lua
[iscript]
local sma = require("kag.sma")
sma.register("hero", sma.load(io.open("assets/sma/hero.json"):read("*a")))
[/iscript]
```

场景内使用：

```kag
[sma_play name="hero" asset="hero" anim="idle" x=440 y=200 scale=2 tex=0]
[sma_anim name="hero" anim="walk" blend_time=0.3]   ; 运行切换（可选淡入）
[sma_ik name="hero" bone0=0 bone1=1 tx=300 ty=400 l2=80]  ; 两骨 IK 到达点
[sma_variant name="hero" part="eye" variant="closed"]      ; 部件/表情变体
[sma_stop name="hero"]
```

- 每帧自动推进动画时间并重蒙皮（引擎更新钩子已接）；绘制在图层树之后
- 枢轴烘焙与骨骼链解析在驱动侧完成；权重混合默认走**GPU 计算蒙皮**
  （S5：bgfx compute，D3D11/GL；`sma.set_skin_mode("auto"|"cpu"|"gpu")`
  可切换，Metal/SPIR-V 自动回落 CPU 软变形——数学与 CPU 逐像素等价）
- **播放控制**：`loop`（默认循环）、`rate` 倍速、`sma.pause/resume/seek/
  set_rate`、非循环动画播完自动回退 `on_done_anim`
- **高级动画**：crossfade 混合（`blend_time`）、两骨 IK（`sma.set_ik`）、
  E-mote 风格部件/表情变体（资产 `parts` + `sma.set_variant`）
- 无 GPU 环境（测试/CI）全链惰性空操作；纹理经现有纹理管线
- 文本标记 `{color}`/`{size}`/`{b}`/`{i}` 均已渲染（见 §13）

## 16. 完整示例

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

## 17. 本地化（i18n；Ren'Py translate 对齐）

对话与文本走**两级本地化管线**（`scripts/i18n.lua`，在 `[ch]`/`[text]`
进入内联标记解析**之前**应用）：

1. **逐行翻译**：语言文件的 `lines` 表按
   `"<场景>:<FNV-1a(消息)>"` 键映射译文——内容寻址，场景重排/运行时
   生成的对话不会错位。空占位符回退原文。
2. **`{key}` 令牌展开**：`{key}` 按当前语言字符串表替换；未知 key
   保留花括号（缺失翻译可见）；内联标记名（`{b}`/`{i}`/`{s}`/
   `{color}`/`{size}`）白名单豁免，绝不当作 key。

```lua
-- assets/lang/ja.lua（工具生成，翻译者填空）
return {
  fullscreen = "フルスクリーン",
  lines = {
    -- galgame_demo.ks
    ["galgame_demo.ks:21c6c4b9"] = "", -- original: Welcome to Caesura AmeKAG Engine Demo.
  },
}
```

```kag
[ch text="Welcome to Caesura AmeKAG Engine Demo."]  ; ja 语言下显示译文
[ch text="{greeting}"]                               ; {key} 令牌展开
[button text="Go to the library"]                    ; 选择按钮标签同样本地化
```

**覆盖范围**：`[ch]`/`[text]` 对话、`[button]`/`[sel]` 选择按钮标签
（注册时本地化，`[endbutton]` 绘制与点击判定用译文）；标题/设置等
菜单文本经 `i18n.t` 字符串表（语言切换即时生效）。

**作者工作流**（`scripts/ks_i18n.lua`）：

```bash
lua scripts/ks_i18n.lua --dir demo --lang ja --out assets/lang/ja.lua
lua scripts/ks_i18n.lua --dir demo --lang ja --update   ; 合并，保留已有译文
lua scripts/ks_i18n.lua --missing --dir demo --lang ja  ; 未翻译清单（CI 门禁：有缺失退出码 1）
```

- 提取 `[ch]`/`[text]` 的 text/message 与裸文本行，生成哈希键模板
  （附 `-- original:` 原文注释供翻译者参考）
- `--update` 合并模式保留已有译文与 settings 等手写键
- 运行时语言切换：设置菜单 Language 热切换（`i18n.load`）；语言随
  存档持久化（save.lua 存 `state.language`，读档恢复）
- 内置 zh/en/ja 界面词条；语言文件同时支持手写 `{...}` 字面量与工具
  生成的 `return {...}`（含注释头）两种形态

**复数形态（G9）**：字符串表的值可以是**复数变体表** `{ one=..., other=... }`，
经 `i18n.translate(key, { n=count })` 按 `i18n.plural_category(count)` 选择变体并
填入 `{n}` 占位符（en：1→`one`，其余→`other`；zh/ja 恒为 `other` 单一形式）：

```lua
-- assets/lang/en.lua
return { items = { one = "{n} item", other = "{n} items" } }
```

```lua
-- [iscript]/[emb] 内（scripts/i18n.lua）
i18n.translate("items", { n = 3 })   -- en: "3 items"；zh/ja: "3"
```

无 `params.n` 时回退 `other`（通用）形式，`{n}` 仍可插值。

**运行时热切换整页重绘**（**超 Ren'Py**：Ren'Py 已显示行保持原语言，
本引擎切换后整页跟随新语言）：

- 每个 `[ch]`/`[text]` 在显示时把**本地化前**的源文本（插值后）连同
  布局参数记录进 `text_state.page_src`（与 draws 平行，随页面清空）
- 设置菜单 Language 切换 → `i18n.load` → `relocalize_page(ctx)`：
  当前页（消息窗 / NVL 累积页）按新语言重放重绘（译文变长折行时后续
  行自动级联下移）；backlog 历史、激活的选择按钮标签、cc 字幕同步重译
- 重绘行立即全显（typewriter 封存）；未翻译行回落原文；backlog 条目
  随存档持久化 `src`（旧档无 `src` 的条目保持原样）；说话人名牌/
  `[ruby]` 不参与（管线本就不译）

## 18. AI 辅助（本地 LLM；Neo-Genesis 差异化）

引擎通过 C++ 绑定（`AI` 表）访问 OpenAI-compatible / Ollama HTTP 服务，
全部走 `config.ai` 配置（scripts/config.lua）：

```lua
config.ai = {
    endpoint = "http://127.0.0.1:11434",  -- Ollama 默认；"" 禁用
    model = "",   -- "" = 自动：Ollama 端点询问服务取首个可用模型
    api_key = "", -- OpenAI-compatible 可选 Bearer
    timeout_ms = 60000,  -- HTTP 读超时（冷加载预留）
}
```

**能力面**：

| 能力 | 入口 | 说明 |
|---|---|---|
| 游戏内 AI 对话 | `[ai_dialog prompt="..." fallback="…"]` | 异步查询（渲染循环不阻塞），超时/不可达优雅回退 fallback 文本 |
| 场景创作 | IDE AiPanel / `kag/aiwriter.lua` | 生成对话（`generate_dialogue`）、续写场景（`continue_scene`），输出过 KAG 标签消毒 |
| 开发辅助 | IDE AiPanel Dev Assist / `kag/aidev.lua` | 诊断解释、修复建议、场景结构审查（本地规则离线可用，LLM 在线增强） |

**模型自动发现**（Ollama 端点且 model 为空时）：绑定向服务发
`GET /api/tags` 取第一个可用模型（进程内缓存）——不再假设一个
必然没拉取的默认模型。**真实验证**：引擎 --headless 经 RPC eval 对
真实 Ollama（gemma3:4b）同步/异步查询均返回真实回复；条件式 C++
用例与 ctest `CaesuraHeadlessAiSmoke`（无 Ollama 时跳过）作为回归。

## 19. round 71-82 新命令速查

> 本教程上文已覆盖多数命令的基本用法；这里集中给出 round 71-82 新增/变更
> 命令的速查（详细契约见 `docs/api/command-contracts.md`）。

### 19.1 变量算术链（KAG3 兼容）

`[add]`/`[sub]`/`[mul]`/`[div]`/`[mod]` 都对数值变量做就地算术（`v = v op value`），
`[dec]` 自减（默认 `-1`）。见 `demo/tutorial/tutorial_13_commands.ks`：

```kag
[add name="f.n" value=5]     ; f.n = f.n + 5
[sub name="f.n" value=3]     ; f.n = f.n - 3
[mul name="f.n" value=2]     ; f.n = f.n * 2
[div name="f.n" value=4]     ; f.n = f.n / 4
[mod name="f.n" value=3]     ; f.n = f.n % 3
[dec name="f.n"]             ; f.n = f.n - 1（amount 默认 1）
```

### 19.2 角色立绘命令（KAG3 兼容）

- `[csp name=... layer=... x=... y=... storage=...]`：显示角色立绘（默认
  `assets/char/<name>.png`；`storage=` 可覆盖路径，`layer` 默认 `0`）
- `[csl ...]`：移动已显示的角色层到新坐标（不改可见性）
- `[csd ...]`：清除角色层

后端缺图/纹理缺失时各 handler 带守护，加载失败仅诊断并跳过，不中断剧本。

### 19.3 打字速度（字符/秒）

```kag
[textspeed cps=40]            ; KAG3 兼容：按字符/秒设置（floor(1000/cps) ms/字）
[cps 60]                      ; [textspeed] 别名，可接收裸参数
[pt speed=60]                 ; 恢复为每字符毫秒控制
```

### 19.4 色调节制与震动

- `[palette effect=day|night|toggle]`：LUT 色调滤镜（`scripts/palette.lua`；
  round 72 起后端未注册 `set_palette` 时安全降级不再崩溃）
- `[vibrate time=300]`：消息层震动，`[vib]` 别名（阻塞）

### 19.5 通知与预加载

```kag
[notify msg="一条通知" time=1000]            ; 作者上屏角落吐司（时间毫秒）
[preload path="assets/bg/hana.png"]         ; 预缓存纹理/音频/场景（type=texture/audio/scene）
```

### 19.6 本地化热切换命令

`[i18n language="ja"]`：场景中途热切换界面语言（等价设置菜单 Language 切换，
触发整页重译重绘）。复数形态见 §17。

### 19.7 [tween] 声明式补间（round 106 / R106-A）

[tween] 是 sprite 属性补间命令：把某个图层上的数值属性（x / y / alpha /
scale）在 N 毫秒内从起点平滑插值到终点。较 [move]（仅 x/y）更通用，内置
5 种缓动 + ${expr} 起点/终点 + 延迟 + 阻塞/非阻塞，取代手写序列。契约：

```kag
[tween target="t0" attr=x to=900 dur=800]        ; 基础：from 缺省=当前值
[tween target="e1" attr=x from=80 to=1100 dur=1500 ease=ease_in_out]
[tween target="n1" attr=x from=0 to=1200 dur=1000 wait=false]  ; 非阻塞
[tween target="n1" attr=alpha from=255 to=0 dur=600]
[tween target="n1" attr=scale from=1.0 to=1.6 dur=800 ease=back_out]
[tween target="t0" attr=x from=${f.base_x} to=${f.base_x + f.step * 2} dur=700]
[tween target="t0" attr=x from=1200 to=80 dur=700 delay=400]  ; 延迟启动
```

- `target`（必填）：目标图层名或说话人（说话人回落 `_char_<name>` 图层；缺失时打印诊断并安全返回）
- `attr`（必填）：x / y / alpha（0..255 不透明度）/ scale（1.0=原大）
- `from`／`to`：起点（缺省=当前值）／终点（必填）；均支持 `${expr}` 插值
- `ease`：linear / ease_in / ease_out / ease_in_out / back_out（默认 linear）
- `dur`（必填）：毫秒（契约钳制 100..30000）；`delay`：延迟启动毫秒（默认 0）
- `wait`：true=阻塞协程（默认）；false=非阻塞，配 `[wait ms=...]` 卡点

教程见 `demo/tutorial/tutorial_16_tween.ks`。实现位于
`scripts/kag/commands/tween.lua`（含 `tests/scripts/test_tween.lua`），
`kag_runner` 每帧钩子（`TweenCommands.update`）已接线。⚠️ 该模块尚未登记进
`kag/init.lua` 预加载清单，故 [tween] 在 ks_check 仍判为未知命令、运行时不可用；
登记完成后再回填 `docs/api/command-contracts.md` 并把 tutorial_16 加入
`web/flow.integration.test.js` 教程扫描清单，跑验证后去掉本标注。

## 20. 表达式运算符全表

`[if]`/`[while]`/`[switch exp=]`/`[eval]`/`[assert]`/`${}`/`%tbl.key%` 表达式走统一
TJS→Lua 翻译层（`scripts/kag/expr.lua`，字符串字面量内不翻译）：

| 运算符 | 含义 | TJS 写法 | Lua 等价 |
|---|---|---|---|
| 逻辑与 | `&&` | `f.hp > 0 && f.flag` | `A and B` |
| 逻辑或 | `\|\|` | `sf.x == 1 \|\| f.y` | `A or B` |
| 逻辑非 | `!` | `!tf.locked` | `not A` |
| 不相等 | `!=` | `f.name != 'Aoi'` | `A ~= B` |
| 三元 | `?:` | `f.hp > 20 ? 'high' : 'low'` | `(A and (B) or (C))` |
| 空合并 | `??` | `f.missing ?? 42` | `A or B` |

详解见 `docs/api/kag-expression-language.md`。要点：

- 三元对数字/字符串精确；对布尔走 `and/or` 语义（`false` 的 then 分支会穿过，
  需要时用括号比较或 `??`）。
- 三元可出现在 `[...]` 索引与 `(...)` 组内（先内层展开）：
  `f.arr[f.flag ? 1 : 2]`；赋值 RHS 也走全管道：`f.pick = f.flag ? 1 : 2`。
- `[switch exp=...]` 表达式选择器按 `tostring` 等值匹配 `[case v]`（数值/布尔均可）。
- `[eval]` 裸值表达式（无 `=` 赋值）自动包 `return`，结果存 `tf.eval_result`。

