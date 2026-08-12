# 2026-08-12-004 — 引擎后续更新计划：代差路线图（vs 市面引擎）

> 用户目标（2026-08-12）：完成所有缺口后继续多轮迭代，直到引擎**全面领先
> 主流 galgame 引擎**——不是简单优于，而是**形成代差**（generation gap）。
> 约束：**不支持 XP3**（CARC 已更先进，归档代差由 CARC/Ed25519 承担）。

## 1. 现状（2026-08-12 实测）

**已交付的缺口**：P0-2 生态配套（CONTRIBUTING/Issue·PR 模板/example_game）、
P1-4 KAG3 导入器、P1-5 rollback -85.5%、P2-7 编辑器前端 + Electron 主进程
（进行中）、Neo-Genesis 核心重构 Phase A/B/C（编译式指令流）。
**未完成**：P0-1 发布就绪（GL/Metal 需硬件）、P0-3 移动管线、P1-6
（GL/Steam 实机）、P2-8 教程体系、P2-9 E-mote 替代。

**性能基线**：tokenizer 52ms/1000tok、scheduler ~308k tok/s；表达式路径
编译后 -32%（165ms vs 241ms）；长循环 O(n²)→O(1)。测试：Lua 103/103。

## 2. 市面引擎共性短板（代差机会）

| 短板 | Ren'Py | 吉里吉里系 | WebGAL/Tyrano | 代差机会 |
|---|---|---|---|---|
| 脚本执行模型 | Python 解释 | TJS 解释 | 浏览器解释 | **编译式指令流 + 字节码持久化** |
| 工具链 | Launcher/第三方 | 无 | 拖拽黑盒 | **原生 IDE（Electron+Monaco）+ 调试器 + 静态校验** |
| 确定性工程化 | 无 | 无 | 无 | **确定性回放/快照测试/模糊测试** |
| 回滚 | 回放式历史缓存 | 内置 | 有 | **token 级快照回滚（已交付）** |
| 脚本遗产迁移 | 语法不同 | 停更 | 无 | **KAG3 导入器（已交付）** |
| 归档 | 无统一 | XP3（停更生态） | 无 | **CARC + AES-256-GCM + Ed25519（已交付，代差）** |
| 内容资产管线 | 无原生 | SpriteStudio 依赖 | 无 | **Live2D 引擎级 + minigame 3D + 资源热更** |

## 3. 代差路线图（五大战役，按代差杠杆排序）

### Battle 1 — 编译链代差（已启动，深化至字节码）
**目标**：`[标签]` 脚本从"解析→编译→解释执行"升级为**一次编译、多处复用**。
市面引擎全部停留在"每次运行重新解析/翻译"。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 1a ✅ | compiler.lua：flow 跳转表/表达式预编译/参数规范化/handler 绑定（Phase A/B/C） | 已交付：表达式 -32%、长循环 O(1) |
| 1b | **字节码持久化**：编译产物（_compiled 表）序列化为 `.ksc` 缓存文件，按 (path, mtime) 失效；热重载/场景跳转零重编译 | 二次加载 <10ms；benchmark 提升 ≥2× |
| 1c | **表达式 AOT**：`expr.evaluateTranslated` 的运行时 `load()` 改为编译期字节码缓存——会话内用 `string.dump` 字节码，跨会话用编译产物 JSON 序列化（与 1b 同一缓存机制，见风险表） | 400-if 场景较当前再 -30% |
| 1d | **宏编译期展开（验证收尾）**：Phase A 已实现宏展开的编译期识别与参数预解析；本阶段补充参数化宏在编译期内联（参数保留、运行时零 splice）并锁定行为等价 | test_macro_nested 全绿 + benchmark 无退化（与 1b 同轮执行） |

### Battle 2 — 语言层代差（Neo-Genesis 2.0）
**目标**：标签语言具备现代 IDE 语言服务（市面标签引擎无类型系统/无 LSP）。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 2a | **LSP 服务**：利用 78 命令契约（schema.dumpContracts）实现 completion/hover/diagnostics，经 RPC 暴露给 Monaco | IDE 内 `[ch` 自动补全 + 参数 hover + 契约错误红线 |
| 2b | **类型系统深化**：契约增加 `list`/`enum`/`file` 类型 + 交叉验证（storage 路径存在性） | ks_check 新增校验项 + 测试 |
| 2c | **ks_check 语言服务化**：行内 diagnostics 推送（编辑器输入即校验） | IDE 内错误标注与 CLI 一致 |

