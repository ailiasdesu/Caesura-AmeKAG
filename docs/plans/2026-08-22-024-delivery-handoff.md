# Caesura (AmeKAG) — 交接文档（2026-08-22 第 24 号 / 产品化 rounds 122-127）

> 面向后续 agent 的完整上下文。承接 023（rounds 118-121 Sprint 1-3）。本文件记录 **rounds 122-127**：
> 产品化 Sprint 4a/4b/5/5b/5c + Steamworks SDK 1.65 真实接入验证 + 版本单一来源修复 + 文档导航层。
> **先读 AGENTS.md（铁律）+ 本文件 + ROADMAP-200.md（rounds 权威）。**

---

## 1. 项目状态与当前基线（round 127 完成态 / 2026-08-22）

| 维度 | 基线 |
|---|---|
| 版本 | v1.0.1（CMake/tag/Release 均一致） |
| C++ 用例 | **996/996**（315813 断言） |
| Lua 用例 | **133/132 + 24 孤儿**（+elsif 别名测试） |
| Web | **298/298** |
| Editor | **604/604**（Sprint2 PM 面板 + Sprint3 asset 测试 + Import） |
| HTTP smoke | **60/60**（含 project 全端点 + import） |
| Golden Project | **18/18**（bash scripts/verify_golden_vn.sh） |
| 契约命令 | 123 |
| 接口普查 | 31 接口 / 390 方法 / **33 HTTP 端点** |
| 能力矩阵 | 82 |
| 教程库 | 16 |
| CI | 三平台 success（最近：32568076876 d4f5b195） |

---

## 2. rounds 122-127 摘要

| Round | 内容 |
|---|---|
| **122** | DebugView Run Current Scene + Reload Scene（sceneRun lib：scenePathForDoc/buildRunSceneSnippet）；交接 023 |
| **123** | Debugger 单步：后端 /api/debug/stepInto\|Over\|Out 三端点（RpcDebugResumeRequest mode dispatch）+ editor debugStep(mode) + DebugView Step Into/Over/Out 按钮；api-stats census 同步 |
| **124** | Build Manager 面板（CARC 一键调 /api/build；Web Package 诚实标注 script-only；Run 跳转 Debug）+ Golden Save v1..v5 迁移夹具（test_golden_saves.cpp 4 用例）+ [elsif] tokenizer 归一化修复（ks_check 与 runtime 解析一致化，test_elsif_alias.lua 登记 Lua→133）+ census 29→33 |
| **125** | §15 崩溃诊断 UI：DiagnosticInfo{project,engineVersion,platform,gpuBackend,scenePath,logDir} + buildDiagnosticText 纯函数（可选字段省略规则）+ [C] 复制剪贴板 / [O] 打开日志目录（SDL_OpenURL file:// percent-encode）；版本单一来源修复（SaveManager::ENGINE_VERSION 硬编码 "1.0.0" 与 CMake 1.0.1 脱节 → CAESURA_VERSION 编译宏于 CaesuraBuildOptions 接口目标全局唯一源）|
| **126** | PM Import 补全（POST /api/project/import：磁盘已有目录校验后注册为管理项目；editor Import 区表单；HTTP smoke +6 → 60/60）+ §16 基准一键化（scripts/run_benchmarks.sh：5 Lua 套件 5/5 PASS 5.77s 汇总 + tmp/bench-latest.txt 日志；docs/guides/performance-benchmarks.md 86 行） |
| **127** | Steamworks SDK 1.65 真实接入验证（用户提供 steamworks_sdk_165.zip 解压至 external/steamworks/sdk/，gitignore 不入库）：首次真实编译暴露四类缺陷全修——C2512（STEAM_CALLBACK 宏 protected 构造对非派生持有者不可达→opaque bridge + CCallbackManual 显式 Register）、ODR 布局分裂（CAESURA_HAS_STEAM PRIVATE 致栈损坏→改 PUBLIC 传播）、API 漂移×2（RequestCurrentStats 已删/GetQuota uint64*）、tests 目录缺 steam_api64.dll；steam-release.md 发布指南（159 行 ✅⏳ 分级）；docs/README.md 任务导航层（122 行 42 链接零缺失） |

