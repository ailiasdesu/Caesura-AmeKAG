# 100 轮自主迭代路线图（campaign 2026-08-14）

> 目标：分析现有情况，自主完成后续的迭代，要求迭代 100 轮，每轮任务不得过于简短。
> 门禁纪律（每轮必跑，与 AGENTS.md §5 一致）：全量构建零错误 → C++ 全绿 → Lua 全绿 → ctest 全绿 → 耦合 PASS → 语义提交。
> 推送策略（用户约定）：多轮本地累积语义提交，到目标节点统一 push + 一次三平台 CI。

## 现状基线（round 1 起）

- 40 轮审计已完成（docs/plans/audit/final-report-40.md）：P0×2/P1×8/P2×23 全部关闭或记录
- 测试基线：C++ 659 例 / Lua 120 例 / ctest 11 项（AI smoke 跳过）/ 耦合 PASS / 三平台 CI 全绿
- 上一轮已关闭：UnifiedBinding 迁移、RTTManager 双池修复、音频 CI 守卫、RTT 池 + 文本缓存测试
- 本轮（round 1）已关闭：TextRenderer 纯布局提取（P2-10 剩余缺口之一）+ 11 例无 GPU 测试

## 剩余已知缺口（后续轮次的素材池）

| 编号 | 内容 | 类型 | 预估轮次 |
|---|---|---|---|
| G1 | BgfxQuadBatch 合并组注入重构 + 测试 | P2-10 剩余 | 2-3 |
| G2 | TextRenderer rebuildCache 脏区间/缓存命中路径测试（updateDirtyRange 纯函数化） | P2-10 剩余 | 2-3 |
| G3 | Live2D/Steam SDK 路径验证文档 + 条件编译测试桩 | P2-6/P2-5 | 3-5 |
| G4 | 编辑器增强：可视化场景树/时间线（在 /api/pick+stats+SMA 基础上） | 追赶路径 | 10-15 |
| G5 | Web 导出可行性调研（emscripten）或移动运行时方案 | 追赶路径 | 5-10 |
| G6 | 示例作品库 + 教程（对标 Ren'Py 教程体系） | 追赶路径 | 5-8 |
| G7 | 性能基线更新（round-25 之后的回归对比） | 维护 | 2-3 |
| G8 | 渲染管线深层优化（quad 合并、脏矩形、atlas 打包） | 优化 | 8-12 |
| G9 | 脚本层：KAG 命令契约文档同步 + 新命令实现 | 功能 | 10-15 |
| G10 | 存档/资源/音频模块边界重构与测试补强 | 硬化 | 5-8 |
| G11 | 平台层：输入映射、窗口管理、事件优先级测试 | 硬化 | 3-5 |
| G12 | 代码质量：clang-format 全量、死代码清理、耦合再收敛 | 维护 | 3-5 |
| G13 | 文档同步：api/cpp-interfaces、lua-modules、能力矩阵 | 维护 | 3-5 |
| G14 | 发布流程：CPack 打包、changelog、release notes 自动化 | 维护 | 3-5 |

## 阶段规划（100 轮）

- **阶段 A（round 1-20）渲染可测试性收官**：G1/G2/G8 前半 + TextRenderer 其余纯函数化
- **阶段 B（round 21-40）编辑器增强**：G4（场景树/时间线/检查器增强）
- **阶段 C（round 41-55）分发与生态**：G5/G6（Web 导出调研、示例库教程）
- **阶段 D（round 56-75）脚本与玩法深度**：G9（KAG 命令/契约/表达力）
- **阶段 E（round 76-90）硬化与性能**：G3/G7/G10/G11
- **阶段 F（round 91-100）收尾**：G12/G13/G14 + 100 轮总结报告

> 阶段划分是指导性的：每轮先看"素材池"，选杠杆最高的任务；若某任务被证明不可行
> （如 Web 导出调研发现需外部 SDK），如实记录并替换为等量价值任务，不硬凑。

## 轮次记录