### Battle 3 — 确定性工程化代差
**目标**：任何游戏可自动化验证（市面引擎无此概念）。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 3a | **确定性回放框架**：replay.lua 扩展为"场景级快照测试"——录制 → 断言状态（f/sf/tf/backlog）→ 比对 | 新增 test_determinism 套件 |
| 3b | **模糊/属性测试**：随机 .ks 生成器（合法/畸形）驱动 tokenizer+scheduler，断言不崩溃不挂死 | 1000 随机场景 fuzz 全过 |
| 3c | **CI 确定性门禁**：回放回归纳入 ctest（确定性导出对比） | CI 新增 job 全绿 |

### Battle 4 — 创作闭环代差（IDE + 可视化 + AI）
**目标**：对标 WebGAL_Terre 图形化 + 超越（原生 IDE 体验 + AI 辅助）。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 4a 🔄 | **Electron 桌面 IDE**：自动拉起引擎 + Monaco + 调试面板 + 可视化预览 dock（主进程已写） | 双击启动 → 引擎自动运行 → IDE 连通（本机联调） |
| 4b | **可视化场景编辑**：拖拽角色/背景到画布 → 生成 `[ch]`/`[bg]` 标签；场景树 ↔ 编辑器双向同步 | 拖拽生成可运行 .ks |
| 4c | **AI 创作辅助**：基于已交付 [ai_dialog] 的本地 LLM 接口，IDE 内"AI 生成对话/续写场景" | IDE 一键生成 → 插入编辑器 → 运行 |
| 4d | **E-mote 替代（P2-9）**：骨骼/网格动画系统（自研）或 Live2D 扩展 | 设计文档 + 原型 |

### Battle 5 — 生态与平台代差
**目标**：迁移入口 + 发布闭环（硬件约束项排后）。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 5a | **P0-3 移动管线**：Android 构建脚本（NDK 交叉编译 .ks→字节码 预烘焙）+ IME 文档 | 构建脚本产出 APK（无真机验证标注） |
| 5b | **P2-8 教程体系**：getting-started 扩充 + 5 个示例场景（flow/rollback/debugger/live2d/minigame） | 文档 + 示例可运行 |
| 5c | **P1-6 硬件验证**：Live2D GL（Linux CI）、Steam 实机——需硬件，排期靠后 | CI 三平台绿 |
| 5d | **CARC 归档代差**：导入器支持 CARC 内 .ks 直接转换（归档内场景迁移） | 导入器读 .carc 场景 |

## 4. 执行顺序建议

```
当前轮：1b/1c/1d（字节码 + 表达式 AOT + 宏展开验证）→ 4a 联调（Electron）
第 2 轮：Battle 2（LSP）→ Battle 3（确定性）
第 3 轮：Battle 4b/4c（可视化 + AI 创作）
第 4 轮：Battle 5（移动/教程/硬件验证）
每轮：全量构建 + Lua ≥103 + C++ 605 + ctest ≥10/10 + 耦合 PASS + benchmark 对比
```

## 5. 门禁（每轮强制）

1. `git diff --check` 干净
2. 全量重建零错误（rm -rf build → cmake -B build → --parallel）
3. CaesuraTests 605/605、Lua ≥103/103、ctest ≥10/10（新增套件/测试只增不减）
4. `python scripts/count_coupling.py --ci` PASS
5. benchmark 无退化（tokenizer ≤52ms/1000tok、scheduler ≥308k tok/s、表达式 ≤165ms/400-if；较 08-04 基线 135ms/1000tok/308k tok/s 为收紧/对齐值，本文档门禁取代旧基线）

## 6. 风险

| 风险 | 对策 |
|---|---|
| 字节码持久化跨 Lua 版本/平台兼容 | string.dump 仅作会话内缓存；跨会话用 JSON 序列化 _compiled 表 |
| LSP 与运行时契约漂移 | schema.dumpContracts 为单一事实源（自动生成） |
| 可视化编辑生成脚本与手写不一致 | 生成器输出过 ks_check 门禁 |
| 硬件约束项（GL/Metal/Steam）阻塞 | 标注"待硬件"，不阻塞本机可闭环项 |
