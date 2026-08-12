# 2026-08-12-002 — KAG Neo-Genesis 核心重写蓝图（现代化重构）

> 目标（用户 2026-08-12 指示）：**KAG Neo-Genesis 是脱胎于 KAG3 的
> 全新一代现代化标签语法**——不是"KAG 语法之上的更现代标准"，而是
> KAG3 的现代重构。不为强行兼容放弃效率与性能，只保证最基本兼容。
> 完成重构后继续多轮迭代，直到引擎全面领先主流 galgame 引擎（形成
> 代差，生态位面除外）。

## 0. 已确认的方向（ask 2026-08-12）

| 维度 | 决策 |
|---|---|
| 重构深度 | **语法继承·核心重写**：保留 `[标签 参数]` 形态与 `.ks` 文件，重写 tokenizer/scheduler/expr 核心；契约先行；**编译式指令流（token→bytecode 预编译）** |
| 阶段顺序 | **重构先行**：语言层重构 → 补 P0/P1/P2 缺口 → 多轮迭代到全面领先 |
| 兼容底线 | **标签名兼容·实现现代化**：常用标签名（ch/text/bg/playbgm/jump/call/if…）可直接运行；TJS 翻译层、%var% 插值、裸参数等兼容包袱**移除**，由 KAG3 导入器（已交付 `kag3_import.lua`）承担转换 |

## 1. 现状（2026-08-12 实测）

| 模块 | 行数 | 现状 | 重构方向 |
|---|---|---|---|
| `scripts/tokenizer.lua` | 253 | LPeg 超集语法；~52ms/1000tok；含裸参数/`[elsif]` 别名 | 拆为 **lexer + compiler**：纯语法→中间表示（IR），编译期做契约绑定/表达式预编译/标签索引 |
| `scripts/scheduler.lua` | 980 | 每 token 运行时 dispatch + 流程控制内联（if/switch/while/for/jump/call/macro）；运行时 coerce | 改为**执行预编译指令流**：flow 结构编译期解析（跳转表），运行时零扫描 |
| `scripts/kag/expr.lua` | 295 | TJS→Lua 运行时翻译（&&/||/!/!=/三目）+ 双环境求值 + 缓存 | **编译期翻译**：compiler 产出已翻译闭包，运行时直接调用；移除运行时 translate 热路径 |
| `scripts/kag/schema.lua` | 246 | 契约 + `$var`/`%var%`/`${expr}` 插值（%var% 为 KAG3 兼容包袱） | 保留 `$var`/`${expr}`；**移除 %var%**；插值编译期模板化 |
| `scripts/kag.lua` | 478 | 22 个 KAG3 兼容别名（r/s/delay/clear/ct/endtag/g/fadeout/ld/shake/quake/playstop/voice/se/bgm/play/waitclick…） | 保留标签名（走统一契约），实现全部现代化；别名表集中管理 |
| `scripts/kag3_import.lua` | ~500 | 已交付：&var→%var%、TJS 静态翻译、命令映射、unsupported 报告 | **升级**：输出直接生成 Neo-Genesis 2.0 语法（无 %var%/裸参数残留） |

**当前性能基线**（docs/plans/2026-08-04-006，本机实测）：tokenizer
119–135ms/1000tok；scheduler ~308k tok/s。测试：C++ 605/605、Lua 100/100。

## 2. 分阶段计划

### Phase A — 编译式指令流（compiler.lua + scheduler 改造）

**目标**：`[标签 参数=值]` 源文件一次编译为紧凑指令数组（IR→bytecode），
scheduler 执行 bytecode 零字符串处理、零运行时扫描。

设计：
```
.ks 源 → lexer(tokenizer.lua 保留语法层) → IR token 流
       → compiler.lua:
           · 参数规范化（裸参数→命名，编译期完成）
           · 契约绑定（schema 查找表：cmd→coerce 函数引用，编译期解析）
           · 表达式预编译（expr 翻译+load 在编译期，产闭包引用）
           · flow 结构解析（if/while/for/switch 的跳转目标编译期计算，
             运行时 O(1) 跳转，替代 skip_to 运行时扫描）
           · 标签索引内联（jump/call 目标=编译期下标）
           · 宏展开编译期完成（不再运行时 splice）
       → 指令流: { {op=CMD, cmd=idx, fn=handler, params={...coerced}},
                   {op=JUMP, target=idx}, {op=IF, cond=closure, t=idx, f=idx}, ... }
```

验收：
- 全量测试（Lua 100 + C++ 605）行为不变
- 新增 `test_compiler.lua`（IR/bytecode 结构、flow 跳转表、错误定位）
- benchmark：scheduler 吞吐较基线提升（目标 ≥1.5×）
- 保留 `tokenizer.parse`/`scheduler.run` 旧 API 兼容层（编辑器/ks_check 依赖）

### Phase B — 表达式与插值编译期化（expr/schema 改造）

- `expr.translate` 从运行时热路径移入 compiler；scheduler 只调用预编译闭包
- schema 插值：`$var`/`${expr}` 编译期模板化（每命令一次编译，运行时拼接）
- 移除 %var% 兼容（导入器负责转换）

验收：插值/表达式测试全绿；`test_expr_cache` 适配；benchmark 无退化。

### Phase C — 兼容层收窄（标签名保留，包袱移除）

- 裸参数：tokenizer 编译期转为命名参数（保留 `[delay 500]` 可运行，
  但 IR 中已是 `ms=500`）——或按用户"只保基本兼容"移除，由导入器转换
- TJS 运算符：运行时翻译移除，编译期翻译保留（旧脚本经导入器转换后
  直接是 Lua 语义）
- kag.lua 别名：集中到 `kag/compat.lua` 显式注册表，主路径零别名判断

验收：`test_kag3_compat` 改为"经导入器转换后运行"链路测试；
`kag3_import.lua` 输出零 %var%/裸参数残留。

### Phase D — 性能冲刺与缺口补齐（后续轮次）

- benchmark 对比（tokenizer/scheduler/表达式）锁定代差
- 然后按市场分析路线图补 P0/P1/P2 缺口（生态配套/示例游戏/编辑器前端/
  rollback 压测/GL 验证）
- 多轮迭代至全面领先（形成代差），每轮：基线 → 变更 → 全量验证 → 对比

## 3. 每轮迭代的强制门禁（不可协商）

1. `git diff --check` 干净
2. 全量重建：`rm -rf build && cmake -B build && cmake --build build --config Debug --parallel` 零错误
3. `CaesuraTests.exe` 605/605 全绿（CWD=build/tests/Debug）
4. Lua 套件 `run_lua_tests.lua` 100/100 全绿
5. `ctest -C Debug --test-dir build` 10/10
6. `python scripts/count_coupling.py --ci` PASS
7. benchmark 无退化（perf 基线对比）

## 4. 风险与对策

| 风险 | 对策 |
|---|---|
| 编辑器/ks_check/调试器依赖旧 token 结构 | compiler 保留 tokenizer API；新增 API 并存；测试锁定 |
| 宏运行时 splice 改动影响嵌套/参数化 | 编译期展开 + 预算保留；test_macro_nested 锁定 |
| 热重载/场景跳转与编译缓存交互 | 编译结果按 (path, mtime) 缓存；reload 失效重编译 |
| 兼容层移除导致旧脚本行为变化 | 导入器升级为权威转换路径；文档明示"KAG3 脚本须经导入器" |
