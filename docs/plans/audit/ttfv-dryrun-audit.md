# TTFV 干跑审计 — docs/guides/getting-started.md 全路径对拍

> 审计对象：docs/guides/getting-started.md（速通 KPI-1 五步 + §0-§10）。
> 方式：逐条干跑（只读仓库 + 系统临时目录产物，已清理）；禁全量构建——凡需全量构建
> 步骤标注 UNVERIFIED 并给原因，不假装验证。
> 执行时刻：2026-08-29；仓库 HEAD 1af807c0（工作树另有在途批次）。环境=Windows 11 +
> git bash（工具名 pwsh 实为 bash 后端）+ build/lua/Debug/lua.exe + 既有 build/Debug 产物。

---

## 1. 分步结果表

| # | 文档步骤（位置） | 命令（原文/干跑等价） | 口径 | 实际输出摘要 | 耗时 | 判定 |
|---|---|---|---|---|---|---|
| 1 | §速通 Step1 构建（L40-43） | cmake -B build（无 -G；工具链自动化） | git bash | ①本会话子进程"无法发现 VS 实例"rc=1（进程环境受限）；②无 -G 时 MSYS 缺省选择异常（首跑报 CMAKE_C_COMPILER not set） | ~30s | 部分/环境受限：build/CMakeCache.txt 实证 SDL3_DIR:PATH=.../external/SDL3/SDL3-3.2.0/cmake 且 0 条 vcpkg 条目，'内置 SDL3、vcpkg 非必需'成立；但速通缺省命令在 git-bash 不可靠（需 -G 'Visual Studio 17 2022' -A x64，§2.1 已给） |
| 2 | §速通 Step2 create（L44-47） | python scripts/caesura.py create my_vn --template basic -o /tmp/t42/my_vn（在 /tmp 执行） | git bash | [OK] ... template from: D:\...\tools\project_templates\basic rc=0；产出 README/assets/caesura.project.json/entry.lua/story.ks | ~1s | 与文档一致（任意 CWD 成立） |
| 3 | §速通 Step3 编辑（L48-52） | 文本编辑 N/A；editor/ React IDE 面 | — | editor/package.json、editor/src/ide/BuildManagerView.tsx 等存在；editor/node_modules 已存在 | — | 部分：形态属实；npm ci && npm run dev UNVERIFIED（重装依赖+在途 editor 套件，资源红线） |
| 4 | §速通 Step4 运行（L53-55） | ./build/Debug/CaesuraAmeKAG.exe --frames 60（§3 窗口运行的确定性代理） | git bash | rc=0，D3D11 初始化、shut down cleanly，0 FATAL | ~5s | 引擎默认 demo 可跑（代理证据） |
| 5 | §速通 Step4 ks_check（L54） | build/lua/Debug/lua.exe scripts/ks_check.lua my_vn/story.ks | git bash | OK: all scenes pass contract checks rc=0 | <1s | 一致 |
| 6 | §速通 Step4 '无 GPU 也能跑逻辑'（L55） | build/lua/Debug/lua.exe scripts/kag_runner.lua my_vn/story.ks | git bash | 运行即报 module 'flow' not found 爆栈（kag_runner 是模块，裸跑缺 scripts/?.lua package.path） | <1s | 断点 P0 |
| 7 | §速通 Step5 桌面包（L57-61） | python scripts/caesura.py build /tmp/t42/my_vn -o /tmp/t42/my_vn-game → 运行 --frames 60 | git bash | build rc=0（GAME-ONLY BUILD COMPLETE）；game rc=0、KAG Runner] Started 1 次；但日志含 [Template] FATAL: cannot find story.ks (run from repo root...)=良性回退探针（verify_release_package.sh §5 已判定该前缀为误报源） | ~40s | 流程 OK；文档'日志无 FATAL'与实际不符（P1） |
| 8 | §速通 Step5 归档（L61） | python scripts/caesura.py package ... --target windows -o /tmp/t42/pkg | git bash | rc=0 → my_vn-win64.zip (79.94 MB)+PACKAGE COMPLETE (1 artifact) | ~30s | 一致 |
| 9 | §速通 Step5 Web 站（L62-63） | bash scripts/package_game.sh --out dist/my_vn my_vn | git bash | UNVERIFIED：该命令默认重建 web/dist（--no-web-build 才跳过）——与在途 web 套件共享状态（round5 实证耦合红线） | — | 只读规避；需净窗 |
| 10 | §速通 --editor 小节（L65-82） | 单文件面板/令牌行为描述 | — | 与 Sprint4 已修状态一致（web-editor/dist/index.html 存在；之前的 t3/t5 实测） | — | 描述相符（未重跑进程——端口卫生属其他用例） |
| 11 | §0 工具表（L98-108） | Lua 免安装断言 | — | build/lua/Debug/lua.exe 存在；external/lua/lua.exe 树内无（发布包才有） | — | 与事实相符 |
| 12 | §1.1 Windows 依赖（L116-134） | vcpkg 安装 SDL3 | — | 与速通'不传 toolchain 自动用内置 SDL3'自相矛盾（多余步骤 +5min） | — | 文档内部矛盾（P1） |
| 13 | §2 克隆（L195-205） | git clone --filter=blob:none ... | — | 未执行（网络）；形态合理 | — | 未跑（网络） |
| 14 | §2.1 配置/构建（L232-240） | cmake -B build -S . -G 'VS 17 2022' -A x64（vcpkg 行） | PowerShell | UNVERIFIED（本会话子进程无法发现 VS 实例——环境受限；缓存证据见 #1） | — | 需净窗验证（源码构建已存在于 build/） |
| 15 | §3 参数说明（L285） | ./build/Debug/CaesuraAmeKAG.exe --help | git bash | --help 被忽略，直接启动引擎窗口（SDL 初始化到 D3D11）；main.cpp grep 无任何 --help 处理 | ~5s | 文档口径错误（P1）——参数表本身真实（--frames/--headless/--editor 等均存在） |
| 16 | §4.1 C++ 测试（L306-318） | cd build/tests/Debug && ./CaesuraTests.exe | git bash | 1120 passed / 0 failed / 0 skipped；385790 断言全过 rc=0 | ~3-4min | 与文档描述逐字一致 |
| 17 | §4.1 CTest（L330-337） | ctest --test-dir build -N | git bash | Total Tests: 15（#15 CaesarGoldenVn 已入）——文档说 14 个 target | <2s | 陈旧（P2） |
| 18 | §4.2 Lua 套件（L339-349） | run_lua_tests.lua / run_orphan_tests.lua | git bash | 主 143/143；孤儿 25/25（t38 登记新测试后） | ~2-4min | 主一致；孤儿文档说 24→实际 25（陈旧 P2，基线段已注明'数字随开发增长'但显式数字仍错） |
| 19 | §4.3 静态契约（L355-363） | 16 教程 + example_game | git bash | 16/16；example_game 全 OK | ~5s | 一致 |
| 20 | §4.4 gen-index --check（L365-370） | node web/gen-index.mjs --check | git bash | CHECK OK: 80 modules up to date rc=0 | ~5s | 一致（输出含中文路径 mojibake 一行=控制台编码，非错误） |
| 21 | §5.1 示例片段（L380-401） | ks_check /tmp/t42/my_first_scene.ks | git bash | OK: all scenes pass contract checks rc=0 | <1s | 片段契约零违规 |
| 22 | §5.2 KAG runner 直跑（L414） | lua scripts/kag_runner.lua my_first_scene.ks | git bash | 运行即 module 'flow' not found（同 #6） | <1s | 断点 P0（同族） |
| 23 | §5.3 模板（L424-431） | demo/template/ 存在性 + lua my_game/entry.lua | git bash | demo/template/{README,assets,entry.lua,story.ks} 存在；verify_template.sh、template-quickstart.md 存在；但 lua my_game/entry.lua 裸跑 → 同 P0 族（entry.lua 无 package.path 前缀，require('kag_runner') 即爆） | — | 模板存在；'lua my_game/entry.lua' 断点 P0（同族） |
| 24 | §5.3 example_game（L432-437） | lua demo/example_game/entry.lua | git bash | 运行即 module 'kag_runner' not found（裸跑） | <1s | 断点 P0（同族） |
| 25 | §6 Web 播放器（L455-467） | npm ci / npm run dev:web / npx vite build | — | web/node_modules 已存在；其余 UNVERIFIED（npm ci 重装 + vite build 改 web/dist 共享工件 + 在途 web 套件） | — | 需净窗 |
| 26 | §7 视频导出（L476-483） | --export-replay r.json ... | — | UNVERIFIED（需真实回放文件 + GPU 窗口交互；无现成 demo_replay.json） | — | 需净窗/真机 |
| 27 | §8 FAQ（L487-545） | 各条目 | — | 抽查：中文路径/括号路径（L509）与本项目路径一致；'lua 命令找不到'（L526-529）指向 build/lua 正确；其余与代码事实无冲突 | — | 抽查通过（但未涉及 #22-#24 的 package.path 根因） |
| 28 | §9 Checklist（L549-597） | 逐项 | git bash | count_coupling PASS；platform-status [OK] up-to-date；gen-index OK；tutorials OK；lua scripts/kag_demo_entry.lua 运行即 module 'kag_runner' not found（P0 同族）；verify_first_vn.sh UNVERIFIED（其 Step12 调 package_game→重建 web/dist 共享状态——在途套件红线）；editor npm ci && npm run dev UNVERIFIED（在途 editor 套件）；--editor curl 系列=Sprint4 断言已覆盖（本审计未重复起引擎——端口卫生） | 多数 OK | 混合 |
| 29 | §9.2 CTest 行（L566） | 14 target | — | 实际 15（见 #17） | — | P2 |

