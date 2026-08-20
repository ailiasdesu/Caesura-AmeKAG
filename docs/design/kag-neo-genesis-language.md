# KAG Neo-Genesis — 新一代 KAG 脚本语言

> 定位：**脱胎于 KAG3 的全新脚本迭代**。KAG Neo-Genesis 保留开发者熟悉的
> `[标签 参数=值]` 语法，抛弃 KAG3 的遗留逻辑（TJS 方言、手写协程等待、
> 五路播放入口），以声明式契约、Lua 语义、可调试性和现代工具链重建语言
> 核心。**KAG3 兼容**：为旧脚本准备的兼容层（别名、裸参数、TJS 翻译）
> 持续保留。
>
> 配套文档：`docs/design/nextgen-kag-standard.md`（标准演化）、
> `docs/api/command-contracts.md`（**123 命令权威契约**，自动生成）。

---

## 1. 与 KAG3 的关系

| 维度 | KAG3（KiriKiri 时代） | KAG Neo-Genesis |
|------|----------------------|-----------------|
| 语言内核 | TJS 方言（`&&`/`||`/`? :`/`%var%`） | Lua 5.4 语义（翻译层保留 TJS 写法） |
| 命令定义 | 硬编码 handler，参数手写解析 | **声明式 schema 契约**（类型/钳制/默认值/插值） |
| 等待机制 | `[delay]`/`[s]` 三个手写协程循环 | `[wait]` 单一契约实现 |
| 播放入口 | `[play]`/`[bgm]`/`[se]`/`[voice]` 五路 | `[play bus=...]` 统一（旧名保留为别名） |
| 赋值 | 手写 `[eval tf.x = ...]` | 声明式 `[set f.x 10]` / `[inc f.x 2]` |
| 循环 | 仅标签跳转 | 数值 `[for]`/`[break]`（KAG3 没有计数器循环） |
| 宏 | 简单文本替换 | 参数化 `[macro m args=...]` |
| 错误 | 静默 else / 黑盒 | 可见 `scene:line` 定位错误 + 未知标签警告 |

KAG3 在 2010 年前后停止维护，其 `.ks` 脚本生态（XP3 归档、SpriteStudio、
E-mote 等）长期停留在 GPL/专有双许可的旧工具链上。KAG Neo-Genesis 是
对同一语法的**现代化重建**：语言层（契约/表达式/控制流）、运行层
（调度器/热重载/调试器）与生态层（无障碍/AI/模组）全部重新设计。

## 2. 先进性：语言层

### 2.1 声明式命令契约（123 命令）

每个命令由 schema 声明（`scripts/kag/commands/*.lua`，共 13 个命令 handler 文件，
自动生成 `docs/api/command-contracts.md`）：

```lua
-- scripts/kag/commands/text.lua（节选）
_meta = {
    category = "text",
    blocking = true,
    desc = "KAG Neo-Genesis 标准文本命令",
},
name  = { type = "string", default = "" },
text  = { type = "string" },
voice = { type = "string", default = "" },
speed = { type = "number", default = 50, min = 8, max = 5000 },
```

- **类型校验**：string/number/bool 参数在进入 handler 前验证
- **钳制**：越界数值自动 clamp（如 `speed=99999` → 5000）并打印可见日志
- **默认值/插值**：缺省参数按 schema 填充；`%var%` 插值统一处理
- **未知参数警告**：拼错的参数名不再静默忽略

### 2.2 表达式系统：TJS → Lua

KAG3 的 TJS 风格表达式（`&&`/`||`/`!`/`? :`、`%tbl.key%` 表访问、
裸标识符）由 `scripts/kag/expr.lua` 翻译为 Lua，**字符串感知比较**
（`a == "3"` 不再与 `a == 3` 混为一谈）、三目运算符翻译、双重环境
（KAG3 变量帧 `lf` + 消息参数 `mp`）统一解析。翻译后的源码按
"翻译结果 + 环境标识"缓存编译产物，热路径零重复翻译。

