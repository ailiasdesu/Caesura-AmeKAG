# Caesura Studio Phase 1 — Claude Code 交接文档（2026-09-02 · goal round 98）

> **交接对象：Claude Code 会话**（本仓已配置 CLAUDE.md = Claude Code 操作手册；AGENTS.md = 规则宪章，模块边界/接口/组合根不可违反）。
> 阅读顺序：本文件（现状与下一步）→ AGENTS.md → CLAUDE.md → docs/plans/2026-09-02-031-studio-phase1-kickoff.md（立项草案，9 开放决策位，**决策源**）。
> 本文档描述的是写入时刻的真实状态（已实查核对）。编号为 032（紧随 031）。

## 1. 仓库状态快照

| 项 | 值 |
|---|---|
| 分支 / 远端 | master / ailiasdesu/Caesura-AmeKAG |
| 当前 HEAD（写本文档时） | 12d167b9（fix(cli): LF 行尾）；本文档提交后 = 文档提交 |
| CI 绿基线 | 69b932d0（r22，十 job 全绿，22.5 分钟）——红点排查以此为准 |
| 在飞 CI | r23（run 33634808148 @ 12d167b9）：in_progress /  / 2026-09-02T13:17:11Z -> 2026-09-02T13:22:30Z；r24 由本文档推送触发 |
| 工作树 | 干净（本文档为唯一新文件） |
| CI 时长基线 | ≈21-22 分钟/run；in_progress 起 >26 分钟未终态才算异常；排队期不计 |
| DSH 目标 | goal-f8a6e126-a26f-479b-924f-27e9a3fbc07c（产品化总目标，256 轮上限；本阶段 round 98） |
| 团队 | AgentTeams caesura-ship-2 五成员（ctest-wiring-2 / release-verify-2 / template-path-2 / browser-e2e-2 / reviewer-2），全 idle，无在账任务 |

提交链（本阶段）：45a3066f（草案 031）→ b83a6353 → 09c1856b（plan #5 修正）→ eace9cd2（fix(kag) layfade 契约）→ f0470234（feat(studio) 接线批）→ 2ff073fd（docs(studio)）→ e2d2777d（chore(editor) sourcemap）→ 69b932d0（fix(release) 文案）→ 5ddef9b8（feat(cli) create 元信息）→ 12d167b9（fix(cli) LF）。

## 2. 已完成：Studio Phase 1 执行批（四波，逐笔经队长磁盘核验）

### 第 1-2 波（M1 接线批 + P1/P2 修复，f0470234 认证）