---

## 2. 断点清单（按可复现失败排序）

| 断点 | 证据 | 级别 |
|---|---|---|
| P0-1 所有'裸 lua 入口'命令失败：lua scripts/kag_runner.lua <scene>（#6/#22）、lua scripts/kag_demo_entry.lua（§9 #28）、lua demo/example_game/entry.lua（#24）、lua my_game/entry.lua（#23）、lua tests/projects/golden_vn/entry.lua（t33 同源）——统一原因：入口文件 require('kag_runner') 而裸 lua 默认 package.path 不含 scripts/?.lua；有 package.path 前缀的是 tests/scripts/*_headless.lua 类驱动 | 5 处实跑 module 'kag_runner/flow' not found | P0 |
| P0-2 速通 Step4 的'无 GPU 也能跑逻辑'承诺系于 P0-1 的那条命令 | 同上 | P0 |
| P1-1 §3 '--help 列出全部'——无 --help 处理，传了反而启动引擎窗口 | #15 实跑 | P1 |
| P1-2 速通'日志无 FATAL'——game 运行日志含 1 条良性 [Template] FATAL（模板 entry 的仓库相对路径探针，verify_release_package.sh §5 已知文案误导）；新手会误判构建失败 | #7 | P1 |
| P1-3 §1.1 vcpkg 必需 vs 速通'自动内置 SDL3' 自相矛盾（§1.1 多要求 5 分钟 vcpkg 安装） | #12 | P1 |
| P1-4 速通 Step1 缺 -G 的 git-bash 失败面（非 PowerShell 口径；需注明或给双命令） | #1 | P1 |
| P1-5 verify_first_vn.sh 13/13（§9）'应 PASS'的表述 + 其重建 web/dist 的共享状态风险未提示（顺序要求） | L570 | P1 |
| P2-1 ctest '14 个 target' → 15（CaesuraGoldenVn 已进 ctest #15） | #17 | P2 |
| P2-2 孤儿套件 '24' → 25（t38 test_select_crossscene_flow 登记） | #18 | P2 |
| P2-3 §4 基线数字行已注明'以实跑为准'（好），但两处显式 14/24 未同步 | #17/#18 | P2 |