| 轮次 | 任务 | 结果 | 提交 |
|---|---|---|---|
| 1 | TextRenderer 纯布局提取 + 11 例无 GPU 测试（G2 前半） | C++ 659/659, Lua 120/120, ctest 绿, 耦合 PASS | 4c12c9c5, 6c30d545 |
| 2 | TextRenderer 脏区间纯函数化（countUtf8Glyphs/computeDirtyRange）+ 10 例无 GPU 测试（G2 完成） | C++ 669/669, Lua 120/120, ctest 绿, 耦合 PASS | b36d82c8 |
| 3 | BgfxQuadBatch 批次数学纯函数化（quadToNdc/computeMergeGroups/buildGroupIndices）+ 11 例无 GPU 测试（G1 完成） | C++ 680/680, Lua 120/120, ctest 绿, 耦合 PASS | 96a2cd6c |
| 4 | 共享像素→NDC 纯数学 NdcMath.h，blitTexture/stretchBlt/affineBlt/quadToNdc 统一委托 + 7 例测试（G8 前半） | C++ 687/687, Lua 120/120, ctest 绿, 耦合 PASS | 7c8b2849 |
| 5 | 无障碍颜色滤镜数学纯化（Machado-2009 预设表 + effect-4 VFX 打包）+ 7 例测试（G8 中段） | C++ 694/694, Lua 120/120, ctest 绿, 耦合 PASS | 89aaf4f1 |
| 6 | DirtyRect merge uint16 回绕修复（32 位计算+钳制）+ shouldUseScissorFor 纯决策 + 11 例测试（G8 中后段） | C++ 705/705, Lua 120/120, ctest 绿, 耦合 PASS | 853e0e31 |
| 7 | 粒子生命周期衰减纯化（lifeFade + buildParticleVisual + maxLife=0 NaN 守卫）+ 6 例测试（G8 收尾） | C++ 711/711, Lua 120/120, ctest 绿, 耦合 PASS | 22203ce8 |
| 8 | 确定性测试套件扩展：嵌套 elseif、jump/call 流程、表达式边界（除/模/串/链）、深快照、状态隔离（G9 前哨） | Lua 120/120（+28 determinism 断言）, C++ 711/711, 耦合 PASS | be61f8be |
| 9 | 表达式语言边界测试：括号作用域、表键引号、短路防 nil 解引用、严格类型、数字字面量/双重否定/链式比较（G9 中段） | Lua 120/120（+17 expr 断言）, C++ 711/711, ctest 绿, 耦合 PASS | 81872bae |
| 10 | tokenizer 边界：blocktext 三引号、注释、iscript 原始体、标签、BOM/CRLF/unicode 鲁棒性、未闭合降级（G9 中段） | Lua 120/120（+22 tokenizer 断言）, C++ 711/711, ctest 绿, 耦合 PASS | 754676d6 |
| 11 | 替换 4 个假通过 switch 测试为 7 项真实断言：case 精确路由/无回落、default 回退、缺变量回退、无匹配跳过、嵌套 switch 隔离（G9 中段） | Lua 120/120（scheduler 25/25）, C++ 711/711, ctest 绿, 耦合 PASS | 79514217 |
| 12 | 孤儿测试审计：发现 8 个 test_*.lua 从未进 runner（假绿）；确认全部 PASS 后建独立 run_orphan_tests.lua（与沙箱套件顺序互斥不可合并）+ 修复 os.exit/BOM/label 计数（G9 中段） | Lua 120/120 + 孤儿 8/8, C++ 711/711, ctest 绿, 耦合 PASS | 4d5bff82 |
| 13 | 孤儿测试接入收尾：精确诊断重排失败根因（全局 mock 与沙箱顺序冲突），改用隔离运行器方案；更新 CLAUDE.md 门禁说明（G9 中段） | 同 round 12 门禁 | 4d5bff82 |
| 14 | 防回归机制：check_test_coverage.py（Lua 128+C++ 65 全注册校验）+ CI Test coverage check 步骤——未注册测试=失败（G9 收尾/质量基建） | Lua 120/120+孤儿 8/8, C++ 711/711, ctest 绿, 耦合 PASS | f67399a6 |
| 15 | 文档同步审计：KAG 契约命令数全仓漂移（72/78/81/84 并存），核实权威值 84（schema_doc 自动生成）并同步 README/AGENTS/CLAUDE/kag-neo-genesis-language；规避 PowerShell UTF-16 重定向坑（G9 收尾） | 文档一致性 | f5caf28d |
| 16 | 编辑器测试基建（G4 起航）：vitest + SceneTree parseSceneElements 7 例（标签/类型分类/注释跳过/截断/行号/空源）+ CI npm ci 步骤 | vitest 7/7, tsc 0, ctest 绿, 耦合 PASS | cfd6de2f |
| 17 | 编辑器安全纯函数测试：luaString 长括号转义（注入守卫：终止符形状内容/等号升级/10 连等） + luaValue JSON→Lua（标量/nil/转义/嵌套表）（G4 中段） | vitest 20/20, tsc 0, ctest 绿, 耦合 PASS | 339e2cf2 |
| 18 | EngineClient RPC 客户端测试（可注入 fetch mock）：URL 拼接/token 头/Content-Type/查询参数/JSON 解析/RpcError/404 详情/setBase（G4 中段） | vitest 28/28, tsc 0, ctest 绿, 耦合 PASS | a44b530f |
| 19 | 编辑器命令高亮漂移审计：发现 sma_anim/sma_ik/sma_variant 缺失（tag.invalid）；补全 3 命令 + check_test_coverage.py 增编辑器命令漂移守卫（负向验证通过）（G4 中段） | vitest 28/28, tsc 0, 覆盖检查 PASS(负向 FAIL 验证), ctest 绿, 耦合 PASS | a8db10c4 |
| 20 | lspCall Lua 桥代码生成测试（导出函数 + mock evalRaw）：方法调用/luaString 转义/终止符升级/多参 join/无参（G4 中段，20% 里程碑） | vitest 34/34, tsc 0, ctest 绿, 耦合 PASS | f36b5415 |
| 21 | jsdom 组件测试基建（G4 中段）：安装 jsdom/@testing-library + SceneTree 组件测试（空态/元素渲染/计数标题/点击跳转 reveal/挂载聚焦/无元素提示）——vi.mock 阻断 Monaco | vitest 40/40, tsc 0, ctest 绿, 耦合 PASS | 4176d232 |
| 22 | jsdom 组件测试推广（G4 中段）：StatusBar 7 例（offline/connected/scene/token/paused/品牌/组合态）+ ExplorerView 9 例（分组渲染/空态提示/大小写不敏感过滤/错误显示/脚本双击打开含 inspect 失败降级/图片占位打开/OPEN EDITORS 生命周期） | vitest 56/56, tsc 0, ctest 绿, 耦合 PASS | c06d8786 |
| 23 | jsdom 组件测试推广 II（G4 中段）：OutputPanel 4 例（空态/分级行/500 条上限/引擎不可达静默重试）+ DebugView 11 例（轮询镜像 store/暂停徽章/离线降级/Run 成功失败与禁用态/Stop/断点设置与非数字回退/清空/Continue） | vitest 71/71, tsc 0, ctest 绿, 耦合 PASS | af6a1995 |
| 24 | jsdom 组件测试推广 III（G4 中段）：ActivityBar 4 例（四视图按钮/图标/标题/active 类/点击切换）+ VisualView 12 例（帧渲染与错误/状态行/统计带单位与 tier/SMA 校验成功 meta+骨架树+动画/校验失败错误列表/evalRaw 加载与样本降级/本地 JSON 校验守卫/保存/Live2D 列表与加载/帧点击拾取）——SmaSkeletonCanvas vi.mock 桩 | vitest 87/87, tsc 0, ctest 绿, 耦合 PASS | 08e95f2b |
| 25 | jsdom 组件测试收官（G4 中段，25% 里程碑）：AiPanel 11 例（对话生成+aiwriter 桥/luaString 转义/AI 错误提示/eval 失败/无文档禁用守卫/尾部嵌入/场景规格守卫/生成插入/诊断行号解析/审查发现与通过/忙碌禁用）+ EditorArea 编辑器注册表 6 例（未注册 no-op/居中+定位+聚焦/行号钳制/多路径路由/注销/身份守卫）——monaco 全桩；vitest 破百 | vitest 104/104, tsc 0, ctest 绿, 耦合 PASS | 387c8349 |
| 26 | G4 功能增强：场景检查器（Inspector）——parseTagParams 全量参数表提取（引号/数字/裸旗标/=内值/未闭合容错）、SceneTree 点击联动 inspected 状态 + 高亮 + aria-pressed、新 InspectorView（类型/命令/全参数表/源行）、挂入 App explorer 侧栏 | vitest 117/117, tsc 0, vite build 0, ctest 绿, 耦合 PASS | 3a9feef8, ffc70167 |
| 27 | G4 功能增强 II：时间线视图——buildTimelineSections 纯函数按 *label 分段（序言/null 标签、空源、无标签脚本、空段）、类型过滤 chips（All/BG/FG/CH/Audio/Labels）、段/元素点击跳转+检查器联动、挂入 explorer 侧栏 | vitest 126/126, tsc 0, vite build 0, ctest 绿, 耦合 PASS | 0b6ae612 |
| 28 | G4 功能增强 III：引擎执行位置联动——RpcStateResult 新增 currentCmd（当前执行元素 [ch]/[bg]/text），main.cpp Lua 快照从 ctx.tokens[token_index] 提取，HTTP+stdio 三端点输出 current_cmd，编辑器 store engineCmd 镜像 + StatusBar cmd 行 + VisualView exec 行；headless RPC 冒烟全过 | vitest 128/128, tsc 0, C++ 711/711（+1 断言）, Lua 120+29, ctest 11/11, 耦合 PASS, headless 冒烟 PASS | 06846d67, a4f93628 |
| 29 | G4 收尾：时间线引擎执行状态条——engineConnected+engineCmd 时显示 ▶ 脉冲 + 当前命令 + token 位置，离线/空闲隐藏（复用 round 28 current_cmd 镜像） | vitest 131/131, tsc 0, vite build 0, ctest 绿, 耦合 PASS | 6c1be1a3 |
| 30 | G5 Web 导出三路径并行调研（30% 里程碑）：A emscripten 原生编译（条件性，6-10 周 Demo，渲染线程/COOP-COEP/Live2D 风险）、B 轻量 Web 播放器（wasmoon Lua5.4 + 90% 纯 Lua KAG 栈复用，1.5-2.5 人周，推荐首选）、C 移动/生态（SDL3 Android 模板就绪）；收敛决策 B 近期实施 + A 中期 PoC | 调研 3 报告 + 决策文档（725 行），门禁无代码变更 | 0578d6f5 |
| 31 | G5 实施第一步：wasmoon 验证 spike（3 连 PASS）——spike1 tokenizer+lpeg 原样运行、spike2 编译前端（schema/expr/compiler）55 tokens/3 labels、spike3 完整调度器执行 demo（55→53 token 推进）；**重大发现：Lua 5.5 通用 for 循环变量 const 化**——4 处 5.4 合法/5.5 崩溃代码修复（compiler/schema/ks_bake/scheduler）+ 4 例防回归测试（test_compiler 10a-10d，39/39） | vitest 131/131, Lua 120+29（+4 新断言）, C++ 711/711, ctest 11/11, 耦合 PASS | a14c2b44 |
| 32 | G5 MVP 核心原型：spike4 真实 kag 命令表 + JS 绑定适配层——9 命令模块加载、galgame_demo 55/55 tokens 零错误执行、good_end 结局解锁、20 次真实绑定调用（load_texture/audio_play/set_layer_image）；适配层契约实测（add_layer 返回节点表/Type 常量表/get 缺失返 nil） | spike 4/4 PASS，引擎门禁无变更 | 4f0fe235 |
| 33 | G5 Web 播放器 MVP（web/ 目录）：wasmoon bridge + AdapterCore 绑定状态机 + DOM 渲染器；帧驱动协程执行（[p] 点击暂停/autoClick/语音模拟）；galgame_demo 完整 DONE:53 + good_end 解锁 + 5 图层 + bgm/voice/se + 2049 绑定事件；12 vitest + Node 集成测试 | web 12/12, spike 4/4, 引擎门禁无变更 | 24324814 |
| 34 | G5 浏览器全流程验证：Layers.snapshot() 纯导出、bridge 收集 TextScene draws→core 覆盖层、render_text 参数契约修复、tag 前缀 img/div 分类、跨 run 纹理重映射；jsdom 集成测试（真实 wasmoon + DOM + galgame_demo：park→点击推进→DONE→hana bg+角色立绘渲染） | web vitest 13/13, Lua 120+29（+3 snapshot 断言）, 耦合 PASS | c57b8f15, c49e6742 |
| 35 | G5 导出链：ks_bake --web story bundle——bakeWeb（compiler.serialize 场景序列化 + storage/file 资产发现）、encode_lua_literal 导出、非 ASCII 路径 CWD 剥离、CLI --web 产出 cache/story/story.lua（4 场景 6 资产）；web 播放器零解析加载验证（.ksc + story bundle 均 load 直取） | web vitest 15/15, Lua 120+29+17, compiler 39, 耦合 PASS | e2764bd7, dc91cafa |
| 36 | G5 浏览器部署形态：vite 配置（publicDir 仓库根）静态服务 /scripts /demo /assets /cache/story 全部 200；main.mjs 优先 story bundle（runFromBundle 零解析）运行 + 点击推进；资源布局测试 3 例（web 18/18） | web vitest 18/18, vite 全资源 200, 引擎无变更 | b3500e69 |
| 37 | G5 音频真实化：AudioEngine（WebAudio 3 总线 bgm/se/voice、解码缓存、play/stop/isPlaying/音量），bridge 音频委托真实引擎 + core 状态降级；5 引擎测试（总线/生命周期/缓存/降级）；web 23/23 | web vitest 23/23, 引擎无变更 | 2cba6e3d |
| 38 | G5 文本排版对齐：bridge 收集 TextScene draws 为 Lua 表（x/y/rgb/scale/bold/italic）经 wasmoon 表桥接直达 JS，core.setDraws + DOM 渲染器逐条绝对定位 span；textBuffer 平面回退保留；flow 断言 span left/top/color/fontSize | web vitest 23/23, 引擎无变更 | 42628adb |
| 39 | G5 图层动画：CSS transition 补间（left/top/opacity 300ms）插值引擎驱动的 sprite_move/sprite_fade、opacity 0..255 归一化 0..1、canonical 节点 mutator（Lua proxy 引用解析）、main.mjs rAF 渲染循环；flow 断言 sprite_move 20+ 帧至 x=120 + transition 就位 | web vitest 23/23, 引擎无变更 | 0392b85f |
| 40 | G5 播放器 UI 收官（40% 里程碑）：backlog 历史（core 快照提交不同文本页 + 面板滚动回看）、自动推进开关（1.2s 定时点击）；3 backlog 测试；web 26/26 | web vitest 26/26, 引擎无变更 | 65c290d1 |
| 41 | G6 示例库起点：showcase.ks（25 命令全覆盖：文本/图层/音频/特效/流程 + random/if/jump 分支 + ending 解锁；56 tokens/6 labels）；backlog 历史机制设计（[p] 页快照） | 引擎 compile 通过, web showcase 全流程 PASS | c8ede0d6, 628d7b64 |
| 42 | G6 示例库收尾：backlog 完整实现（Lua 累积 __SCENE_BACKLOG + core.pushBacklog 快照去重不影响视图）+ showcase flow 测试（DONE + 9 页历史 + ending）+ 示例库文档 docs/guides/sample-library.md | web vitest 27/27, bundle 5 场景 | 628d7b64, c8ede0d6 |
| 43 | G6 教学示例库：demo/tutorial/ 6 个递进式教程（01 hello → 02 文本 → 03 图层 → 04 音频 → 05 分支 → 06 特效），全部过 ks_check 契约零警告 + ks_bake 编译 + Web 播放器 DONE 运行验证；Web 播放器新增 ending 解锁记录（bridge 导出 ctx.seen_endings → core.recordEndings 去重事件）；flow 测试固化教程全路径 + adapter 测试 2 例；sample-library.md 教程路径表；story bundle 11 场景 | web vitest 30/30, Lua 120/120+29, C++ 711/711, ctest 10/11（AI smoke 跳过）, 耦合 PASS | (round 43 提交) |
