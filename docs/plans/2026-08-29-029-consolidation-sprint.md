# 2026-08-29-029 · Consolidation Sprint（能力闭环冲刺）计划

> 依据：外部技术审查（2026-08-29，基准 HEAD `142d0dbe`）×《产品化推进总任务书》对照。
> 审查结论：Runtime / 渲染 / 打包已达第一梯队；最大缺口不是功能，而是**闭环证据**——
> 真实平台验证、能力级闭环、真实作品。本计划把审查建议落成可执行工件与任务拆解。
> 状态：已放行执行中（2026-08-29 用户放行；T-A1..T-E1 五路已派发并落地，复核/修复/接线批次进行中——t103 复核、t105 letter_spacing 位置级接线已入库 33fb6b19、t106/t107 修复复核链在跑）。

## 一、方向调整（三条新纪律，立即生效）

1. **Feature 冻结**：Capability Closure Matrix 首版完成前，不新增 KAG command / 新 feature
   （P3 默认拒绝；P0-P2 缺陷修复、既有能力接线、文档不受限）。
2. **平台表述七级阶梯化**：一切「平台支持」以
   `Support → Build → Boot → Gameplay → First-VN → Package → Store` 分级表述，
   每格注判据出处（CI job / 真机记录 / verify 断言），无证据 = `?`。
3. **CPU baseline 显式化**：Linux x86_64 发行注明 `x86_64 + SSE4.1`
   （与 Windows MSVC 事实基线一致，`142d0dbe`）；Linux GL 运行时依赖 `libEGL.so.1`（`0fce3311`）。

## 二、核心工件

### 工件 A：Capability Closure Matrix（能力闭环矩阵）
- 位置 `docs/design/capability-closure-matrix.md`，由 `scripts/capability_closure.py` 生成（可再生，非一次性人工表）。
- 分级：Declared → Parsed → Dispatched → Consumed → Observable → Tested → Platform Tested → Packaged。
- 状态：VERIFIED / PARTIAL / UNWIRED；首批范围 = 134 KAG contracts + input 手势链。
- 自动判级为启发式并注明局限：Declared=contract 在册 / Dispatched=handler 注册 /
  Consumed=handler 触达效果面（backend.* / layers.* / kag.* / ctx.tf）/ Tested=测试引用；
  Observable / Platform / Packaged 三列首版 manual 占位（诚实分级）。
- 产出：UNWIRED / PARTIAL 清单 → 逐批接线，或显式标注 experimental。

### 工件 B：平台支持矩阵（七级阶梯）
- 位置 `docs/design/platform-support-matrix.md`（新建；吸收 Sprint6-L2 原「platform-matrix 刷新」项）。
- 当前诚实基线：Linux Boot 及以上层级 = round-7 判读中；mac 窗口化 Metal = 取证中（.ips + demo 探针，`bca67d42`）；
  iOS = hardware-gated；Store 全平台 = `?`（Steam 发布管线属 Sprint7）。

### 工件 C：Production Showcase 立项规格（先立项，不开工）
- 规格：15-30min / 2-3 结局 / Live2D / CJK / i18n / 存档 / 音频 / 转场 / VFX。
- 发布目标：Windows + Web（第一公开展示平台）+ Android。
- 前置：工件 A 首版 + Sprint6-L2 收官；规格评审通过后才排制作。

## 三、立即工程线（顺序不变，正在进行）

1. `libegl1` CI 修复（`0fce3311` 已推送）→ round-7 → **Linux 真渲染首证**（verify §5 `renderdisabled=0`）。
2. mac §3 取证判读（.ips + demo 探针已埋，`bca67d42`）。
3. Sprint6-L2 收官三连：ci.yml mac verify 硬门化 / 平台矩阵（并入工件 B）/ RC 再签发
   （顺带治愈本机 RC 基线红：Lua 孤儿套件 24→26）。

## 四、任务拆解（已备好，待放行后派发）

| 任务 | 成员 | 内容 | 边界 |
|---|---|---|---|
| T-A1 | template-path-2 | `scripts/capability_closure.py` 扫描器 v1 + 矩阵首版生成 | 新脚本+新文档；不碰引擎/测试注册文件；不 commit |
| T-B1 | release-verify-2 | `platform-support-matrix.md` 七级表（判据出处全注） | 仅新文档；Linux 行留 round-7 占位；不 commit |
| T-C1 | browser-e2e-2 | CPU baseline + libEGL 运行时依赖写入 getting-started 等用户文档 | 仅文档；表述用「需要」非「已验证」；不 commit |
| T-D1 | ctest-wiring-2 | 审查例子核真：[typewriter] / letter_spacing / SwipeDown + 手势族全链判级 | 只读审计，file:line 证据链；不改文件 |

（复核任务在各产出落地后按惯例接线；reviewer-2 留守待复核。）

## 五、里程碑重排

| # | 里程碑 | 判据 |
|---|---|---|
| M1 | Linux 真渲染绿证 | round-7 Linux Package §5 renderdisabled=0 |
| M2 | Sprint6-L2 收官 + RC 再签发 | mac verify 硬门 + 新 RC bundle 本机 ctest 全绿 |
| M3 | Closure Matrix 首版 + UNWIRED 首批接线 | 矩阵入库 + 清单处置计划 |
| M4 | 平台矩阵七级化 + baseline 文档 | 工件 B 入库 + getting-started 更新 |
| M5 | Showcase 规格评审通过 | 工件 C 评审记录 |

> 用户侧硬件/运营项（Apple 真机、3-10 名第三方作者、公开发布）保持 honest gating，不计入本 sprint 判据。

## 六、审查细节核真纪律

审查点名的 `[typewriter]` / `letter_spacing` / `SwipeDown` 闭环状态为示意性举例、未经本仓实查——
T-D1 核真后才可进矩阵；矩阵中任何格子的状态都必须可追溯到 file:line 或测试 / CI 证据。