**已一致/已覆盖（非缺口）**：ks_check 全链路、C++ 1120/385790、create、build/package CLI、--frames 60 冒烟、count_coupling、platform-status、gen-index、tutorial 16、demo/template、模板工具链（tools/project_templates + demo/template）、web-editor 单文件面板说明、Sprint4 令牌行为说明。

---

## 3. 『30 分钟目标』风险评估

**当前：风险高（红）**。拆解：

| 阶段 | 现实耗时 | 风险 |
|---|---|---|
| 克隆 | 1-3 min（filter=blob:none） | 低 |
| 环境准备 | 5-15 min（VS2022 + CMake；§1.1 的 vcpkg 多余步骤 +5min） | 中（文档矛盾） |
| 配置+构建 | 5-15 min（16GB/高核）~30+ min（低配）——KPI 预算的大头 | 高（未压缩；Release ZIP 下载路径已存在但未被速通列为首选，文档只口播） |
| create+编辑 | 2-5 min | 低（create 通过） |
| 运行（ks_check+逻辑跑） | 1-2 min | 高（P0-1：命令即炸，卡死在这里） |
| 打包 | 1-3 min（build 通过 / WEB 包需净窗） | 中 |
| 校验（§9 清单） | 5-15 min | 中（verify_first_vn/editor/web 未跑即未证） |

