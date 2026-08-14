# G5 Web 播放器验证 spike（round 31）

wasmoon 运行时验证：证明 Caesar 的 KAG 执行栈（纯 Lua 部分）可在浏览器 Lua 运行时
（wasmoon 2.0.0-next.0，Lua 5.5 内核）中原样运行，无需修改脚本。

## 运行

```bash
cd spike
npm install wasmoon@2.0.0-next.0   # 1.16 在 Node 24 有 environ_get 断言 bug
node spike1_tokenizer.mjs          # tokenizer + lpeg（纯 Lua）tokenize 三个 demo
node spike2_compiler.mjs           # kag.compiler 编译前端（schema/expr/operation）
node spike3_scheduler.mjs          # 完整调度器：tokenize→compile→scheduler.run
node spike4_kag_real.mjs           # 真实 kag 命令表 + JS 绑定适配层（MVP 核心原型）
```

## 结果

| Spike | 内容 | 结果 |
|---|---|---|
| 1 | tokenizer + lpeg 原样运行 | PASS（3 个 demo .ks 全部 tokenize） |
| 2 | 编译前端（schema/expr/compiler/operation） | PASS（55 tokens / 3 labels） |
| 3 | 完整调度器执行 demo 场景 | PASS（55 tokens 推进到 53，命令 handler 全链路调用） |
| 4 | **真实 kag 命令表 + JS 绑定适配层** | PASS（9 命令模块加载，demo 全 55 tokens 零错误执行，good_end 结局解锁，20 次绑定调用真实对接：load_texture/audio_play/set_layer_image） |

## 关键发现

### 1. Lua 5.4 → 5.5 语义差异：通用 for 循环变量 const 化

Lua 5.5 将通用 for 的循环变量视为 const：`for x in ... do x = ... end` 在 5.5 报
`attempt to assign to const variable`，5.4 合法。引擎脚本有 4 处此模式：

| 文件 | 行 | 修复 |
|---|---|---|
| scripts/kag/compiler.lua | 335（macro args 拆分） | raw_a 循环 + local a |
| scripts/kag/schema.lua | 174（list 参数拆分） | raw_item 循环 + local item |
| scripts/ks_bake.lua | 106（Windows dir 列表） | raw_line 循环 + local line |
| scripts/scheduler.lua | 939（运行时 macro args） | raw_a 循环 + local a |

修复保持 5.4 行为不变（Lua 套件 120+29 全绿），同时兼容 5.5（wasmoon/Web）。
防回归测试：tests/scripts/test_compiler.lua 10a-10d（4 例新增，39/39 全过）。

### 2. wasmoon 版本选择

- **wasmoon 1.16.0**：Node 24 下 environ_get 断言崩溃（Aborted(Assertion failed)）
- **wasmoon 2.0.0-next.0**：修复该问题，API 变更为 Lua.load() → createState()
- 运行时内核是 **Lua 5.5**（非 5.4）——这是 const 语义差异的根源

### 3. 模块加载模式

```js
// 把 scripts/*.lua 读入 package.preload（剥离 BOM）
for (const [name, rel] of Object.entries(mods)) {
  const src = readFileSync(new URL(rel, import.meta.url), 'utf8')
  lua.global.set('__PRELOAD_' + name.replaceAll('.', '_'), src)
}
// Lua 侧注册
package.preload[name] = function()
  local src = _G['__PRELOAD_' .. safe]
  return assert(load(src, '@' .. name .. '.lua', 't', _ENV))()
end
```

### 4. 绑定 stub 面

调度器本身零绑定依赖（全通过 kag 命令表间接调用）；命令 handler 需要 backend.*
（32 函数）/ layers.*（20 函数）+ mods.resolve / i18n.localize 等。Web 播放器的
JS 适配层只需实现这些函数的口（多数为 no-op 或 DOM/WebAudio 映射）。

### 5. JS 适配层契约（spike4 实测）

1. JS 适配层：backend.* / layers.* 核心子集（bg/fg/ch/text/playbgm/playse/wait）
2. kag/init.lua 组合根替换：spike3 用 stub kag 表，MVP 需要真实命令表加载
3. 剧本→JSON 预编译（ks_bake --web）
4. IndexedDB 存档 + woff2 字体子集化

---
*round 31 spike，验证结论：G5 路径 B 可行性确认*