### 2.3 完整控制流

| 结构 | 能力 | 与 KAG3 对比 |
|------|------|-------------|
| `[if]/[elseif]/[else]/[endif]` | 表达式条件 + `[elsif]` 别名 | KAG3 拼写兼容 |
| `[for i=1..10]/[break]/[endfor]` | 数值计数器循环 | **KAG3 没有**（Neo-Genesis 新增） |
| `[call *label]/[return]` | 同场景调用 + `lf` 变量帧栈 | KAG3 语义保留 |
| `[macro m args=...]` | 参数化宏 | KAG3 仅文本替换 |
| `[select]/[sel]/[endselect]` | 选择分支 | KAG3 语法兼容 |


### 2.4 声明式 UI：补间 / 布局 / 后处理（阶段 G，round 102-107）

阶段 G 为语言层新增三族**声明式能力**，全部走 schema 契约（类型/钳制/枚举/插值），
无需手写逐帧代码：

#### 2.4.1 声明式补间 `[tween]`（round 106，对标 Ren'Py ATL 最小子集）

```kag
[tween target="t0" attr=x to=900 dur=800]                    ; from 缺省 = 当前值
[tween target="e1" attr=x from=80 to=1100 dur=1500 ease=ease_in_out]
[tween target="n1" attr=alpha from=255 to=0 dur=600 wait=false]  ; 非阻塞 fire-and-forget
[tween target="t0" attr=x from=${f.base_x} to=${f.base_x + f.step * 2} dur=700]  ; 表达式插值
```

- **契约**：`target`（speaker 或图层名，_char_ 约定隐式解析）、
  `attr ∈ {x, y, alpha, scale}`、`from`（可省）、`to`（必填）、
  `dur` 100–30000、`delay` 0–30000、`ease ∈ {linear, ease_in, ease_out, ease_in_out, back_out}`、
  `wait`（阻塞=与 [wait] 同构，非阻塞=每帧钩子推进）。
- **语义**：单时间线管理器 `ctx.tweens`（delay 相/t 累计/终点精确落 to）；
  终点精确、端点定义 e(0)=0 / e(1)=1（back_out 有过冲）。

#### 2.4.2 声明式布局 `[layout]`（round 107）

```kag
[layout name="band" kind=vbox gap=10 padding=6 align=start x=290 y=200 w=360 h=180]
[layout_slot parent="band" layer="row1" index=1 size="90x30"]
[layout_place parent="band" layer="badge" x=20 y=40]   ; 绝对偏移放置
```

- **契约**：`[layout]`（name 必填 + kind 枚举 hbox/vbox/grid + gap/padding/align/cols/w/h/x/y）、
  `[layout_slot]`（parent/layer/index/size "WxH"）、`[layout_place]`（parent/layer/x/y/w/h）。
- **设计决策**：容器是**计算器不是渲染层**——recompute 后 `layers.move_layer` 写现有层
  坐标，渲染管线零改动，与 `[position]`/`[tween]` 天然组合；坐标与手写公式逐像素等价
  （settings 迁移试点证明）。

#### 2.4.3 后处理链 `[vfx ... postfx=]`（round 102）

```kag
[vfx type=vfx postfx=bloom strength=0.6 radius=2]     ; 开启/更新 bloom
[vfx type=vfx postfx=none]                            ; 关闭整条后处理链
[vfx type=vfx postfx=vignette amount=0.5]
```

- **契约**：`postfx` 枚举 {bloom, vignette, lut, softblur, none} + strength/radius/amount/lutMix
  clamp + rgb "r,g,b"；validation-only 无 default 保 legacy 语义。
- **引擎侧**：VIEW_MAIN→m_sceneRtt 重定向 + 逐 stage 全屏 pass（scratch RTT 乒乓）+
  VIEW_POSTFX=40；绑定 `Render.set_postfx` 家族（5 API）。


## 3. 先进性：运行层

