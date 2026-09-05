# 运行时底层计划执行记录 — 2026-09-05

当前计划：docs/plans/2026-09-05-001-refactor-runtime-foundation-plan.md。

路线图保留 U1–U29，底层优先，Studio 暂停。用户最新决定：今天的实现止于 U10，不开展 U11；完成提交、推送、合并 master，并修复 CI 至全绿。U11–U29保留为后续工作，本记录不把排期当成完成证据。

## 工作区

- 分支：codex/runtime-foundation。
- 开始时 HEAD：87a96f3d846caf5ad2aac6e3b8585133e7a5aed5。
- 开始时已有改动：AGENTS.md、CLAUDE.md、新计划及计划入口；未跟踪的 %SystemDrive%/ 保留。
- 执行过程使用独立日志目录 artifacts/validation/bootstrap-20260905/。这些是证据采集器完成前的诊断原始日志，尚不是发布候选证据。

## 单元状态

| 单元 | 状态 | 现状与下一项证明 |
|---|---|---|
| U1 | 本机适用验收通过 | 53项证据测试、57项隔离mutation及真实run→collect→verify通过；无自动RC-GO；CI发布身份继续由U23承担 |
| U2 | 进行中 | U10源码已通过Windows Debug与独立干净检出Release完整profile；完整跨平台/sanitizer、Web skipped分类与性能基线保留后续，今天只处理合并所需CI |
| U3 | 本机回归通过 | 真实SDL所有权、过滤器重入与取消释放计数通过；Engine统一暂停/恢复消费路径有真实Lua回归；跨平台整机释放观测仍依U2/U16 |
| U4 | 本机适用验收通过 | 默认provider、两种策略、legacy导入、HTTP/Steam替身staging及metadata回归通过；云校验mutation两例必红且原源码摘要已恢复 |
| U5 | 本机适用验收通过 | 请求epoch、旧worker/cache隔离、SDL重入取消、Engine暂停/回调重入、完整与局部脚本重载均已有运行回归；平台扩展仍依U2 |
| U7 | 本机及POSIX定向验收通过 | 两条存档写入路径共用独占临时文件与原生替换；Windows13项存档/云回归及POSIX18场景ASan/UBSan通过；不等同断电保证 |
| U6 | 本机适用验收通过 | worker-only等待、有限回调快照、重入关停/重启与Engine销毁屏障有回归；JobSystem的POSIX sanitizer及两种Job实现的行覆盖率已测量 |
| U8 | 本机适用验收通过 | 预启动宿主公钥、全部识别CARC层固定验证、失败回滚与CLI已接通；12项核心及39项CLI回归通过；不认证整个分发目录 |
| U9 | 本机及POSIX定向验收通过 | 合法签名边界、并发/短读、真实分配失败及恢复已测；Reader有界拒绝、OpenSSL上下文异常释放已修复；真实包内存/耗时留样，不宣称全平台或穷尽fuzz |
| U10 | 本机适用验收通过 | 唯一会话引用、start/stop/reload/EOF/失败终态、真实协程与输入清理、资源/AI回调失效及Engine关停顺序已验证；Cpp1229与Lua147/30全绿 |
| U11–U29 | 待执行 | 仍属于完整目标；U11恢复状态、提交前副作用和旧格式证据已完成只读地图，尚未实现或验收 |

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

## 第四批CARC发行者信任（2026-09-05）