**结论**：
1. P0-1 单点即把'陌生开发者 ≤30 分钟完成首个 Tiny VN'击穿（复制 §5.2/§9 命令必失败）；不清除 P0-1，KPI 红线。
2. 构建为时间大头：30 分钟目标要成立，必须把「下载 Release ZIP + 解压即建即跑」升为速通首路径（Sprint4 已具备：create/build 包内跨目录），源码构建降为 §1-2 备选——文档目前以源码构建为起点（L19-20 自认），这与 KPI-1 矛盾。
3. P1-2（FATAL 文案）会让新手在'游戏已跑通'时仍判定失败——修复模板 entry 探针文案（或文档说明）后消除。

---

## 4. 修复建议（P0 / P1 / P2）

### P0（断点，先修）
1. 入口文件自带 package.path（治本）：给 demo/template/entry.lua、demo/example_game/entry.lua、tests/projects/golden_vn/entry.lua、scripts/kag_demo_entry.lua 顶部加一行 package.path = 'scripts/?.lua;scripts/?/init.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;' .. package.path（与 tests/scripts/*_headless.lua 一致）。归属：模板/示例文件（各文件小改，不涉引擎）。
2. 文档同时止血：速通 Step4 / §5.2 / §9.3 的'无 GPU 逻辑验证'改为 SAMPLE_STORY=my_vn/story.ks build/lua/Debug/lua.exe tests/scripts/sample_game_headless.lua（可复现、输出 RESULT DONE）；§5.3 lua my_game/entry.lua 注明需入口已补 package.path（或同款驱动式示例）。
3. KPI 首路径重排：速通 Step1 改为「下载 Release ZIP → 解压 → python scripts/caesura.py create → build → 运行」（Sprint4 已闭环，t4/t27 实测）；源码构建挪 §1/§2 并明确标注'30 分钟按 Release 包口径、源码构建另计'。

### P1（摩擦）
4. §3 删除'--help 列出全部'或实现真 --help（推荐：main.cpp 加 --help/-h 分支——引擎参数面已有成熟解析）。
5. 模板 basic/example 的 [Template] FATAL 探针文案改 [Template] NOTE: repo path probe ... continuing 或文档澄清（verify_release_package.sh 已把 [caesura] FATAL 作为真致命判据——统一前缀会更好）。
6. §1.1 加'可选：若用仓库内置 SDL3（推荐），跳过 vcpkg'或改 §1.1 为快速+可选两栏；速通与 §1 口径统一。
7. 速通 Step1 命令补 -G 'Visual Studio 17 2022' -A x64（Windows）或提示 git-bash 差异。

### P2（打磨）
8. ctest 14→15；孤儿 24→25（含说明'数字随开发增长'）；两处显式数字刷新。
9. §4 基线段与 §9 清单数字同源刷新（或直接删除显式数字只留'以实跑为准'）。
10. FAQ/Q 补充：为何裸 lua 需要 package.path（指向 P0-1 的说明）。

---

## 5. 证据留存

实跑日志（本审计期临时文件，已清理）：/tmp/t42_cpptests.log（1120/385790）、/tmp/t42_lua_main.log（143）、/tmp/t42_lua_orph.log（25）、/tmp/t42_build.log、/tmp/t42_game.log（FATAL 行 110）、/tmp/t42_pkg.log、/tmp/t42_demo.log、/tmp/t42_cfg.log（环境受限 rc=1）。
仓库只读：未改任何既有文件；仅新建本文档。