- **EditorServer 静态根覆盖**（t156，f0470234）：CAESURA_EDITOR_WEBROOT 环境变量——设且 <dir>/index.html 存在 → set_mount_point(/) 挂多文件 SPA（成功打印 webroot override active 可作门禁 grep；失败响亮回退）；未设/无效 → 逐字节回退 web-editor/dist 单文件面板。token 门控只护 /api/* 不变；路由计数 36 不变。验证：增量构建 BUILD_RC=0 + 队长三层核验 + t159 复核。
- **editor 生产构建链**（t155）：editor/ 是 React/Monaco 五视图 IDE（91 tracked / 38 测试文件 / 605 it()；electron/main.cjs 壳雏形）；npm run build = tsc -b && vite build（Vite 5.4.21）；产物 base=/ 绝对 /assets/ 引用，EditorServer 根路径可同源服务；无 Router → 无深链 fallback；sourcemap 自 t164 起默认关（dist 58.75MB → 14.24MB）。
- **CMakeLists.txt:546-553**：install(DIRECTORY editor/dist/ DESTINATION editor/dist OPTIONAL)（editor/dist 被 .gitignore 排除 → 引擎-only 构建不得失败；哨兵在 verify 层）。
- **verify_release_package.sh:279-289**：断言 30→32（editor/dist/index.html + assets/*.js ≥1；无字节下限——Vite index.html ~702B 合法）。
- **ci.yml / release.yml**：三 Package job + release.yml build-windows 在 cpack 前各加 Build editor IDE 步（npm ci --no-audit --no-fund && npm run build）。
- **eace9cd2 fix(kag)**：[layfade] 契约统一（handler 兼容 to(0-255)；schema 补 opacity/alpha；showcase to=0 从 no-op 变生效）+ test_layfade_contract.lua（13 断言入主套件，主套件 144/144）+ command-contracts.md 重生成（+2 行）+ showcase/basic 模板修正。
- **CI 认证**：r21（f0470234，21.4min）与 r22（69b932d0，22.5min）十 job 全绿，含 macOS/Linux verify 32。

### 第 3-4 波（文档 + 元信息）

- **2ff073fd docs(studio)**：editor/README.md（2b. Serve the production build from the engine；命令 URL 一律 http://127.0.0.1:9876/）、editor-api-reference.md（静态根覆盖）、release-process.md（包内 editor/dist、Build editor IDE 步、30→32 口径）。
- **e2d2777d chore(editor)**：vite.config.ts sourcemap → process.env.CAESURA_EDITOR_SOURCEMAP === 1（默认关）。
- **5ddef9b8 feat(cli) + 12d167b9 fix(cli)**：caesura.py create 后处理（name=basename/--name、created=modified=ISO-8601 UTC、template 保持、[WARN] 容错）+ open(newline=LF) 跨平台 LF；测试 29/29（ctest CaesuraBuildCli）。

### 五模板 e2e 与 P1-P5（t157/t158 调查）

- 5 模板（blank/basic/live2d/kag3/showcase）create/ks_check/headless 全绿；manifest 无 displayName，name 即展示名（与任务书 §6.3:316-320 一致）。
- P1 layfade → 已修；P2 tween target=fg → 已修（[fg] 前插）；P3 basic 陈旧注释 → 已修；P4 assets 占位=模板设计（live2d model3.json 用户自供）→ 不修；P5 create 后处理 → 已修。

## 3. 待决策清单（**全部需要用户拍板，未拍板前不要开新执行批**）

1. **草案 031 决策位**：#1 已按路径 A 执行完成；#5 有答案（name 即展示名，建议关闭）；#2-#4、#6-#9 开放（#2 Inspector/Console 归属；#3 R-AB-7/8；#4 Scene Preview 形态；#6 029→Studio 注记；#7 目标 OS；#8 SE 文件管理；#9 标准 LSP/DAP 路线）。
2. **常设 3 项**：① Web Package CI 硬门（t144：推荐 A+C；web_browser_smoke.mjs:77-88 的 CHROME_PATHS Linux 修复属此批；落 A 后需更正 key 记忆 Web 包 CI 覆盖真相）；② A 类 16 条流程控制命令注册（capability closure）；③ Showcase D1-D5（docs/plans/2026-08-29-030-production-showcase-spec.md §7.1）。
3. **web-editor 11KB 面板 vs editor React IDE 产品关系**：当前并存；合并/替换待拍。

## 4. M1 尾项

- m1a 日志 RC 标记口径：build/m1a-editor-build.log 缺显式 BUILD_RC 行（m1b 有），读取以 built-in XXs 与 job 时长为准。

## 5. 关键纪律 / 运维知识（详见 DSH key 记忆；仓内规则以 AGENTS.md/CLAUDE.md 为准）

- **shell 后端**：DSH 的 pwsh 工具 = git bash 后端（C:\Program Files\Git\bin\bash.exe，2026-09-02 重装修复）。故障期降级面：read/grep/glob/edit、agent_teams_*、memory、job_output、get_goal 可用；git/构建/门禁不可用。
- **watcher 模式**：状态轮询→落盘文件→完成标记行；job_list 存活判据=目标 job id 条目在列；宿主重启后 job 编号从 pwsh-1 重置、旧 job 永不发通知（r22/r23 间发生过，判决直查补齐）。
- **CI**：串行队列；排队期不计基线；run 未 completed 前 --log 拉不到；CLI 挂死用 REST。
- **AgentTeams 账本**：captain 能 reassign 收回任务但不能 claim——亲手做完的工作转 idle 成员只读闭合；复核任务白名单必须含队长在途改动（t162 假阴性教训）；terminal task 不可改。
- **锚点传播**：上游 file:line 是声明不是事实，嵌入前 sed -n 实读；计数冲突先怀疑口径（36 = 34 /api + 2 static）。
- **测量器自检**：计数=0 即通过必须与独立指标交叉；GNU BRE 中 + 是量词，统计新增行用 grep -E 的 ^\+[^+] 形式。
- **多代理接管**：reassign 撤不了在飞 shell；接管共享产物先通知+等一轮+只读进程面扫描；接管日志独立文件名。
- **双栈 9876**：产品打印/文档/验证一律 127.0.0.1（EditorServer 只绑 IPv4；Windows 本地主机名优先解析为 IPv6 回环）。
- **分步放行**：规划工件入库供审阅；执行批需用户执行口令；暂停=修完当前红停+监控只报；继续=恢复。

## 6. 给 Claude Code 的下一动作序列

1. 先跑 CLAUDE.md 里的构建/测试命令确认环境（cmake -B build -DCAESURA_LIVE2D=OFF；主套件 run_lua_tests.lua 144 用例；CLI 测试 test_caesura_build_cli.py 29 用例）。
2. 收割 r23/r24（build/ci-r23.log、build/ci-r24.log；绿则 CI 绿基线推进到对应 HEAD；红点排查以 69b932d0 为绿参照，注意三 Package job 的 Build editor IDE 步）。
3. 等用户对 §3 的拍板；拍板后按草案 031 开执行批（AgentTeams 优先，成员钉 deepseek-official/deepseek-v4-flash-vision-exp；队长契约/共享耦合点独占：main.cpp、BackendRegistry、kag/init.lua、tests/CMakeLists.txt、api/I*.h）。

## 7. 环境与复现速查

- editor 构建：rm -rf editor/dist && (cd editor && npm run build)（187 文件/约 14.24MB）；
- 服务验证：CAESURA_EDITOR_WEBROOT=editor/dist ./build/Debug/CaesuraAmeKAG.exe --editor → http://127.0.0.1:9876/；
- Lua 解释器：build/lua/Debug/lua.exe；主套件 run_lua_tests.lua；孤儿套件 run_orphan_tests.lua；
- CLI 测试：python tests/scripts/test_caesura_build_cli.py（pytest 未装，用文件自身运行方式）；
- 模板：python scripts/caesura.py create --template blank|basic|live2d|kag3|showcase <dir> [--name X] [--description Y]；
- build/ 日志索引：m1a-*（editor 构建）/ m1b-*（EditorServer 编译）/ m1c-*（五模板 e2e 82 行）/ m1d-*（打包接线清单 120 行）/ ci-r2x.log（CI watcher）/ g-*.log（本地门禁）。
- 关键锚点：EditorServer.cpp:1395-1455；CMakeLists.txt:546-553；verify_release_package.sh:279-289；ci.yml/release.yml 的 Build editor IDE ×4；vite.config.ts sourcemap；caesura.py cmd_create。
