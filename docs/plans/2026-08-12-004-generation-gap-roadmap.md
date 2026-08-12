# 2026-08-12-004 — 引擎后续更新计划：代差路线图（vs 市面引擎）

> 用户目标（2026-08-12）：完成所有缺口后继续多轮迭代，直到引擎**全面领先
> 主流 galgame 引擎**——不是简单优于，而是**形成代差**（generation gap）。
> 约束：**不支持 XP3**（CARC 已更先进，归档代差由 CARC/Ed25519 承担）。

## 1. 现状（2026-08-12 实测）

**已交付的缺口**：P0-2 生态配套（CONTRIBUTING/Issue·PR 模板/example_game）、
P1-4 KAG3 导入器、P1-5 rollback -85.5%、P2-7 编辑器前端 + Electron 主进程、
Neo-Genesis 核心重构 Phase A/B/C（编译式指令流）+ 1b 字节码持久化、
**P0-3 移动管线**（Android 构建脚本 + IME 文档 + .ksc 预烘焙）、
**P2-8 教程体系**（语言教程 + 演示场景）、
**P2-9 E-mote 替代设计**（SMA 设计定稿 + S1 接口）。
**未完成**：P0-1 发布就绪（GL/Metal 需硬件）、P1-6（GL/Steam 实机）、
P0-3 真机验证（待设备）、4a 运行时联调（待网络，见 §3 与交接 §3）。

**性能基线**：tokenizer 52ms/1000tok、scheduler ~308k tok/s；表达式路径
编译后 -32%（165ms vs 241ms）；长循环 O(n²)→O(1)；.ksc 预烘焙首载
737ms→25ms（29×）。测试：Lua 112/112、C++ 609/609。

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
| 1b | **字节码持久化**：编译产物（_compiled 表）序列化为 `.ksc` 缓存文件，按 (path, mtime) 失效；热重载/场景跳转零重编译 | ✅ 已交付（3f3646f2）：Lua-literal 格式 5ms/2500token（JSON 7× 提速）、FNV-1a hash 失效、cache/ksc 隔离、sandbox 容错 |
| 1c ✅ | **表达式 AOT**：`expr.evaluateTranslated` 的运行时 `load()` 改为编译期字节码缓存——会话内用 `string.dump` 字节码，跨会话用编译产物 JSON 序列化（与 1b 同一缓存机制，见风险表） | ✅ 已交付（本轮）：compile 预生成 string.dump 存 `_compiled.exprDumps`（[for] 三表达式独立子表）、evaluateTranslated 走 load(bc,'b') 路径（实测冷 1.35×/热 2.18×，符合 -30% 目标）、dump 失败回退源码、dump_cache 会话内共享 |
| 1d ✅ | **宏编译期展开（验证收尾）**：Phase A 已实现宏展开的编译期识别与参数预解析；本阶段补充参数化宏在编译期内联（参数保留、运行时零 splice）并锁定行为等价 | ✅ 已交付（本轮）：`compiler.inlineStaticMacros` 静态安全宏编译期内联（flow 嵌套外定义/先于调用/无 erasemacro/无重复定义），%arg% 参数保留+嵌套递归展开，动态宏保持运行时 splice；test_macro 12/12、nested 5/5、bare 6/6 全绿 + benchmark 无退化（无宏场景快速路径 0.38ms） |

### Battle 2 — 语言层代差（Neo-Genesis 2.0）
**目标**：标签语言具备现代 IDE 语言服务（市面标签引擎无类型系统/无 LSP）。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 2a | **LSP 服务**：利用 78 命令契约（schema.dumpContracts）实现 completion/hover/diagnostics，经 RPC 暴露给 Monaco | ✅ 已交付（dfb254f5）：kag/lsp.lua + Monaco providers + eval 桥接（零 C++ 改动） |
| 2b | **类型系统深化**：契约增加 `list`/`enum`/`file` 类型 + 交叉验证（storage 路径存在性） | ✅ 已交付（bca6b925）：list 列表转换/enum 枚举校验/file 资产路径静态+运行时双重验证；7 处生产契约升级 |
| 2c | **ks_check 语言服务化**：行内 diagnostics 推送（编辑器输入即校验） | ✅ 已交付（42fbd306）：lsp.diagnostics 与 ks_check 完全对齐（表达式编译/尾部截断/未知命令/契约）+ 宏感知修复 |

### Battle 3 — 确定性工程化代差
**目标**：任何游戏可自动化验证（市面引擎无此概念）。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 3a | **确定性回放框架**：replay.lua 扩展为"场景级快照测试"——录制 → 断言状态（f/sf/tf/backlog）→ 比对 | ✅ 已交付（f1f977ac）：kag/determinism.lua（无 GPU 执行 + 状态快照/断言 + kag_override + 超时保护）15 断言 |
| 3b | **模糊/属性测试**：随机 .ks 生成器（合法/畸形）驱动 tokenizer+scheduler，断言不崩溃不挂死 | ✅ 已交付（f1f977ac）：xorshift 种子随机 200 场景零崩溃/零挂起 + 畸形输入 + 5000 token 大场景 |
| 3c | **CI 确定性门禁**：回放回归纳入 ctest（确定性导出对比） | ✅ 已交付：determinism+fuzz 注册进 Lua 套件（sandbox 前），随 CI 运行 |