### 3.1 Lua 双层混合

```kag
[expr "f.gold = f.gold + 100"]      ; 行内 Lua 表达式
[emb "math.floor(f.hp / 2)"]        ; 输出插值
[iscript]                           ; 多行 Lua 块
  local t = { 1, 2, 3 }
  f.sum = t[1] + t[2] + t[3]
[/endscript]
```

`.ks` 场景文件与 Lua 双向调用：`kag.jump`/`kag.call`/`kag.save_game`
使 Lua 脚本可以驱动 KAG 流程；`backend.*`/`layers.*` 直接 API 或 KAG
调度器两种执行模式共存。

### 3.2 可调试性

- **场景级调试器**：断点/单步/继续 + 暂停态状态检查（RPC 方法
  `setBreakpoint`/`removeBreakpoint`/`clearBreakpoints`/`continue`/
  `stepInto`/`getState`）
- **热重载**：编辑 `.ks` 文件后 `kagReloadScene` 重新解析并 remap 到
  最近标签/场景起点，无需重启引擎
- **可见错误**：表达式错误带 `scene:line` 定位；未知标签警告而非静默

### 3.3 性能

- **tokenizer**：本机实测 **4000 token 场景 ≈52ms 解析**（10 轮均值；
  机器相关）；2026-08 优化轮将冗余前缀模式从 12 组降到 3 组，解析耗时
  **-28.5%**（以 `perf(tokenizer)` 提交基准为准；基准测试由
  `tests/scripts/test_frame_bench.lua` 守护）
- **表达式缓存**：翻译 + 编译产物按 env 身份缓存，热路径零重复翻译
- **调度器**：schema 模块热路径提权（每 token 的 require 查找消除）

## 4. 先进性：生态层

| 能力 | 说明 |
|------|------|
| 无障碍 | CC 字幕、TTS 接口、色盲/高对比滤镜（deuteranopia/protanopia/tritanopia/grayscale/high_contrast） |
| AI 辅助 | `[ai_dialog]` 本地 LLM 对话集成 |
| 模组系统 | 目录/归档模组：配置合并、脚本钩子、资源覆盖 |
| 录制/回放 | 输入录制驱动自动演示；回放 + `--export-replay` 确定性导出 PNG 序列 |
| 云存档 | 存档槽位 + 云同步（HTTP REST / Steam Remote Storage），离线降级 |
| 多后端渲染 | D3D11 / OpenGL 4.3 / Metal（引擎侧完整），bgfx 统一 |

## 5. 示例场景

```kag
*start
[bg storage="scene01.png" time=500]
[ch name="Hero" text="欢迎来到 KAG Neo-Genesis。"]
[p]
; 声明式变量与数值循环（var/start/end/step 命名参数）
[set f.gold 100]
[inc f.gold 50]
[for var="i" start="1" end="3"]
  [ch name="Hero" text="计数：%f.gold% 第 %f.i% 次"]
  [p]
[endfor]
; 参数化宏（args 逗号分隔占位符，调用时按名/按位填充）
[macro show_status args="hp,mp"]
  [ch name="Hero" text="HP %hp% / MP %mp%"]
[endmacro]
[show_status hp=100 mp=50]
; Lua 混合
[iscript]
  if f.gold > 200 then
    kag.jump("*bonus")
  end
[/endscript]
*end
[ending]
```

---

> 数字口径：命令契约 **123**（`docs/api/command-contracts.md` 自动生成计数，幂等）；
> 测试基准（阶段 G 终态 / round 113）：Lua 主套件 **132** + 孤儿 **24** 全绿、
> C++ doctest **976/976**（8858 断言）、web **297/297**、editor **530/530**
> （`run_lua_tests.lua` + `run_orphan_tests.lua` + `CaesuraTests.exe`）；
> 性能数字为本机实测，绝对值随机器波动，相对提升（-28.5%）以 `perf(tokenizer)`
> 提交的基准为准。