- 新增`ArchiveTrustMode::{Compatible,PinnedPublisher}`与按值持有的32字节`archivePublisherKey`。默认兼容模式验证归档自签名；固定模式只使用宿主预先提供的字节，归档内嵌声明及旁置公钥文件不能替换它。保留原公钥文件路径API，新增公钥字节overload复用既有验签、索引解密及解析路径。
- 自动挂载按原名称/优先级/去重规则收集全部候选，固定模式在任何provider进入资源链前验证全部候选；错误模式、缺少宿主公钥、发现失败或任一包验证失败均使Engine初始化失败并走既有关停回滚，不使用部分已验证链。松散文件、入口Lua、manifest和宿主程序仍不在CARC认证范围内。
- `--carc-trust compatible|pinned`和`--carc-public-key PATH`在切换资源根之前解析和验证。固定模式要求显式公钥，文件恰好32字节且只读一次；坏值、长度、未知选项或冲突配置在创建Engine之前拒绝。删除Lua启动之后的重复嵌入公钥检查；旧`carc_verify_on_startup`值不再控制验签。
- 12项C++核心RED为4通过/8失败，覆盖攻击者包被误接受、候选链部分注册和Engine失败未回滚。CLI在旧二进制上9/34通过，明确证明指定pinned A后B包仍被接受；修复后核心及相关archive/entry/源码约束132项通过。
- CLI中发现一个实际Windows中文路径问题：窄argv不能直接当UTF-8解码。仅对公钥参数从原始宽命令行提取路径，检查参数数量和选项索引，RAII释放argv，保留原生路径用于打开与重复值比较；Windows目标链接系统shell32。另修正三处测试诊断字符串匹配，保留非零退出、未执行config、未响应ping的断言。最终39项CLI全部通过，包含中文、空格、冲突参数、A/B替换、错误长度和Lua true/false。
- `CaesuraArchiveTrustCli`已加入CTest，全部原生profiles的CTest发现数下限提高到21，Windows Debug的C++下限提高到1204。独立安全审查覆盖核心、CLI/晚期检查删除及最后宽路径增量，未发现阻断问题。
- 完整windows-debug运行：11个check实际退出0；全量Debug构建通过，C++1204 passed、0 failed、0 skipped、387374 assertions；Lua145/30、验证器15/7/53/57、耦合与注册检查通过；CTest20 passed、0 failed、1允许的真实AI服务skip（131.29s），包含39项CLI脚本。源码和夹具前后摘要一致。receipt：artifacts/validation/raw/windows-debug-u8-01/run.json；run_id=b329f72d-4049-4af4-af92-09d930e8d5cf；collect和diagnostic verify均PASS。归一化证据位于artifacts/validation/3f7e335b5f1ae8112357230df2a62f22d916adc7/b329f72d-4049-4af4-af92-09d930e8d5cf/windows-debug/，仍是dirty快照诊断，不是发布批准或未运行平台证明。
- 当前`carc_pack`每次打包生成新发行者，未新增同密钥签署多个不同包、轮换或多发行者授权。CARC索引密钥可由公钥派生，不能把它称为DRM或内容保密机制。运行时固定公钥验证通过不代表这些作者工具与分发能力完成。
- 后续准备：artifacts/validation/u9-prepared/u9-tests.apply_patch新增8个合法签名边界/共享reader并发用例，尚未应用或运行；artifacts/validation/u2-fixtures-prepared/u2-fixture-repair.patch修复Demo/AI假通过及仅Lua变化时夹具不刷新的问题，亦未应用或运行。二者涉及CMake时应按上下文合并，不能覆盖其他修改。更强的删除文件镜像一致性、实际分配失败、精确上限与真实包性能仍待验证。

## 第五批真实夹具与CARC异常边界（2026-09-05）

