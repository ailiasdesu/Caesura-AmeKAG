# KAG3 脚本导入器（kag3_import）

> 生态入口：把停更引擎（KiriKiri/KAG3）的存量 `.ks` 脚本批量迁移到
> KAG Neo-Genesis 格式。纯 Lua 工具，零 C++ 依赖，可在任意环境运行
> （编辑器、CI、命令行）。

## 为什么需要导入器

Caesura 的 tokenizer 是 KAG3 语法的超集，运行时也有兼容别名层（`[bgm]`/
`[se]`/`[delay]`/`[select]` 等），但存量 KAG3 脚本直接运行仍有三个坑：

1. **`&var` 文本嵌入**：KAG3 文本中的 `&f.hp` 直接嵌入变量值；引擎的
   schema 插值只认 `$f.hp` / `%f.hp%` / `${expr}`，`&f.hp` 会原样显示。
2. **TJS 表达式**：`[if exp="f.hp > 10 && f.flag != 0"]` 的 `&&`/`||`/`!`/
   `!=`/三目运算需要运行时翻译层；导入器静态翻译为 Lua 语义
   （`and`/`or`/`not`/`~=`），输出脚本不依赖运行时翻译。
3. **不支持的 KAG3 命令**：`[chara_show]`/`[motion]`/`[btndef]` 等引擎
   没有等价物，scheduler 会把未知命令**静默渲染成对话文本**——导入器
   逐条报告（带行号 + 建议），绝不静默。

## 用法

```bash
# check 模式（默认）：只报告，不写文件
lua scripts/kag3_import.lua scene.ks
lua scripts/kag3_import.lua scenario/*.ks          # 批量

# convert 模式：写出导入后的 .ks（<原名>.imported.ks）
lua scripts/kag3_import.lua -o out/ scenario/*.ks

# strict 模式：存在不支持命令/iscript 块时退出码 2（CI 门禁）
lua scripts/kag3_import.lua --strict scene.ks
```

退出码：

| 码 | 含义 |
|---|---|
| 0 | 干净（或仅有可自动转换项） |
| 1 | 文件无法打开 / 解析失败 |
| 2 | `--strict` 且存在不支持命令或 iscript 块 |

## 输出示例

```
=== scenario/demo.ks ===
tokens: 13 (0 text, 2 labels, 0 iscript)
converted: 3 (&var embeds 1, expressions 1, renames 1)
unsupported: 1
  line 7: UNSUPPORTED [chara_show]: use [fg storage=...] or [sprite_fade] for enter animations
conflicts (advisory, pass-through KNOWN): 1
  line 9: NOTE [palette]: KAG3 [palette] selects the message-window palette by index; the engine [palette effect=...] is LUT color grading...
iscript blocks: 1 -- TJS code needs manual porting to Lua
  line 14: [iscript] ... [endscript]
```

## 转换规则

| 输入（KAG3） | 输出（Neo-Genesis） | 说明 |
|---|---|---|
| `&f.x` / `&tf.x` / `&sf.x` / `&mp.x` / `&lf.x`（文本与 text 类参数） | `%f.x%` 等 | 逻辑 `&&` 不受影响 |
| `exp=`/`expr=`/`cond=` 参数中的 TJS 表达式 | Lua 语义 | `&&`→`and`、`\|\|`→`or`、`!`→`not`、`!=`→`~=`、三目展开；字符串字面量安全 |
| `[waitse]` | `[waitsound]` | 语义等价的重命名 |
| `[goto]` | `[jump]` | 严格别名（同语义跳转） |
| `&N` / `&name`（`[macro]` 体内/宏体参数引用） | `%N%` / `%name%` | 宏展开时从调用参数填充；命名空间变量（`&f.x`/  `&kag.status`）被屏蔽、不受影响 |
| 参数名别名：`[add/sub/mul/div/mod/dec var=...]` → `name=`；`[csp]/[csl] left/top=` → `x/y=` | 参数名改写 | KAG3 参数名 → 引擎契约参数名（数学命令引擎读 `name`，角色定位引擎读 `x/y`） |
| 引擎已支持的命令（schema 契约 + 流程命令 + kag 处理器） | 原样保留 | 已知命令集 = `schema.dumpContracts()`（当前约 107）+ `FLOW_COMMANDS` 跳转/分支/宏命令 + `kag` 处理表；运行时兼容层已覆盖 |
| 名称与引擎命令相同但语义不同的 KAG3 命令（如 `[palette]`） | 通过（不阻断）+ **非阻断冲突提示** | 见下节「命名冲突提示」 |
| `[chara]`/`[chara_show]`/`[motion]`/`[btndef]` 等 | **保留原样 + 报告** | 见下节建议 |
| `[iscript]` TJS 块 | **保留原样 + 报告** | TJS 代码无法自动转换，需人工改写为 Lua |

convert 模式基于 tokenizer 字节偏移重建文件：注释、空白、`[iscript]`
块体逐字保留，只重写被转换的 token。

> **宏体内参数引用**：`&N` / `&name` 只在 `[macro]...` 到 `[endmacro]`
> 之间的 text token 里转换（KAG3 宏体约定）；正文中的命名空间系统变量
> （`&f.x` 等）由 `convertAmpVars` 单独处理，二者互不误伤。

### 命名冲突提示（CONFLICT_NOTES）

有些 KAG3 命令**名称与引擎命令相同但语义不同**（如 KAG3 的 `[palette]`
按索引选消息窗配色，引擎的 `[palette effect=...]` 是 LUT 色彩分级）。
这类命令按已知命令**通过（非阻断）**，但会打印一条
`conflicts (advisory, pass-through KNOWN):` 提示（带行号）告诉作者语义
已变，避免静默语义错位。导入绝不对此类命令硬失败；退出码仍只由
`--strict` 下的**不支持命令**与 **iscript 块**决定。

## 不支持命令的处理建议

导入器内置常见 KAG3 命令的建议映射（`scripts/kag3_import.lua` 的
`SUGGESTIONS` 表）：

- 立绘：`[chara_show]`/`[chara_hide]`/`[chara_modify]` → `[fg storage=]` +
  `[sprite_fade]`/`[sprite_swap]` 精灵家族
- 动作：`[motion]`/`[waitmotion]` → `[sprite_move]`/`[sprite_scale]` +
  `[wait]`
- 按钮：`[btndef]`/`[btn]` → `[button text= target=*label] ... [endbutton]`
- 存档：`[savegame]`/`[loadgame]` → `[save slot=N]`/`[load slot=N]`
- 特效：`[monocro]` → `[palette]`（自定义 LUT）或 `[flash]` 色调（脚本当前无单色命令，建议如上）
- TJS 桥：`[user]`/`[syscall]` → `[iscript]` Lua 改写
- 其余：`--strict` 下导入器以退出码 2 阻止合并，直到人工处理完毕

## 与 ks_check 的关系

导入器负责"转换 + 报告不支持的命令"；`scripts/ks_check.lua` 负责"契约
静态校验"（参数类型/必填/枚举）。工作流：

1. `kag3_import.lua --strict -o out/ scenario/` → 处理所有报告项
2. `ks_check.lua out/*.imported.ks` → 0 violations 后合入

## 测试

`tests/scripts/test_kag3_import.lua`（注册于 `run_lua_tests.lua`，当前 93 条断言通过）：
`&var` 转换、`&&` 保护、TJS 翻译、命令映射、行号报告、convert 重建
（注释保留 + 可重新 tokenize）、退出码逻辑、缺文件错误。
