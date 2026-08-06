---
module: kag
tags: [expression, tokenizer, sandbox, performance, debugger]
problem_type: architecture-pattern
---

# KAG Neo-Genesis 现代化：经验与陷阱（2026-08-07）

本次迭代为 KAG 脚本语言加入现代化特性（表达式翻译、场景调试器、模组、
录制回放、无障碍），沉淀 4 类可复用经验。

## 1. TJS 表达式翻译层（kag/expr.lua）

KAG3 的 `[if exp="a && b"]` 用 TJS 语法；引擎内部是 Lua。翻译规则：

- `&&`/`||`/`!`/`!=` → `and`/`or`/`not`/`~=`（**字符串感知**：字面量内的
  运算符不翻译，避免 `"a && b"` 被破坏）
- `?:` 三元 → `(cond and (a) or (b))`——**已知边界**：then 分支为 `false`
  时 Lua `and/or` 语义与 TJS 不同（数值/字符串精确）
- 嵌套三元：按 `:` 深度计数匹配（`{...}` 大括号按计数跳过）
- **错误可见性**：翻译/求值失败打印 `scene:line`（原先静默走 else，
  脚本作者完全无感）

## 2. 裸位置参数：tokenizer 的 "1" 硬编码

LPeg 的 param 模式 `Ct(Cc("1") * C(uval))` 给**每个**裸参数都标 `"1"`——
`[set f.hp 30]` 的两个参数解析后位置相同，scheduler 展开时后写覆盖先写
（f.hp 变 30 而非 "f.hp"）。修复：`renumber_bare_params` 按出现顺序重编
号（named 参数不占位，符合 KAG3 `[tag x=1 500]` → `params.x`+`params[1]`）。

配套：schema 契约 `positional_index = N`——required 检查在该位置槽被填时
跳过，且**不写 default**（default 会遮蔽位置值）。

## 3. 沙箱 preload 顺序陷阱（suite 级事故）

`run_lua_tests.lua` 在 `test_sandbox` 之后的所有测试只能用
`package.loaded` 里的模块。**在 preload 里 require kag_runner/scheduler
会拉入整条命令模块链**，改变后续 mock C binding 的测试行为（waitsound
从"无 binding 快速返回"变成走 factory 而崩溃）。规则：

- preload 只加**零依赖**模块（`mods`/`kag_debug`/`replay`）
- 需要 io 写/require 的测试放在 `test_sandbox` **之前**（`test_mods`/`test_replay`）
- 测试 mock 的**泄漏**要清理：`test_title_entry` 的 `package.preload`
  mock 把 `package.loaded["kag_runner"]` 留成 3-key stub，测试后必须
  重新 require 真实模块
- 业务代码**不要**在热路径 require config（其初始化会触发
  backend_factory）——用 `ctx.xxx` 单一来源，config 由启动层同步

## 4. tokenizer 性能：冗余前缀模式（-28.5%）

`explicit_cmds` 原有 11 个 ordered choice（iscript/eval/se/stopse/fadebgm/
fadevoice/fadese/wait/delay/skip + cmd_pat）。其中 9 个是 **cmd_pat 的严格
子集**：`C(ident)` 在空格/`=`/`]` 处自然停止（等价 cmd_pre 的边界函数），
裸参数分支覆盖 `[wait 500]`/`[se 1]`/`[eval f.x = 1]`。删除后：

- 4000-token 基准 362ms → 259ms（64.75ms/1000tok）
- 全部 96 个 Lua 测试验证等价性

**教训**：LPeg 的 ordered choice 按声明顺序尝试，**每次失败都回溯**；
声明"特殊"模式前先验证通用模式是否已覆盖（用全套测试做等价性证明）。

## 5. 场景级调试器的 yield 语义

KAG 调度器**每个 token 都 yield**。调试暂停（`__kag_pause`）是 token 前
的额外 yield——驱动方（测试/编辑器）需要**循环 resume**：一次 resume
执行 token（返回 nil），下一次到达断点（返回 `__kag_pause`）。断点按
`scene:cmd` 或 `scene:token序号` 匹配；`token_index` 在暂停时指向**上一个
已执行** token。