- U2保留Demo和AI各5个用例，去掉资源缺失或Ollama不可用时MESSAGE后return的假通过。Demo改为真实tokenizer/compiler/scheduler执行及内容/分支/EOF断言；AI普通回归使用真实loopback HTTP的模型发现与请求响应，真实Ollama仍由独立smoke检查，缺服务明确skip。LuaManager测试夹具显式shutdown，成功pcall不读取无效错误栈。
- 构建依赖和CTest setup共用`tests/SyncTestAssets.cmake`，每次准确镜像7个生成夹具目录，清除陈旧文件但保留二进制、存档等其他输出。删除前预检全部根、祖先与子项，拒绝source重叠、输出越界、符号链接/junction及分号引起的GLOB结果歧义。独立审查发现的方括号路径、POSIX冒号、分号条目问题均已修复；Windows24项、WSL30项通过，0 skipped。
- 实际无重链接检查中，修改复制后的tokenizer并放入陈旧文件，再次构建可恢复精确副本且删除陈旧项，C++二进制SHA与mtime均未变化。另对Demo缺失、full-pipeline缺失、对白篡改、tokenizer空参数、禁用[end]做5次输出夹具敏感性检查，全部按预期失败且逐项恢复SHA。证据：`artifacts/validation/u2-sync-proof/result.json`、`u2-sensitivity/result.json`；均非发布证据。
- U9新增8个合法签名语料用例，覆盖19个subcase：整数/区域边界、index/entry不一致、坏tag、解压尺寸、重复索引既有语义、close/reopen、真实截断后短读与恢复，以及4线程共享reader读取。首轮8/8、764断言通过，没有据此虚构解析器缺陷；相关archive/entry定向套件118/118通过。
- 独立Windows子进程在受限JobObject提交额度中实际复现两条bad_alloc：read抛异常，open还遗留部分打开状态。Reader现在对分配失败返回empty/false；open清理全部打开状态，read保留索引并允许再次读取。新增`CaesuraCarcMemory`持续执行两例，RED均失败、GREEN均通过；限制为基线加8MiB，外层128MiB并带超时。证据：`u9-measure/contract-red*`、`u2-u9-green/result.json`。
- POSIX进一步复现OpenSSL加/解密先创建EVP上下文、后续vector分配异常时漏释放，两条释放断言RED；改为unique_ptr原生deleter后10条检查全部通过。`tests/probes/crypto_allocation`仅在原生非Windows构建注册，standalone亦可运行。独立审查补正了主工程sanitizer继承；复用真实`CaesuraValidation.cmake`的GCC ASan/UBSan检查通过，CryptoEngine与probe编译、最终链接均确认插桩，启用泄漏检测。证据：`u9-crypto-allocation/{red,green,sanitized}`；这是定向POSIX证明，不是完整Linux引擎或Clang验证。
- 真实包使用139个文件、39,824,624字节，归档30,917,289字节；3个新进程417/417路径、长度与SHA精确匹配。Reader修复后的Windows暖缓存样本中位数：open353.8546ms、read调用合计424.7545ms、含摘要循环465.5651ms、峰值工作集50,556,928字节。原始包未重打，配置/构建/读取退出码及输入摘要留存于`u9-measure-after/summary.json`。此样本早于仅影响非Windows的EVP修复；共享主机、未定预算，不作性能PASS或升降判断，也不据此引入流式重构。

### 本批集成证据

1. 完整Windows Debug run `a16fcaf8-b94a-41b8-bf95-d4f1e20b9b0a`：构建、Cpp1212/1212、Lua孤儿30/30及CTest22 passed+1真实AI允许skip通过；Lua主套件2个既有耗时阈值失败，验证器15项中3项Git malloc失败。该完整receipt如实采集为FAIL，路径`artifacts/validation/raw/windows-debug-u2-u9-01/run.json`。
2. 在没有并发构建时仅复跑失败的Lua主套件与验证器，源码、二进制、阈值不变：145/145及15/15通过，collect/diagnostic verify PASS，run `aebc429d-c00f-459b-bbb6-f7c87c52f1f0`，原始目录`windows-debug-u2-u9-02`。结果支持瞬时资源/耗时波动，未定位到具体外部进程，不宣称已消除主机资源风险。
3. EVP修复及新probe注册后，重做完整Debug构建、完整C++、耦合/注册检查及完整CTest：1212 passed、0 failed、0 skipped，388153断言；CTest22 passed、0 failed、1允许的真实AI skip，151.30s。run `28768856-56db-4332-b9a9-ca765c4d4905`，原始目录`windows-debug-u2-u9-03`；collect/diagnostic verify PASS。Lua未受该非Windows修改影响，复用上一条通过证据。

三次receipt的源码与夹具前后摘要均一致。后两次是各自范围的定向profile，不能合称一次完整profile全PASS，更不能把dirty快照当成发布批准。最终独立审查已处理新增probe的sanitizer配置遗漏；其余本批边界测试、资源释放与夹具镜像审查无待处理阻断项。当前共有8个单元达到本机适用或定向验收，U2和U10–U29继续执行。

## 第六批唯一脚本会话与失败终态（2026-09-05）

