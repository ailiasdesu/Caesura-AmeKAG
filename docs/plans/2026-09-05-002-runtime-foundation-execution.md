# 运行时底层计划执行记录 — 2026-09-05

当前计划：docs/plans/2026-09-05-001-refactor-runtime-foundation-plan.md。

目标仍为完成 U1–U29，底层优先，Studio 暂停。本记录只登记实际工作，不缩小目标，也不把排期当成完成证据。

## 工作区

- 分支：codex/runtime-foundation。
- 开始时 HEAD：87a96f3d846caf5ad2aac6e3b8585133e7a5aed5。
- 开始时已有改动：AGENTS.md、CLAUDE.md、新计划及计划入口；未跟踪的 %SystemDrive%/ 保留。
- 执行过程使用独立日志目录 artifacts/validation/bootstrap-20260905/。这些是证据采集器完成前的诊断原始日志，尚不是发布候选证据。

## 单元状态

| 单元 | 状态 | 现状与下一项证明 |
|---|---|---|
| U1 | 本机适用验收通过 | 53项证据测试、57项隔离mutation及真实run→collect→verify通过；无自动RC-GO；CI发布身份继续由U23承担 |
| U2 | 进行中 | Windows Debug完整profile通过；执行器15项、进程管理Windows7/Linux6项通过；Release、完整跨平台/sanitizer与性能基线仍待完成 |
| U3 | 本机回归通过 | 5项真实SDL回归由4红变全绿，最终完整C++/CTest通过；跨平台释放观测及Engine暂停消费路径不据此宣称已验证 |
| U4 | 本机适用验收通过 | 默认provider、两种策略、legacy导入、HTTP/Steam替身staging及metadata回归通过；云校验mutation两例必红且原源码摘要已恢复 |
| U5 | 回归准备完成 | 5项确定性取消回归位于artifact补丁，尚未应用；需接通Engine最终代次检查与热重载/后端取消路径 |
| U7 | 回归准备中 | 原子写盘调查与artifact补丁准备，尚未应用 |
| U6、U8–U29 | 待执行 | 仍属于完整目标；按计划依赖推进 |

## 本轮实际执行

| 执行 | 结果 | 原始记录 |
|---|---|---|
| Debug全量构建，已有生产源码 | 退出0 | artifacts/validation/bootstrap-20260905/build-debug.log |
| C++完整套件（build/tests/Debug CWD） | 1137 passed，0 failed，0 skipped；386033 assertions | artifacts/validation/bootstrap-20260905/cpp-baseline.log |
| Lua主套件 | 145 passed，0 failed | artifacts/validation/bootstrap-20260905/lua-main-baseline.log |
| Lua孤儿套件 | 30 passed，0 failed | artifacts/validation/bootstrap-20260905/lua-orphan-baseline.log |
| 新验证执行器初始红灯 | 缺少实现模块，退出1 | artifacts/validation/bootstrap-20260905/validation-runner-red.log |
| 新验证执行器实现后的隔离测试 | 9 tests，OK | artifacts/validation/bootstrap-20260905/validation-runner-test.log |

## 首批集成验证（2026-09-05）

- 恢复受控mutation后的完整C++：1158 passed、0 failed、0 skipped；386514 assertions，记录 artifacts/validation/bootstrap-20260905/cpp-restored-full.log。
- 云同步变异：临时绕过两处staging校验后，HTTP/Steam两例全部失败；源文件恢复前后SHA256均为5619d9b70cd869c008c84360e0497c61303fade2e50651732aed475886a5fd3e。结果记录 u4-sync-mutation-result.json，标记test-fixture，不能作为发布通过证据。
- 证据工具独立审查完成；110项测试通过。coverage.py报告collector96%、verifier93%、合计95%，原始报告位于 artifacts/validation/bootstrap-20260905/evidence-finish/。
- 真实Windows Debug profile：run_id=9839a6fd-0a91-4af9-bd65-e924006a3556，11个check实际退出0；源码与夹具执行前后指纹一致。Cpp1158/1158、Lua145/145与30/30、Python15/7/53/57，CTest19 passed+1允许的真实AI服务skip。
- 该run原始receipt：artifacts/validation/raw/windows-debug-detached-01/run.json。采集及diagnostic复核均通过，归一化记录：artifacts/validation/87a96f3d846caf5ad2aac6e3b8585133e7a5aed5/9839a6fd-0a91-4af9-bd65-e924006a3556/windows-debug/manifest.json。它记录dirty工作树，不是对某个新commit的发布授权。
- Windows新preset干净configure在显式指定已核实VS实例后成功；禁用Python时完整验证configure按预期失败。WSL中的隔离CMake sanitizer探针正常输入退出0、整数溢出退出1并有UBSan诊断；这不是完整引擎sanitizer通过。
- 最初的windows-debug-initial运行在上下文切换后进程消失，只有部分原始日志，没有run.json；已保留为未完成运行，没有继续冒充通过。后续使用有PID/创建时间记录的隐藏后台执行器，复核进程终态及完整receipt。

执行器覆盖真实退出码、预/后置binary与fixture指纹、配置/源码目录身份、进程树清理、唯一目录和参数不经shell解释。U2的完整跨平台与性能要求仍未完成，计划整体保持active。

## 并行所有权

- resource agent：U3首批已冻结；U5只准备artifact补丁，收到主代理信号后才应用。
- storage agent：U4实现、回归与文档已冻结；U7暂只准备artifact补丁。
- evidence agent：U1实现和独立审查已完成。
- 主代理：构建/测试统一执行、U2执行器与profiles/CMake/CI、组合根接线、集成审查与本记录。

共享构建目录只由主代理使用。任何单元只有全部适用验收有实际证据后才能改为完成。
