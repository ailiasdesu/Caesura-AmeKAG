# KAG Neo-Genesis 命令规则（新一代 KAG 标准）

> 2026-08-05 · 根本改进 KAG3 老旧逻辑：声明式命令契约。
> 本引擎不是 KAG3 的现代重构，而是在 KAG 语法之上的更现代标准——
> 代号 **KAG Neo-Genesis**：保留开发者熟悉的标签语法，抛弃 KAG3
> 不合理的老规则。

## 一、抛弃的 KAG3 老逻辑

| KAG3 老规则 | 问题 | 新一代替代 |
|---|---|---|
| 参数全是字符串，命令内 `tonumber(params.x) or default` 散落 | 每个命令重复解析、错误静默吞掉 | **声明式契约**：类型在 `schema.define` 声明一次 |
| 坏输入静默 fallback 默认值 | `[pt speed=abc]` 无提示 | **类型错误报错**（含 `cmd@scene:token` 位置） |
| 无参数范围概念 | `[wait time=99999999]` 冻结 27 小时 | **契约钳制**（min/max 声明） |
| 参数拼写错误静默忽略 | `[ch tex=]` 无效果无提示 | **未知参数警告** |
| 布尔无统一解析 | `"true"/"1"/"yes"` 各处不同 | **统一 boolean 类型**（true/1/yes + false/0/no） |
| 无必填校验 | 缺失参数运行到深层才炸 | **required 声明 + 提前报错** |
| 无枚举约束 | `[trans method=whatever]` 静默 | **choices 校验** |

## 二、新一代规则（Schema.define）

```lua
-- scripts/kag/schema.lua
Schema.define("pt", {
    speed = { type = "number", default = 50, min = 8, max = 5000 },
})
Schema.define("playbgm", {
    file   = { type = "string", required = true },
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
    loop   = { type = "boolean", default = true },
})
```

- 类型：`number` / `boolean` / `string`
- 约束：`default` / `min` / `max` / `choices` / `required`
- 调度：scheduler 在 dispatch 前统一 `Schema.coerce`（类型转换+钳制+校验）
- 报错：`cmd [pt]@scene.ks:42: param 'speed' expects a number, got "abc"`
- 增量：未迁移命令透传（行为不变），命令逐个迁移

## 三、防挂防护（无限执行防护）

| 面 | 机制 | 上限 |
|---|---|---|
| `[iscript]`/`[eval]` Lua 指令 | C++ `lua_sethook` 指令计数（LuaManager） | 200 万指令/帧（每帧重置；超出 `luaL_error` 强制展开） |
| `[while]` 循环 | `_whileIterByScene` 迭代计数（Lua scheduler） | 上限触发报错（含场景定位） |
| `[for]` 循环 | `_forIterByScene` 迭代计数（`step=0` 退化为 1） | 同上 |
| `[wait]`/`[delay]` 阻塞 | 显式别名优先 + handler 钳制 | 60s 上限（裸值绕过 schema 也钳制） |
| `[waitsound]`/`[waitbgm]` | 有界等待循环 | 60s 上限 |
| `[video]` 等待 | 播放状态循环 | 60s 上限（loop 视频自动停） |
| `[trans]` 转场 | elapsed 循环 | duration 有限（schema 30s 上限） |

测试锁定：`test_lua_manager.cpp`（指令预算 round-trip/无限循环中断/实例隔离）、
`test_while.lua`（guard 触发）、`test_for.lua`（step=0 退化 + guard）。

## 四、已迁移命令（20）

| 批次 | 命令 |
|---|---|
| 1 | pt, wait |
| 2 | scroll, trans, move, quake |
| 3 | playbgm, playse, stopbgm, stopse, fadebgm, fadevol |
| 4 | ch, text, ruby, font |
| 5 | position, layopt, fadeout |
| 6 | particles（22 参数） |

## 五、收益

- **可靠性**：坏参数从"静默错误"变"定位报错"（crafted/typo 脚本快速暴露）
- **可维护性**：命令契约集中声明，处理体只读类型化参数（tonumber 减少 40+ 处）
- **可文档化**：契约即 API 文档（类型/范围/默认值自动可生成）
- **可扩展**：新命令先声明契约再写实现；工具链（编辑器/校验器）可直接读契约

## 五、演进状态（2026-08-05 更新——路线图 1-4 已完成）

- ✅ 剩余命令迁移：全部命令族契约化（27 命令）
- ✅ 契约→API 文档自动生成（schema_doc.lua → docs/api/command-contracts.md）
- ✅ 脚本静态校验器（ks_check.lua + LPeg Cp 字节偏移 + CI 三平台门禁）
- ✅ 表达式插值（`$f.var` + `${expr}` 完整表达式）
- ✅ 命令返回值（`[eval]` 存 `tf.eval_result` → `${tf.eval_result}` 展开——表达式上下文闭环）

## 六、命令重构（新一代精简）

| 重构 | 说明 |
|---|---|
| [delay]/[s] → [wait] | KAG3 三个手写协程等待循环统一为一个契约实现（-26 行） |
| [play bus=] | KAG3 的 play/bgm/se/voice 五个入口统一为一个命令（bus choices 契约） |
| [bgm] 别名 | 保留 KAG3 兼容，绑定统一 [play]（bus 契约生效） |
| [se]/[voice] 别名 | 保留 KAG3 兼容，直调 audio 模块（不经 play 契约） |
| [cl] 契约化 | clear/ct/clearscreen 别名统一入口契约 |
| [auto mode=] | 显式 on/off/toggle（KAG3 仅 toggle） |
| [voice_off] | 静音命令（保 voice_end 防卡——KAG3 需 stopvoice+设置） |
| 测试门禁真实化 | 8 测试文件 results 局部化 + 退出门禁（FAIL 静默漏洞全闭） |