- 初始8项真实runner回归全部RED：C++ registry与runner/global身份不同、失败start遗留状态、stop缺失、旧load_tokens写入后继会话，以及完整reload未关闭实际wait/tween协程。`GameState`改为只引用table/nil的绑定桥，验证普通栈索引与类型、保留栈和拒绝时旧引用；VM init不再创建影子表。桥的84条断言先通过，Lua runner统一发布到`Engine.bind_active_context`、自身和`_CAESURA_CTX`。
- start先准备场景，成功后才替换旧上下文；场景准备异常返回失败，已开始执行的候选失败则关闭。所有scheduler创建点集中捕获所属ctx并发布同一个co引用。stop/完整reload/自然结束/无效延迟跳转关闭旧协程与操作、停止旧tween、再撤销资源及AI回调；清理期间拒绝重入start/update等操作。自然EOF保留可查询的最终表，并继续返回既有`ended`状态；显式恢复请求可以创建新协程，不能复活旧协程。
- Engine在Job、async及render/layer等后端销毁之前停止runner；LuaManager关闭VM时也调用幂等停止入口。真实Engine回归覆盖A停止/B启动、成功/失败载入、worker与loader两个队列边界，A引用可GC且不进入纹理配额申请；B恰好一次申请并在headless guard处返回失败。另证明scope清理先于Job/render/layer关闭，清理中新排队的载入也不回调。不声称创建了真实GPU资源。
- AI六项原生回归全部先RED。修复数字键记录用字符串键删除导致的重复unref，取消按VM移除回调，移除跨VM全局cancel epoch；completion使用VM主Lua线程，拒收submit时释放引用并返回false。之后补上提交协程实际GC后的投递及真实NullJobSystem同步完成两个分支，均进入最终完整C++验证。
- 独立生命周期审查发现并修复两项P1：HistoryUI没有关闭作用域，reload后残留history输入焦点；选择/文本输入关闭协程后残留全局回调及原生输入模式。HistoryUI新增真实`<close>`，选择和输入新增`Operation <close>`、幂等清理、函数身份恢复及失活closure guard；普通完成、取消与部分初始化失败共享清理。历史18项、输入26项通过，RED分别为14/18及8/26通过。旧choice/i18n夹具改为实际协程挂起，保留并加强原断言。
- 后续独立审查补出缺失label/不安全pending jump绕过终止清理的问题；两种真实runner场景先RED，再加统一finish，GREEN通过。其他引用桥、关停、输入恢复及U2路径修复无新增阻断项；AI与原生回归另经独立只读审查。失败跳转、自然EOF、同场景reload的旧运动取消、失败替换保留pending reload都已进入持续C++测试。

### U2：独立Release检出与明确产物路径

- 在`D:/文件存放处/code/Caesura-foundation-validation`创建独立detached检出`1d9a71f15d91870b7b3d079d027e834b7dc8303d`，使用windows-foundation preset，FFmpeg/Steam/Live2D关闭。SDL3为本机已有SDK，96个文件摘要保存在`u2-fresh-preparation/sdl3-sdk.json`；这是新源码/构建目录验证，不是全新机器依赖安装证明。
- 新Release配置与全量构建退出0，Cpp1212/1212、388142断言，Lua145/30及验证器检查通过。原完整run `126615b1-46eb-49f1-87bd-533382480070`因BuildCli找不到preset目录的引擎而skip、Golden找不到Lua而FAIL，如实采集FAIL；该检出执行期间dirty=false且源码/夹具一致。
- CTest现在显式传入当前配置的`CaesuraAmeKAG`和`lua_cli`路径，经`CAESURA_ENGINE`/`CAESURA_LUA`传递到实际CLI、预编译和Golden。显式无效值（含空值和Lua目录）不能回退或skip；无显式配置的手动发现保持可用。Golden契约检查任何非零退出均FAIL，不能把42/127等失败当作通过。
- 9项路径检查通过，包括中文/空格、陈旧默认程序、缺失显式产物与真实CLI失败清理。测试哨兵使用`/bin/sh`，避免宿主PATH中的Windows WSL bash截获`env bash`而造成夹具错误；哨兵仅验证选择路径，始终故意返回失败，不冒充Golden通过。
- 只将U2路径修复应用到独立检出，不混入U10代码；原Release二进制不变，相关3项CTest全部通过，run `c9a0a863-934b-4126-aa4a-6a8096ab1f37`，目录`raw/windows-release-output-paths-02`，collect/diagnostic verify PASS。此补丁快照为dirty诊断，不能改写原完整FAIL为单次全绿；U10新代码的Release仍需复核。

