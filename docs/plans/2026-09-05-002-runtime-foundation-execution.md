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
| U3 | 本机回归通过 | 真实SDL所有权、过滤器重入与取消释放计数通过；Engine统一暂停/恢复消费路径有真实Lua回归；跨平台整机释放观测仍依U2/U16 |
| U4 | 本机适用验收通过 | 默认provider、两种策略、legacy导入、HTTP/Steam替身staging及metadata回归通过；云校验mutation两例必红且原源码摘要已恢复 |
| U5 | 本机适用验收通过 | 请求epoch、旧worker/cache隔离、SDL重入取消、Engine暂停/回调重入、完整与局部脚本重载均已有运行回归；平台扩展仍依U2 |
| U7 | 本机及POSIX定向验收通过 | 两条存档写入路径共用独占临时文件与原生替换；Windows13项存档/云回归及POSIX18场景ASan/UBSan通过；不等同断电保证 |
| U6 | 本机适用验收通过 | worker-only等待、有限回调快照、重入关停/重启与Engine销毁屏障有回归；JobSystem的POSIX sanitizer及两种Job实现的行覆盖率已测量 |
| U8–U29 | 待执行 | 仍属于完整目标；按计划依赖推进；U8已有源码调查，未据准备文档宣称实现 |

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

## 第二批运行时与存档修复（2026-09-05）

- U5 先复现取消后旧 waiter 回流、pending 不归零；修复后5项确定性核心测试通过。补充两项真实SDL取消/过滤器重入精确释放检查，包含既有回归的42项 AsyncLoader 测试全通过。
- Engine 的真实Lua测试复现无窗口模式忽略调试暂停；统一消费函数现在逐项检查代次，并以unique_ptr持有延迟结果。另以“旧回调取消后连续建立两个新回调”复现 registry 引用复用后误释放，改为执行闭包前移除并释放旧引用。
- 完整脚本重载在旧操作回调和coroutine.close清理之后取消异步任务，再重置上下文。局部场景重载先成功解析、关闭旧协程、取消旧请求，再切换场景；解析失败与非当前场景刷新保留旧任务；暂停或协程正在执行时拒绝切换。修复dot-call、混合Lua/KS变更检测及Windows路径分隔符；局部重载26项Lua检查通过。
- U7 初始6项回归3红，包含真实Windows文件占用导致旧档被删除。新写盘函数通过CREATE_NEW/O_EXCL独占创建同目录临时文件，检查写入/flush/close，单次替换发布且没有remove旧档的回退。Windows下12项原子存档用例及1项HTTP密文拉取故障用例通过，覆盖并发、临时文件撞名、六类受控失败和进程中断。
- POSIX独立验证直接编译当前ISaveProvider.cpp，GCC15.2+ASan/UBSan，18场景、128断言、0失败，无sanitizer诊断；源码前后摘要一致。环境为WSL2、v9fs，不代表完整Linux引擎、原生ext4、所有文件系统或断电持久性。证据：artifacts/validation/u5-u7-posix/result.json。

### 验证记录与范围

1. 完整windows-debug执行：run_id=5ec2d07c-6eb5-4031-b8ff-8842bebc42b9；全量Debug构建、Lua145/30、验证器15/7/53/57、耦合与注册检查通过。Cpp1182中1项旧静态检查失败，CTest同时暴露平台矩阵测试依赖当前Git HEAD的问题。原始receipt为artifacts/validation/raw/windows-debug-u5-u7-01/run.json；已如实采集为FAIL。
2. 移除“未使用RenderBinding.h”这一已经失效的静态断言，保留该测试及其他具体后端禁用断言，未减少测试用例。定向profile再次全量构建与完整C++：1182 passed、0 failed、0 skipped，386930 assertions全部通过。该次CTest为18 passed、1 failed、1允许的AI smoke skip；唯一失败是平台矩阵。receipt为artifacts/validation/raw/windows-debug-u5-u7-02/run.json，run_id=0f80580d-4281-4d46-a24f-f217ba020227，也如实采集为FAIL。
3. 平台矩阵测试改为按历史证据自身的HEAD检查内容一致性，并新增不同HEAD必须拒绝的用例，32项通过。未修改生产生成器、历史矩阵及其证据标签；默认当前HEAD过期检查仍然有效。只复跑失败的CaesuraPlatformMatrixAdversarial，CTest通过，collect与diagnostic verify通过。receipt为artifacts/validation/raw/windows-debug-u5-u7-03/run.json，run_id=c4a751a8-d57b-4ab2-878e-2cc99e8d288e。

三次执行均记录源码与夹具前后摘要一致；最后一次是单项profile，不能冒充一次完整windows-debug全PASS记录。最终采用完整C++通过结果，以及CTest已通过项加修复后的单项复跑；后续Python测试修复未改变生产源码与C++二进制。以上均为dirty工作树诊断证据，未授予发布资格。

