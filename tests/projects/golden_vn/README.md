# Golden Project v2 (tests/projects/golden_vn/)

> **长期回归夹具**——产品化总任务书（§14 / release-gate.md §7）要求每次 release 都完整跑一遍的
> 黄金项目。既有基线 = **d39b12d0**（story.ks 166 行 + entry.lua + README + `scripts/verify_golden_vn.sh`
> 18/18 端到端门禁）；本文件记录 **v1 增量**（26/26 门禁）与 **v2 增量**（2026-08-29，
> 30/30 门禁——rollback/history/backlog 语义断言 + 真实 save→load roundtrip）。
> **v2 仍有未覆盖项**——下方清单按【既有（d39b12d0）／v1 增量／v2 增量】分列，未覆盖项如实列出。

## 目录

| 文件 | 作用 |
|---|---|
| `story.ks` | 主场景（dialogue / choices A·B / [eval] 变量 / save+load / macro / 中英文本 / i18n / audio / tween / layout / 转场 / credits）+ v2 Section E/F（rollback/history 语义段落，主 [end] 之后、仅 headless stage jump 可达） |
| `golden_cross.ks` | 跨场景 [jump] 专用起点（独立覆盖面；"选择后立即跨场景"的直连场景在 tests/scripts/test_select_crossscene_flow.lua——引擎限制已于 t38 修复） |
| `scene_b.ks` | 跨场景 [jump] 目标场景（独立收尾，进入即 [end]） |
| `entry.lua` | 真实 GPU 运行入口（标准 UI wiring，同 demo/example_game 模式） |
| `../scripts/golden_vn_headless.lua` | v1 headless 驱动（route 选择 + 四大功能旗标断言 + cross 模式）+ v2/v3 语义模式（GOLDEN_RB / GOLDEN_HISTORY / GOLDEN_ROUNDTRIP / GOLDEN_NVL / GOLDEN_VOICE） |
| `../../tests/scripts/golden_rt.ks` | v2 真实存档 roundtrip 场景（tests/scripts/ 放行路径——原因见 v2 覆盖表下方说明） |
| （无本地 assets/） | 本项目**不含**本地 assets/ 目录；场景中的资源路径（assets/bg|fg|bgm|se|voice 前缀，如 story.ks:40 [bg storage="assets/bg/classroom.png"]）由引擎在仓库根 CWD 下直接解析到**仓库共享资产池**（assets/），项目不新增任何二进制 |

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

# headless v2 语义模式（门禁 4c；仅 headless 驱动，正常 gate 路径不经这些段落）
GOLDEN_RB=1       build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua  # rollback 语义
GOLDEN_HISTORY=1  build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua  # history/backlog
GOLDEN_ROUNDTRIP=1 build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua # 真实存档 roundtrip

# headless v3 语义模式（门禁 4d/4e；同 v2——主 [end] 后仅 stage-jump 可达）
GOLDEN_NVL=1   build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua   # NVL 累积/翻页/存档持久化/退出
GOLDEN_VOICE=1 build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua   # voice backlog/存档序列化/真派发

# 完整门禁（静态契约 + headless 全跑 + 分支可达 + feature 覆盖 + v1 旗标 + v2/v3 语义旗标）
bash scripts/verify_golden_vn.sh        # 实测 32/32 PASS

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

### v2 增量（2026-08-29，语义断言——从 source-face 升级为真跑）
| Feature | 增量 | headless 旗标 |
|---|---|---|
| rollback 语义 | story.ks Section E（`*rollback_check`，主 [end] 之后、仅 GOLDEN_RB stage jump 可达）：f.rb=1 → 2（两行点击各推快照 {1}/{2}）；驱动在 [wait] 暂停点连续两次 `kag_runner.rollback()`——#1 弹出 {2}（令牌回卷 + f.rb 仍 2 的机械证据），#2 弹出 {1}——f.rb==1 只可能来自 snapshot.restore（正向路径在 line one 之后不再写 1；驱动在两次 pop 之间不推进，无新快照） | RB_FORWARD rb=2 observedB=true / RB_POP1 ok=true rb=2 rewind=true / RB_POP2 ok=true rb=1 / RB_POP2_A / RB_REPLAY_END |
| history 语义 | story.ks Section F（`*history_check`）：两条 [ch] 后真实 [history] 打开 backlog 遮罩——headless 下由驱动在 input_focus=="history" 时验 backlog 内容并在下一帧送 Esc 关闭（无死等）；关闭后故事继续跑过该块到 [end] | HISTORY_OPEN backlog=2 / BACKLOG_ENTRY1 … / BACKLOG_OK / HISTORY_OK |
| backlog 内容 | 同 Section F：遮罩打开时校验 ctx.backlog 条目数 ≥2 且每条 text/name/scene/token_index 类型与取值合法（真实玩家可见数据的 API 口径） | 同上 |
| 真实存档 roundtrip | tests/scripts/golden_rt.ks（新场景，见下）：f.rtMarker=PRE_SAVE/1 → [save slot=9] → POST_SAVE_MUTATED/2 → 就绪；驱动经 `SaveCommands.load(ctx,{slot=9})`（[load] tag 的同一 handler）发起装载并**同步断言**恢复（f.rtMarker 回 PRE_SAVE、f.rtCounter 回 1——无正向路径再写 PRE_SAVE）；随后驱动 resume-from-save 重放到 [end] | RT_FORWARD marker=POST_SAVE_MUTATED counter=2 / ROUNDTRIP_OK / RT_REPLAY_END |
| 门禁 | verify_golden_vn.sh Step 1b + Step 4c：既有 26 项全部保留；新增 1（roundtrip 场景 ks_check）+ 3（rollback/history/roundtrip 语义组） | 30/30 |