---

## 3. 可复用资产（新增）

| 资产 | 位置 | 说明 |
|---|---|---|
| Compatibility Policy | docs/compatibility.md | KAG3/Neo-Genesis/save/project/Lua 兼容承诺 |
| Golden Project | tests/projects/golden_vn/ | 全 feature 回归夹具，verify_golden_vn.sh 18/18 |
| Release Gate | docs/guides/release-gate.md | 正式版门禁清单（§21） |
| 5 项目模板 | tools/project_templates/ | blank/basic/live2d/kag3/showcase |
| Project 端点 ×5 | EditorServer | templates/list/create/duplicate/import |
| Build Manager | editor/src/ide/BuildManagerView.tsx | CARC 一键 + Web 标注 |
| sceneRun lib | editor/src/lib/sceneRun.ts | 场景路径判断 + evalRaw 片段生成 |
| assetFilter/assetDrop libs | editor/src/lib/ | 资产过滤与拖放解析纯函数 |
| recentProjects lib | editor/src/lib/recentProjects.ts | 最近项目持久化（RECENT_LIMIT=20） |
| Benchmark 一键化 | scripts/run_benchmarks.sh | 5 套件汇总 + performance-benchmarks.md |
| CAESURA_VERSION 宏 | cmake/CaesuraModules.cmake L10-14 | 全模块版本单一来源（根 project()） |
| Steam SDK 集成 | src/steam/ + CMakeLists L249-268 | CCallbackManual opaque bridge；SDK gitignore 不入库 |

---

## 4. 下一步

### 需外部资源
- **Sprint 7 发布动作**：Steamworks 账号（$100 Steam Direct）→ AppID → steamcmd 上传（模板在 steam-release.md §5）→ setlive → overlay/成就/云存档真机往返。引擎侧已就绪。
- **Sprint 6 跨平台真机**：Linux(AppImage)/macOS(.app/DMG/notarization)/Web 多浏览器矩阵。
- **Sprint 8 第三方验证**：招募 5-10 名陌生开发者，Time to First VN ≤30 分钟指标。

### 纯代码侧可继续
- §13 文档物理重组评估（当前已有任务导航层软方案）
- Inspector/变量树深度、Project Manager 设置页深化等 Studio 打磨

---

## 5. 门禁

C++ 996/996 → Lua 133/132+24 → web 298 → editor 604 → golden 18/18 → http-smoke 60/60 → 耦合 PASS → api-stats 幂等（**改 EditorServer 端点后必须重生成**——freshness 门禁已三次正确拦截）。

## 6. 踩坑（本轮新增）

1. **假 500 = LNK1168**：引擎进程占 exe 致构建链接失败，curl 打旧 exe——测端点前 taskkill 引擎进程。
2. **DSH 重启窗口期启动的子代理会零事件静默失败**（日志仅 session 头一行）——重启后先用最小探针子代理验证通道再扇出真实任务。
3. **[set] 契约需 var=/value=**（旧式 f.x= 致 headless missing-param 卡死）。
4. **阻塞式 [history]/[replay]/[tween wait=true]** 在 headless 死等——golden 主路径非阻塞替代。
5. **Monaco 组件测试**：import EditorArea 拉 Monaco 需 DOM——纯函数抽独立 lib 规避。
6. **中文 cwd**：project 端点返回相对路径规避 nlohmann 序列化问题。
7. **CAESURA_HAS_STEAM 必须 PUBLIC**（SteamBackend.h 改变类布局，ODR 栈损坏且非致命静默通过——镜像 CAESURA_VIDEO_FFMPEG 先例）。
8. **CCallbackManual + 显式 Register** 替代 STEAM_CALLBACK 宏（protected 默认构造对非派生持有者不可达）。
9. **python 往 .cpp 插代码时 \" 转义会被吃**——用 edit 工具或 base64 中转。