### Battle 4 — 创作闭环代差（IDE + 可视化 + AI）
**目标**：对标 WebGAL_Terre 图形化 + 超越（原生 IDE 体验 + AI 辅助）。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 4a 🔄 | **Electron 桌面 IDE**：自动拉起引擎 + Monaco + 调试面板 + 可视化预览 dock（主进程已写） | ⚠️ 主进程完成 + CJS 修复（09d8cb71）；运行时验证待网络（Electron 二进制下载） |
| 4b | **可视化场景编辑**：拖拽角色/背景到画布 → 生成 `[ch]`/`[bg]` 标签；场景树 ↔ 编辑器双向同步 | ✅ 已交付（252e6b1a）：Explorer 拖拽 → VisualView drop 生成标签；SceneTree 解析 .ks 点击跳转 Monaco |
| 4c | **AI 创作辅助**：基于已交付 [ai_dialog] 的本地 LLM 接口，IDE 内"AI 生成对话/续写场景" | ✅ 已交付（f7482b2c）：kag/aiwriter.lua（生成/续写/sanitize/降级）+ IDE AiPanel（✨ 活动栏）+ 16 断言 |
| 4d | **E-mote 替代（P2-9）**：骨骼/网格动画系统（自研）或 Live2D 扩展 | ✅ 设计定稿 `docs/design/skeletal-mesh-animation.md` + **S1 接口已实现（1e6ce1a5）**：IMeshRenderer + Null 后端 + BackendRegistry（C++ 609/609）；S2-S5 待后续 |

### Battle 5 — 生态与平台代差
**目标**：迁移入口 + 发布闭环（硬件约束项排后）。

| 阶段 | 内容 | 验收 |
|---|---|---|
| 5a | **P0-3 移动管线**：Android 构建脚本（NDK 交叉编译 .ks→字节码 预烘焙）+ IME 文档 | ✅ 已交付：`scripts/android_build.sh` + `docs/guides/mobile-pipeline.md`（799fa03b）+ **字节码预烘焙 `ks_bake.lua`（be763458，737ms→25ms 29× 首载加速）**；真机验证标注待设备 |
| 5b | **P2-8 教程体系**：getting-started 扩充 + 5 个示例场景（flow/rollback/debugger/live2d/minigame） | ✅ 已交付（d8b59ba4）：`kag-language-tour.md` 13 章 + demo_tutorial.ks + 附带修复 2 个真实解析 bug |
| 5c | **P1-6 硬件验证**：Live2D GL（Linux CI）、Steam 实机——需硬件，排期靠后 | CI 三平台绿 |
| 5d | **CARC 归档代差**：导入器支持 CARC 内 .ks 直接转换（归档内场景迁移） | ✅ 已交付（c9c4af0d）：carc_pack list/extract 子命令 + kag3_import --carc 模式（归档内 .ks 直接转换） |

## 4. 执行顺序建议

```
第 1 轮（已执行）：1b/1c/1d 全部交付（字节码持久化 + 表达式 AOT + 宏编译期内联）→ 4a 联调（主进程交付，运行时待网络）
第 2 轮（已执行）：Battle 2（LSP）→ Battle 3（确定性）
第 3 轮（已执行）：Battle 4b/4c（可视化 + AI 创作）
第 4 轮（已执行）：Battle 5（移动/教程/CARC 导入）+ 4d S1 + 收尾交接
每轮：全量构建 + Lua ≥112 + C++ 609 + ctest ≥10/10 + 耦合 PASS + benchmark 对比
```

## 5. 门禁（每轮强制）

1. `git diff --check` 干净
2. 全量重建零错误（rm -rf build → cmake -B build → --parallel）
3. CaesuraTests ≥609/609、Lua ≥112/112、ctest ≥10/10（新增套件/测试只增不减）
4. `python scripts/count_coupling.py --ci` PASS
5. benchmark 无退化（tokenizer ≤52ms/1000tok、scheduler ≥308k tok/s、表达式 ≤165ms/400-if；较 08-04 基线 135ms/1000tok/308k tok/s 为收紧/对齐值，本文档门禁取代旧基线）

## 6. 风险

| 风险 | 对策 |
|---|---|
| 字节码持久化跨 Lua 版本/平台兼容 | string.dump 仅作会话内缓存；跨会话用 JSON 序列化 _compiled 表 |
| LSP 与运行时契约漂移 | schema.dumpContracts 为单一事实源（自动生成） |
| 可视化编辑生成脚本与手写不一致 | 生成器输出过 ks_check 门禁 |
| 硬件约束项（GL/Metal/Steam）阻塞 | 标注"待硬件"，不阻塞本机可闭环项 |