### 本次发现的后续验证边界

- 完整C++日志中，既有Ollama与demo_story缺失分支仍以MESSAGE后return显示在“passed”中；统计上的0 skipped不能证明这些分支实际执行。外部AI与Demo资源路径的显式适用性和缺失处理继续归U2/U21，不能据总数宣称已验证。
- 取消失败夹具最初使用短非法图片，触发独立的Debug SEH：ImageDecoder调用bimg::imageParse未传显式bx::Error，错误作用域可能断言。取消测试改用确定性的空资源读取失败，保留晚到失败覆盖；解码器问题尚未修改，不将它归因于取消代次。
- U6源码调查已记录于artifacts/validation/u6-preparation.md：waitIdle按时序决定是否执行callback、shutdown清空剩余callback、回调重入shutdown与Engine先销毁部分后端的问题。尚未应用U6代码或测试。

## 第三批任务系统与关停屏障（2026-09-05）

- 先取得6项Job与2项Engine确定性RED；问题包括worker已完成时shutdown丢弃回调、递归poll提前执行新批次、关停/重启后旧批次继续执行、Null空任务行为不一致、后端早于最终回调销毁，以及owner pump关停后继续更新音频。
- `waitIdle`现在只等worker和回调发布。poll每次执行一个快照并拒绝嵌套poll；关停关闭入口、join全部线程再执行一次最终快照。回调重入shutdown会改变epoch并取消剩余批次；普通poll中的重启保留原poll护栏，旧回调不能进入新生命周期。Null保留inline特性，但空work拒绝和旧生命周期回调抑制与真实实现一致。
- Engine在销毁VFX/渲染/图层/小游戏之前取消脚本异步请求并关闭JobSystem；关停设置运行标志为false。owner pump、job completion以及已识别事件消费出口在关停后停止当前帧。新增活动worker等待入口关闭及pending Lua载入关停回归，确认先关闭入口、线程退出、旧Lua闭包不执行。
- 测试拆分为test_job_shutdown.cpp和test_entry_shutdown.cpp；Job测试移除未使用的Engine头，便于直接编译相同测试到POSIX目标。定向52项Job及47项Engine/Async测试通过；后续完整套件包含新增活动worker案例，共1192项。
- 首次完整windows-debug运行（run_id=f77bc05e-c689-4c39-a8b8-6d4d19a1dcf1）除旧“asset先于job关停”的源码顺序断言外，各检查通过。按新的安全顺序将其更新为job先于async、async先于asset，保留用例及顺序约束。该首轮证据如实采集为FAIL。
- 仅复跑受影响的build/cpp/ctest profile：全量Debug构建退出0，C++1192 passed、0 failed、0 skipped，387023 assertions通过；CTest19 passed、0 failed、1允许的真实AI服务skip（140.97s）。源码和夹具前后摘要均一致。原始receipt：artifacts/validation/raw/windows-debug-u6-02/run.json，run_id=405a6049-f582-4c59-810d-5d85867d61c7；collect及diagnostic verify为PASS，仅对应windows-debug-u6-recheck范围，不是单次11项完整profile或发布批准。未受修改影响的Lua145/30、验证器15/7/53/57、耦合与注册检查沿用首轮结果。
- POSIX直接运行相同仓库中的47项Job/Null测试，174断言通过。标准gcov：JobSystem.cpp为156/159（98.11%），NullJobSystem.h为36/39（92.31%）。JobSystem.cpp单独启用ASan/UBSan，无诊断；不声称Null、Engine或完整Linux引擎被sanitizer覆盖。前后源码摘要一致，并已与当前文件复核。证据：artifacts/validation/u6-posix-02/result.json、evidence-manifest.json。
- 先前对全部测试翻译单元同时插桩的POSIX编译在240s超时，没有测试结果；确认WSL编译进程已终止后，改用独立目标文件及限定插桩范围，一次新构建约13s完成。失败产物保留于u6-posix，未作为通过证据复用。
- U8源码地图与约束见artifacts/validation/u8-preparation.md。U2中MESSAGE后return造成的未执行分支、其他平台/Release验证及后续U8–U29仍未完成。

## 并行所有权

- resource agent：U3/U5代码与SDL回归已交付，独立只读审查完成。
- storage agent：U4/U7代码及存档回归已交付，POSIX分支另有独立运行证据。
- reload agent：重载边界、Lua回归与平台矩阵测试隔离已交付；Engine接线完成只读审查。
- U6主代理：任务系统、Null模拟、Engine屏障与回归；契约/最终代码只读审查通过；POSIX agent仅负责独立编译与证据工件。
- evidence agent：U1实现和独立审查已完成。
- 主代理：构建/测试统一执行、U2执行器与profiles/CMake/CI、组合根接线、集成审查与本记录。

共享构建目录只由主代理使用。任何单元只有全部适用验收有实际证据后才能改为完成。
