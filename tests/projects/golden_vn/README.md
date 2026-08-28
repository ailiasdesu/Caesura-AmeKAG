# Golden Project v1 (tests/projects/golden_vn/)

> **长期回归夹具**——产品化总任务书（§14 / release-gate.md §7）要求每次 release 都完整跑一遍的
> 黄金项目。既有基线 = **d39b12d0**（story.ks 166 行 + entry.lua + README + `scripts/verify_golden_vn.sh`
> 18/18 端到端门禁）；本文件记录 **v1 增量**（在既有之上扩展，全部保持 26/26 门禁）。
> **v1 禁止声称全覆盖**——下方清单按【既有（d39b12d0）／v1 增量】分列，未覆盖项如实列出。

## 目录

| 文件 | 作用 |
|---|---|
| `story.ks` | 主场景（dialogue / choices A·B / [eval] 变量 / save+load / macro / 中英文本 / i18n / audio / tween / layout / 转场 / credits） |
| `golden_cross.ks` | 跨场景 [jump] 专用起点（独立覆盖面；"选择后立即跨场景"的直连场景在 tests/scripts/test_select_crossscene_flow.lua——引擎限制已于 t38 修复） |
| `scene_b.ks` | 跨场景 [jump] 目标场景（独立收尾，进入即 [end]） |
| `entry.lua` | 真实 GPU 运行入口（标准 UI wiring，同 demo/example_game 模式） |
| `../scripts/golden_vn_headless.lua` | v1 headless 驱动（route 选择 + 四大功能旗标断言 + cross 模式） |
| `assets/` | 引用仓库共享资产（assets/bg|fg|bgm|se|voice），**不新增二进制** |

## 运行

```bash
# 静态契约（门禁 1）：零违规（golden_cross 有 2 条预期性 [WARN]——见下）
build/lua/Debug/lua.exe scripts/ks_check.lua tests/projects/golden_vn/story.ks
build/lua/Debug/lua.exe scripts/ks_check.lua tests/projects/golden_vn/scene_b.ks
build/lua/Debug/lua.exe scripts/ks_check.lua tests/projects/golden_vn/golden_cross.ks

# headless 双路线（门禁 2）：route A / route B
GOLDEN_ROUTE=1 build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua
GOLDEN_ROUTE=2 build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua

# headless 跨场景（门禁 3）
GOLDEN_CROSS=1 build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua

# 完整门禁（静态契约 + headless 全跑 + 分支可达 + feature 覆盖 + v1 旗标）
bash scripts/verify_golden_vn.sh        # 实测 26/26 PASS

# 真实 GPU 窗口运行
build/lua/Debug/lua.exe tests/projects/golden_vn/entry.lua
```

## 覆盖清单（既有 d39b12d0 / v1 增量）

### 既有基线（d39b12d0，本次未改动/仅保留）
| Feature | 位置 | 状态 |
|---|---|---|
| dialogue（中英双语文本） | [ch]/[p] 主路径 | 既有，未改 |
| branch choices A/B（[select]/[sel] 块本身） | *choice_moment | 既有，未改（仅分支内新增 f.route 记录——见增量） |
| [save slot=9]（写点） | *choice_moment | 既有，未改（其后新增 savedByStory 标记——见增量） |
| NVL 模式 token | *common_mid | 既有，未改 |
| tween / layout / layout_slot | *common_mid | 既有，未改 |
| i18n 热切换 en/ja/zh | *i18n_check | 既有，未改 |
| audio（bgm/se/voice）/ [if] / [trans] / credits / [end] | 既有各段 | 既有，未改 |
| replay / mod（source 注释面） | Section D | 既有，未改（专项测试覆盖） |

### v1 增量（本次新增/增强，真缺口）
| Feature | 增量 | headless 旗标 |
|---|---|---|
| [eval] 表达式变量 | Section A0（[eval exp="f.energy = f.energy + 5"] + [if 15] 断言）——既有仅 [set]+[if]，无 [eval] | EVAL_OK |
| [load] 点 | *common_mid（[load slot=99] 未写槽位 graceful-miss + [set f.after_load] 标记）——既有仅 [save] 无 [load] | LOAD_MISS_OK |
| macro（参数化） | [macro route_note args=...]/[endmacro] + 调用 [route_note where=...]——既有全仓无 macro 使用 | MACRO_OK |
| 跨场景 jump | golden_cross.ks（新）+ scene_b.ks（新）+ load_tokens 重映射 seam——既有 story.ks 全为场景内跳转 | XSCENE_OK |
| 分支路由断言（enable 项） | 两分支 [set var="f.route" value="forest"/"city"]——既有分支不记 route，route 断言不可实现（first_vn 同款模式）；GOLDEN_ROUTE=1/2 → ROUTE forest/city | ROUTE |
| 写点标记（enable 项） | [save slot=9] 后 [set f.savedByStory=1] | FLAGS 行 |
| 驱动 | tests/scripts/golden_vn_headless.lua（仿 first_vn_headless 扩展；tests/scripts 此前**无** golden 专用驱动，既有入口=sample_game_headless.lua 通用驱动，保持原用并向 verify_golden_vn.sh Step 4b 增补专用断言） | — |
| 门禁 | verify_golden_vn.sh：既有 18 项检查全部保留（14 项 feature 清单未删一项），新增 5 项 feature grep（load/eval/macro/jump/set）+ Step 4b 三组（route1/route2/cross） | 26/26 |