> **roundtrip 场景为什么在 tests/scripts/ 而非 tests/projects/golden_vn/**：原生 [load] 的
> resume 用 `flow.load_scene` 直接按存档里的路径重载场景，而
> `SaveCommands._safeScenePath`（scripts/kag/commands/save.lua:30）只放行
> scripts/ | assets/script/ | assets/scripts/ | demo/ | tests/scripts/（+.ks、无 `..`）——
> **tests/projects/ 不在放行列表**。golden_rt.ks 因此落在 tests/scripts/（仓库既有测试场景
> 惯例，如 save_test.ks / integration_test.ks），使 save→load→resume 全链真实执行。
>
> **roundtrip 为什么由驱动发 load 而非 story 内 [load] tag（引擎缺陷，已报告不修）**：同场景
> `[save]→[load]` tag 会回到存档 token 位置重放，重放到 [load] token 再次设置 pending——
> 原生 runner 无自引用守卫（cursor+1 守卫只存在于 web/bridge.js:650 与 1024），形成循环。驱动改走
> 同一 handler（SaveCommands.load），恢复语义等价、断言同步（重放前）。修复归属 save.lua/
> kag_runner 的引擎侧（本轮约束：不碰 scripts/kag/**）。


## 覆盖与仍未覆盖（诚实清单，禁止声称覆盖）

| Feature | 状态 | 原因 / 现有专项覆盖 |
|---|---|---|
| NVL 模式语义 | **已覆盖（v3，2026-08-30）**：story.ks Section G `*nvl_check` 两 [ch] 无 [p] 累积于同一页（page_src/draws/光标推进 + TextScene.commit 封页）+ [nvl clear] 翻页 + [save slot=7] nvl_mode 持久化 + [nvl off] 退出 | GOLDEN_NVL=1 → NVL_ACCUM_OK / NVL_PAGE_OK / NVL_SAVE_OK / NVL_OFF_OK（4 断言） |
| 语音（voice）语义 | **已覆盖（v3，2026-08-30）**：story.ks Section H `*voice_check` [ch voice=] backlog voice 字段 + save backlog[].voice 序列化 + [playvoice storage=] 真派发（mock is_voice_playing=false 不阻塞） | GOLDEN_VOICE=1 → VOICE_BL_OK / VOICE_SAVE_OK / VOICE_DISPATCH_OK（3 断言） |
| Live2D | **未覆盖** | SDK 依赖，v3+ |
| Steam | **未覆盖** | SDK 依赖，Phase2 |
| replay 实跑 / mod 实跑 | 未覆盖（story 注释保留 source 面） | replay.lua / mod 单测专项 |
| 存档**真机**读写（引擎 SaveManager 磁盘语义） | 未覆盖 | v2 覆盖的是 save.lua 捕获→恢复→resume 全链（存储层由驱动内存 mock）；真磁盘 roundtrip 属引擎/真机验证域 |
| history 遮罩**渲染**（backend 绘制）与跳转（Enter→_pendingJump） | 未覆盖 | v2 断言遮罩打开/关闭与 backlog 数据；像素渲染与 Enter 跳转属真机/浏览器域 |

**v2 已覆盖（从 v1 source-face 升级为语义断言）**：rollback（快照 pop + f.* 恢复 + 令牌回卷 + 重放）、history（遮罩开/关 + 后续继续）、backlog（条数与 text/name/scene/token_index 内容）、真实 save→load（捕获→恢复→resume 重放全链，驱动层断言）。

**v2 驱动层两个诚实边界**：
1. **roundtrip 的存储层是驱动内存 mock**（KAG.save_game/load_game 在内存保存序列化状态表）——原生的捕获（capture_state）、恢复（SaveCommands.load）、resume（resume_from_save）全部真实执行；只有"写磁盘/加密"由引擎 SaveManager 负责（真机域）。
2. **history 遮罩由驱动送 Esc 关闭**——真实键语义（HistoryUI.show 的 key_consumed("_GAME_KEY_ESC")）；渲染像素不动（无 GPU）。

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

- 结构基线同 `tests/projects/first_vn/`（story.ks + entry.lua + 同款 [select] 延迟
  pending-jump 行为——first_vn 的 ROUTE sun/rain 验证模式与本项目一致）；**差异在于资产来源**：
  first_vn 自带本地 assets/ 目录（tests/projects/first_vn/assets/），而 golden_vn **无本地
  assets/**，直接引用仓库根共享资产池（assets/bg|fg|bgm|se|voice）。
- `package_game.sh tests/projects/golden_vn` 可直接打包为 Web 站（多 .ks 目录会被收集并逐档
  ks_check；web 端 bundle 化后跨场景由 story bundle 内 scene 键按名解析）。
- 门禁链：`verify_golden_vn.sh`（4 步 + 4b v1 旗标 + 4c v2 语义旗标，实测 30/30 PASS）→ 本地/CI 全量门禁。
