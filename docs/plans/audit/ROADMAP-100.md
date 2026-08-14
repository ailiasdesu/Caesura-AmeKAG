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