## v1 未覆盖（诚实清单，禁止声称覆盖）

| Feature | 状态 | 原因 / 现有专项覆盖 |
|---|---|---|
| rollback / history | **未覆盖** | 阻塞式弹窗，headless 死等；由 sample_game_headless（替代 [notify]）与 kag/snapshot 单测覆盖 |
| backlog | 未作为 v1 断言 | 同上 |
| NVL 模式语义 | story 含 [nvl] token（source 面），**v1 不做语义断言** | 后续 v2 |
| 语音（voice）语义 | story 含 [playvoice] token（source 面），headless 仅 mock 返回 false | 后续 v2 + 真机 |
| Live2D | **未覆盖** | SDK 依赖，v2+ |
| Steam | **未覆盖** | SDK 依赖，Phase2 |
| replay 实跑 / mod 实跑 | 未覆盖（story 注释保留 source 面） | replay.lua / mod 单测专项 |
| 存档真实读写（真机语义） | 未覆盖 | v1 只用未写槽位 miss 路径 + 写槽位 token 派发；真实 roundtrip 属 v2 |

## 跨场景 jump 痕迹说明（诚实记录）

调度器把非 `*` 的 `[jump]/[call]/[link]` storage 目标固定解析为
`assets/script/<target>.ks`（`scripts/scheduler.lua` `is_safe_scene_path`）——引擎运行时的
场景布局约定。为把夹具源码保留在 `tests/projects/golden_vn/` 下，headless 驱动
`tests/scripts/golden_vn_headless.lua` 在 GOLDEN_CROSS=1 时把 `ctx.load_tokens` 的
`assets/script/golden_scene_b.ks` 逻辑名**重映射**到夹具文件（打印 XSCENE_REMAP 自证）。
这是唯一的测试缝：调度器跨场景机制（前缀构建 / 安全检查 / switch 预算 / token+label 交换 /
新 local frame）全部真实执行。**为什么用独立 golden_cross.ks（不在 story.ks 主路径）**：
"选择后立即跨场景 [jump]"曾因 [select] 的延迟 pending-jump（在下一处协程死亡点结算，见
`scripts/kag/commands/text.lua:1475` 与 `kag_runner.lua` 死亡分支）在 新场景 labelMap 中
解析旧标签（"Choice label not found"）而运行停摆——**该引擎限制已于 t38/t43 修复**
（`scripts/kag_runner.lua` 死亡分支：场景切换先于 pendingJump 消费）。丢弃语义仅适用于
**无返回的跨场景 [jump]/[link]**（切换即新流权威，场景局部标签在新场景不存在，残留延迟
跳转被响亮丢弃）；**跨场景 [call] 有返回语义**——`[return]` 恢复调用者场景并清除切换信号，
选择后的延迟跳转仍在调用者场景内解析、选择分支照常重放（t43 回归锁定）。约束：**选择标签须属于
恢复后场景**——若选择发生在 callee 内且 [return] 前未消费，其标签必不在调用者 labelMap，
由 [return] 恢复点响亮丢弃而非停摆（t49 回归锁定，Case E）。直连复现/回归在
`tests/scripts/test_select_crossscene_flow.lua`（Case B：选择→立即跨场景 [jump]——修复前
FRAME_LIMIT 停摆 / 修复后 DONE 且新场景推进到 [end]；Case A 锁旧语义不回归；Case C：
选择→跨场景 [call]→[return]——回放保留（route=b）；孤儿套件注册）。golden_cross.ks + XSCENE_REMAP 保留为独立的
跨场景覆盖面（不与选择叠加，覆盖面互不干扰）。story.ks 内 [jump] 均为场景内标签跳转，
因此主路径仍不需要该缝。另外 golden_cross.ks 会触发 2 条
ks_check 预期性 [WARN]：其一 "cross-scene target scene 'golden_scene_b.ks' not found in
scene directory"——静态视角看不到 headless 驱动的 load_tokens 重映射缝（与运行时 XSCENE_REMAP
是同一测试缝的两面）；其二 "token(s) unreachable after [jump]"——回退路径行。两条均属 lint
层不增计数，exit 0；见 scripts/ks_check.lua:48-51。

## 与 first_vn / package_game 的同构关系

- 结构同 `tests/projects/first_vn/`（story.ks + entry.lua + assets/ 引用共享池；同款
  [select] 延迟 pending-jump 行为——first_vn 的 ROUTE sun/rain 验证模式与本项目一致）。
- `package_game.sh tests/projects/golden_vn` 可直接打包为 Web 站（多 .ks 目录会被收集并逐档
  ks_check；web 端 bundle 化后跨场景由 story bundle 内 scene 键按名解析）。
- 门禁链：`verify_golden_vn.sh`（4 步 + 4b v1 旗标，实测 26/26 PASS）→ 本地/CI 全量门禁。