### U10最终集成证据

1. 初次完整run `846932f0-0fca-4e84-a38d-cd4eb17bf60c`如实FAIL。Cpp1227/1228：新增重载测试错误地把重载后新动画的合法执行也当作旧动画继续；改为严格观察旧tween取消、计时不再前进及新co身份。Lua还暴露新依赖未纳入隔离fixture、旧静态检查匹配到错误helper、以及EOF清理后返回状态不兼容；分别修正夹具定位与真实`ended`返回，不放松时限或减少测试。
2. 定向完整运行时复核`772fde91-f1da-4d5e-a920-406b61444588`中Cpp1228、Lua主147及CTest23 passed+1允许AI skip通过，只有旧textspeed夹具失败。它在EOF后重置显示状态，改为在真实wait期间测量skip-off且断言会话仍活跃；仅复跑孤儿套件30/30通过，run `8843ca72-1076-4f47-ba33-31743998bb34`。以上失败和修复receipt保留。
3. 加入最后的延迟跳转终态回归与AI同步/GC验证后，再执行一次完整11项profile：**Cpp1229 passed、0 failed、0 skipped，388674断言；Lua主147/147与孤儿30/30；Python15/7/53/57、耦合、注册检查均通过；CTest23 passed、0 failed、1允许的真实AI服务skip，161.85s。**

最终run为`9fa6f181-343b-4069-b0f3-3f32eb2843ba`，原始receipt `artifacts/validation/raw/windows-debug-u10-04/run.json`；collect和diagnostic verify PASS。执行前后源码与夹具摘要均一致。该次是完整本机Debug验证，仍为dirty快照，不授予发布资格或未运行平台证明。当前9个单元达到本机适用/定向验收；U2及U11–U29继续。

## U10交付检查（用户决定今天不开展U11）

- `5c073cb9753c803f44f01636673498e348f54977`在独立干净检出完成完整Release profile，run `0bbb0ef2-0d76-4814-b62e-cbabcae01d60`，原始目录`raw/windows-release-u10-03`。11项检查全部通过，Cpp1229、Lua147/30、CTest23 passed+1允许AI skip；dirty=false、源码/夹具一致，collector与严格verifier通过，仅证明该profile，不代表发布批准。
- 原独立检出的U2路径修复已核对摘要后保存在stash `cb96db3f6dfb383bc70bc23e786064d608851232`，再更新到U10提交。未删除该备份。
- CI预检查修正Windows测试步骤中多余的`Pop-Location`文本，并将原生会话3项回归纳入Entry专项过滤。Debug全量构建与Entry专项通过；Web Player 370/370、现有Editor 631/631通过，记录`artifacts/validation/delivery-u10/preflight.json`及JSON报告。没有新增Studio功能。
- API普查和能力矩阵按源码重新生成；平台矩阵只同步用于新鲜度检查的源码锚点，每项平台能力仍保留实际历史验证commit/日期，未伪称重测所有平台。平台矩阵和历史计划事实块检查通过。
- 分支已推送`origin/codex/runtime-foundation`。本次交付要求为合并到master并使云端CI全绿；合并及CI结果待后续实际记录。U11仅有只读地图，未开始实现。

## 并行所有权

- resource agent：U3/U5代码与SDL回归已交付，独立只读审查完成。
- storage agent：U4/U7代码及存档回归已交付，POSIX分支另有独立运行证据。
- reload agent：重载边界、Lua回归与平台矩阵测试隔离已交付；Engine接线完成只读审查。
- U6主代理：任务系统、Null模拟、Engine屏障与回归；契约/最终代码只读审查通过；POSIX agent仅负责独立编译与证据工件。
- U8：核心补丁准备、CLI实现与独立安全审查分别负责；主代理应用补丁、统一构建/验证及文档；后续U9/U2 agent仅准备artifact补丁。
- evidence agent：U1实现和独立审查已完成。
- 主代理：构建/测试统一执行、U2执行器与profiles/CMake/CI、组合根接线、集成审查与本记录。

共享构建目录只由主代理使用。任何单元只有全部适用验收有实际证据后才能改为完成。
