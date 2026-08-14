# Caesura (AmeKAG) — 交接文档（2026-08-14 第 19 轮迭代）

> 面向后续 agent 的完整上下文。本轮为**创作工具链轮**：
> SMA 资产校验器（sma_check）+ 资产模板/示例角色/损坏示例 + SMA demo 展示场景
> + GPU 蒙皮性能基准（8k 顶点 CPU 1.27ms vs GPU 0.08ms，≈15.8×）
> + /api/sma/validate 端点 + IDE SMA 资产面板 + 分语义提交 + 推送。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交）

| 提交 | 内容 |
|---|---|
| `feat(script)` | **SMA 资产校验器** `scripts/kag/sma_check.lua`：库（validate/validate_file 含 meta 摘要）+ CLI（退出码 0/1，CI 门禁）；规则与运行时消费契约严格一致（骨骼 id 唯一/parent 无环/pivot；网格 positions/uvs/indices %3 不越界/weights 1 或 2 交错条目 w∈[0,1] 和=1；动画 duration/track bone/frames t 升序；parts 变体递归）；**修复 create_part_mesh 双骨权重错位**（改为 [2i-1]/[2i] 交错布局，18 轮未覆盖 weights 路径）；sma.load(validate=true) 门禁 + sma.validate/validate_file；kag/init.lua 预加载登记 |
| `feat(assets)` | **资产模板与示例**（demo/assets/sma/）：template.json（_comment 说明）、hero.json（8 骨/5 部件/eyes 双变体/idle+wave 动画/肘部双骨混合）、_broken_example.json（_intent 标注的故意损坏演示资产） |
| `feat(kag)` | **SMA demo 展示场景**：demo/sma_demo_driver.lua（测试共享驱动）+ demo/sma_demo.ks（[sma_anim]/[sma_variant]/[sma_ik]/[sma_stop] 剧本）+ demo/sma_entry.lua（独立入口，运行时 create_solid_texture 注入纹理——仓库无 PNG 素材） |
| `test(render)` | **GPU 蒙皮性能基准**：8k 顶点/64 骨/~16k 三角，120 帧主机侧计时——CPU 1.268ms vs GPU 0.080ms（≈15.8×），断言 gpu < cpu（WARP 亦成立，只测主机侧） |
| `feat(rpc+editor)` | **/api/sma/validate 端点**：RpcSmaValidateRequest/Result，main.cpp 经 luaL_loadstring 跑共享校验器（路径安全：assets/ 或 demo/assets/ 前缀 + 禁 .. + 禁绝对路径）；stdio smaValidate；VisualView "SMA ASSET" 面板（✓/✗ + 错误列表 + 结构摘要）；headless_http_smoke 4 新断言 |
| `fix(script)` | **command-contracts.md UTF-16 修复**：PowerShell `>` 重定向默认 UTF-16LE 把 18 轮生成的文档写坏，导致 test_schema 的 nameplate 检查失败并中断整个 Lua 套件（本地门禁假绿隐患）；转回 UTF-8 无 BOM |
| `docs` | 设计文档 §2.2 权重布局精确化 + §9c 创作工具链 + §9 性能基准数字 + §10 风险表（2 项闭环）；editor-api-reference /api/sma/validate；api-stats（626/6117/120/22 端点/26 方法）；交接 019 |

## 2. 关键实现细节与坑（务必记住）

- **RPC Lua 执行通道**：main.cpp 的 RpcSmaValidateRequest 分支仿 18 轮 getState
  模式（luaL_loadstring + lua_pcall + nlohmann 解析）；Lua 端把 errors/meta
  序列化成 JSON 字符串返回（%q 转义）。
- **沙箱 io.open 白名单**（scripts/sandbox.lua）：只允许 scripts/ assets/
  tests/ 读——本轮加 **demo/** 前缀（SMA 资产在 demo/assets/sma/）。
  validate_file 的 io.open 在引擎沙箱内执行，白名单外的路径报
  "cannot open file"。
- **edit 替换陷阱**（本轮翻车）：把 getState 分支收尾（lua_settop + return
  reply）整体替换成新分支开头，导致 if-else 链里 getState 无 return（C4715
  警告 + 运行期 0xC0000005 崩溃）——**修改 if constexpr 链时务必保留原分支
  的 return 收尾**。
- **PowerShell `>` 重定向 = UTF-16LE**：生成 markdown 用重定向会写坏文件
  （read 工具报 binary、Lua find 找不到子串）。生成文档用 `lua ... | Set-Content
  -Encoding utf8` 或 python 写文件。
- **sma_check CLI 门**：basename 匹配（`arg[0]` 结尾 == sma_check.lua），
  否则 require 场景（arg[0] 是主 chunk 路径如 test_sma_check.lua）会误入
  CLI 分支 os.exit 提前终止宿主测试。
- **demo 复制到引擎输出**：tests/CMakeLists.txt sync 脚本加
  `file(COPY demo/ → $<TARGET_FILE_DIR:CaesuraAmeKAG>/demo)`（此前只有
  scripts/；headless smoke 的 CWD 是引擎输出目录）。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| P1-6 Live2D GL/Steam 实机 | **待设备** | GL 需 Linux/macOS 硬件；Steam 需开发者账号 |
| P0-1 Metal 后端真机验证 | **待设备** | macOS 硬件；Metal 计算蒙皮当前回落 CPU |
| P0-3 移动真机验证 | **待设备** | Android/iOS 设备；管线与文档已就绪 |
| /api/pick 预览帧点击拾取 | 可闭环 | IDE 深化下一候选（18 轮计划顺延项） |
| SMA 骨骼可视化编辑器 | 可闭环 | 创作工具链后续里程碑 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 626/626（6117 断言，含 GPU 蒙皮子测试 +
性能基准）→ Lua 120/120（test_sma 74 + test_sma_check 39 + test_sma_demo
24 + 既有）→ ctest 10/10（+AI smoke 77 跳过）→ 耦合 PASS → api-stats 重生成。

## 5. 注意事项

- **demo 实机**：demo/sma_entry.lua + sma_demo.ks 是独立 SMA 展示入口
  （主 demo 不动）；实机画面需本地 GPU 窗口验证（CI 以 headless 冒烟 +
  mock 驱动保证逻辑）。
- **编辑器前端**：改动在 editor/src（rpc.ts smaValidate + VisualView SMA
  面板 + styles.css）；tsc 通过；editor/dist 不入库（既定规则）。
- **性能数字**：CPU 1.268 ms/帧 vs GPU 0.080 ms/帧（8k 顶点/64 骨/主机侧），
  写入设计文档 §9。
- 历史交接：`2026-08-14-018-delivery-handoff.md` 为上一权威状态。
