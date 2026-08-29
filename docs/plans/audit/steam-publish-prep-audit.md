# Steam 发行预备审计（Sprint7 预研）

> 性质：设计预研文档（零代码改动）。目标：把"现有产物接上 Steam"的接入面、落仓方式、
> depot 映射、CI 边界与最小可发布路径在动手前审定，供 Sprint7 排期与分工使用。
> 日期：2026-08-29（分支 master）。
>
> **证据纪律**：✅ = 仓库证据（路径/行号已核实）；⚠️UNVERIFIED = 依赖 Steamworks 外部事实，
> 本会话 web_search 不可用（provider 认证失败，子会话无网络查询能力），已标注待核对项与
> 官方文档入口；⏳ 复用 `docs/guides/steam-release.md` 的"需账号/设备"标记（该文为权威
> 引擎侧现状，本文只做预研补充，不重复其 ✅ 清单）。

## 0. 现状盘点（✅ 仓库证据）

### 0.1 引擎侧：Steamworks 抽象已完整、条件编译、no-SDK 安全
| 面 | 证据 | 说明 |
|---|---|---|
| 开关 | `CMakeLists.txt:182` `option(CAESURA_HAS_STEAM ... OFF)` | 默认 OFF；仓库无任何 SDK 查找逻辑，`STEAM_INCLUDE_DIR`/`STEAM_LIBRARY` 必须由调用方显式传入（`CMakeLists.txt:311-331` 仅消费） |
| 接口 | `src/steam/api/ISteamBackend.h:16-47` | 23 个纯虚：init/shutdown/runCallbacks/overlay + 成就×4 + 统计×5 + 云存档×9 + name |
| 实现 | `src/steam/SteamBackend.cpp:1-26` | SDK 符号全部锁在 `#ifdef CAESURA_HAS_STEAM` 内；`SteamBackend.h` SDK-free（回调桥接） |
| 空实现 | `src/steam/NullSteamBackend.h:1-25` | 无 SDK 时全部安全默认值 |
| 装配 | `src/entry/Engine_Backends.cpp:34-40` | `createDefaultSteamIntegration()` 按宏返回 Steam/Null |
| 生命周期 | `src/entry/Engine.cpp:178,536-542` | init 可选、注册 `BackendRegistry`、**Lua 绑定无条件注册**（无 SDK 也安全） |
| 帧循环 | `Engine.cpp:1005-1006` runCallbacks；`1020-1021` overlay 暂停输入；`1721-1724` shutdown | 已接线 |
| Lua 面 | `src/script/bindings/SteamBinding.cpp:157-186` | 19 函数 + 1 别名（`set_achievement`→`unlock_achievement`），全局 `steam` 表 |
| no-SDK 门禁 | `tests/cpp/test_steam.cpp`（CI 全绿） | 防 SDK 泄漏的静态守卫 |
| 模板 | `scripts/steam/app_build_template.vdf`、`scripts/steam/depot_build_windows_template.vdf` | 已就绪、未执行（⏳） |
| 既有文档 | `docs/guides/steam-release.md`（160 行，✅/⏳ 双标） | 引擎侧现状权威；含 2.3 steam_appid.txt、4. steamcmd 模板 |

**结论 0.1**：规格 (a) 的"最小接入"在**引擎侧已经完成**——无 Steam 必可跑（Null 回落 + 无条件绑定）、
Steam API 仅可选初始化。Sprint7 真正缺口在 **SDK 就位 → 带 SDK 构建 → depot → CI 上传 → 账号侧验证**，
全链路目前无一处执行过（⏳）。

### 0.2 打包产物现状
| 产物 | 生成 | 验证 | Steam 就绪? |
|---|---|---|---|
| Windows `CaesuraAmeKAG-<ver>-win64.zip`（CPack ZIP；`CMakeLists.txt:544-556`，文件名 `559-562`） | CI `release` job（`.github/workflows/ci.yml:315-360`）：`cpack -C Release -G ZIP` | `docs/status/platform-matrix.yaml:58`：verify_release_package.sh 29/29（Debug+Release，sha256 b6a5b93c…） | **否**：job 配置 `ci.yml:336` 无 `-DCAESURA_HAS_STEAM` → 包内无 steam_api64.dll，SteamAPI_Init 必失败 |
| Linux TGZ `CaesuraAmeKAG-<ver>-Linux-x86_64` | CI `release-linux`（`ci.yml:362-425`）：`cpack -G TGZ` | 同脚本 29 断言（Xvfb） | 同样否 |
| Web 包（`-web` 后缀） | `scripts/package_game.sh`（276 行，纯 Web 静态站） | web vitest + smoke | **不适用**：Steam 发布桌面 depots，不含 Web 包 |

**结论 0.2**：现有 29/29 验证的 ZIP/TGZ 是"开发者工具包"，**不是** Steam depot 素材——depot 需另出一份
`CAESURA_HAS_STEAM=ON` 构建（差异极小：链接 SDK + 包根多 steam_api64.dll，可复用现有 CPack/verify 全链）。

### 0.3 布局与复用面
- 包根布局 = Depot content root 候选：exe + SDL3/FFmpeg/steam dll + `scripts/ assets/ demo/ projects/ shaders/ tools/project_templates/ web-editor/dist/ external/lua/` ＋ README/LICENSE（`CMakeLists.txt:507-542,597-600`）。
- 游戏制作者侧：`scripts/caesura_build.py:361-362,688` `runtime_libs()` 把引擎旁的**全部** dll（含 steam_api64.dll）拷入 game-only 包 → Steam 引擎二进制即产出"带 Steam dll 的游戏包"。
- `scripts/verify_release_package.sh:1-40`：stranger-path 验证（解压外目录起服务、token 门、demo 非空、29 断言、`--skip-if-missing` 77）——**可原样复用于 depot 内容预检**（新增一条断言：Steam 构建的包根含 steam_api64.dll）。
- `.gitignore` 已含 `steamworks_sdk_*.zip` 与 `external/steamworks/`（已验证）——SDK 放置约定已有两条历史路径（`thirdparty/` 在 steam-release.md:18 提及但**不在** .gitignore 中，见 g-R4）。

## a) SDK 接入面分析：最小接入 vs 深接入（含 MVP 建议）

| 层次 | 内容 | 现状 | Sprint7 建议 |
|---|---|---|---|
| 最小接入（必须） | ①无 Steam 可跑 ②可选 SteamAPI_Init ③overlay 暂停输入 ④Lua 安全降级 | ✅ 已完成（0.1 全表） | 保持；**唯一新增 = SDK 就位 + `-DCAESURA_HAS_STEAM=ON` 构建链** |
| 成就（深） | `steam.unlock_achievement(id)`/`is_achievement_unlocked` + 伙伴站配置成就 ID | ✅ API 完成；⚠️ 成就 ID 仓库零常量（Lua 传裸字符串），需伙伴站逐条录入且逐字一致 | MVP 做：2-3 枚自验成就（`ACH_*` 命名 + 登记表） |
| 统计（深） | set/get stat + store_stats（已带 ≤1 次/秒节流） | ✅ API 完成 | 可延后（无 UI 消费方）；MVP 仅验证往返 |
| 云存档（深） | `steam.cloud_*` ✓；`CloudSaveProvider`（ISaveProvider ← ISteamBackend，64MB 分块）**已存在但未接线**（`storage/CloudSaveProvider.cpp`；`SaveManager` 仅 HTTP provider，见 steam-release.md:73-74） | ⚠️ 未接线 | **延后**：与存档格式耦合，属旗舰作品期；MVP 用 cloud_* 直调验证 |
| 回调时序 | 成就/统计须在 `UserStatsReceived_t` 之后触发（引擎不排队） | steam-release.md:44 | 可加 `steam.ready` 标志，脚本轮询（Sprint7 可选） |

**MVP 建议（结论）**：仅做"带 SDK 构建链 + depot + 最小账号验证"，功能面 = overlay 冒烟 + 2 枚成就 + 1 个 stat 往返。
云存档/成就系统化/统计 UI 全部移出 Sprint7 核心。

## b) SDK 落仓方案（vendoring 与许可 ⚠️UNVERIFIED）

- **不允许公开再分发**（Steamworks SDK 用户协议经典条款；⚠️ 未现场核对，入口：partner.steamgames.com 的 SDK License 文档）。因此：
  - ❌ 不把 SDK 提交进 git（含 gitignored 目录留档——SDK zip 禁止摆进公开仓库历史）。
  - ✅ 现状已是正确形态：`.gitignore` 已有 `steamworks_sdk_*.zip`/`external/steamworks/`；全仓库无 SDK 文物（核实：`STEAM_INCLUDE_DIR` 仅 CMakeLists 3 处消费，无 set）。
- **提议（与 external/ 现有布局一致）**：
  ```
  external/steamworks/            # gitignored；解压后的 SDK
  └── public/                     # -DSTEAM_INCLUDE_DIR 指向（含 steam/steam_api.h）
  └── redistributable_bin/win64/  # steam_api64.lib/.dll（-DSTEAM_LIBRARY）
  ```
  获取方式（推荐序）：
  1. 开发者本机手动下载 SDK zip 放 `steamworks_sdk_<ver>.zip`（gitignore 已覆盖），解压到 `external/steamworks/`；
  2. CI 侧（见 e）：SDK zip 作为**私有位置资产**（公开仓库的 Release 资产也可见——需放私有仓库/私有托管，或经 secrets 下载），runner 上临时解压 `external/steamworks/`（临时目录，工作区可清）。
- **版本策略**：SDK 版本钉死（Steamworks 1.60+，`src/steam/SteamBackend.h` 依赖），`<ver>` 记录到 `scripts/steam/` 常量化；升级 SDK 属独立 PR（ABI 兼容性 ⚠️ 需实测）。

## c) steam_appid.txt：开发期处理与打包期剔除

- 仓库现状：**0 处引用**（scripts/src/docs/tests 全 grep 核实）。开发期行为：非 Steam 启动下 `SteamAPI_Init()` 读取 exe 同目录/工作目录的 `steam_appid.txt`（⚠️UNVERIFIED 经典行为，入口：partner.steamgames.com SDK 文档）。
- **开发期**：开发目录放 `steam_appid.txt`（一行 AppID；`480`=Spacewar 测试 AppID 可先行）；仓库 `.gitignore` 增加 `steam_appid.txt`（**建议行**——`.gitignore` 为 captain 独占，批准后加）。
- **打包期剔除策略**：depot 组装仅含 CPack 输出 + steam_api64.dll；`depot_build_*.vdf` 的 `FileExclusion` 显式列 `steam_appid.txt`；verify_release_package.sh 加"包根无 steam_appid.txt"断言（复用现有框架）。

## d) depot 结构：现有产物 → Steam depot 映射 + VDF 骨架

**映射**（v1 建议：单 Windows depot + 单 Linux depot，不拆语言/DLC）：

| Steam depot content root 内 | 来源 |
|---|---|
| `CaesuraAmeKAG.exe` / `steam_api64.dll` / SDL3.dll / av*.dll… | CPack ZIP 根（Steam 构建） |
| `scripts/ assets/ demo/ projects/ shaders/` | 同上 |
| `tools/project_templates/ web-editor/dist/ external/lua/` | 同上 |
| `README.md LICENSE` | 同上 |
| ~~`steam_appid.txt`~~ | 排除（见 c） |

**VDF 骨架**（`scripts/steam/` 已有同款模板——以其为准；此处给出完整语义版）：

`app_build_<APPID>.vdf`：
```
"AppBuild"
{
    "AppID"       "<APPID>"
    "Desc"        "Caesura AmeKAG <version> CI upload"
    "BuildOutput" "output"
    "ContentRoot" "content"
    "SetLive"     ""              // 留空：上传≠发布；分支切换走伙伴站或 VDF branch 字段
    "Depots"
    {
        "<DEPOTID_WIN>" "depot_build_windows_<DEPOTID_WIN>.vdf"
    }
}
```
`depot_build_windows_<DEPOTID>.vdf`：
```
"DepotBuild"
{
    "DepotID"   "<DEPOTID>"
    "FileMapping"
    {
        "LocalPath" "windows/*"
        "DepotPath" "."
        "recursive" "1"
    }
    "FileExclusion" "*.pdb"
    "FileExclusion" "steam_appid.txt"
}
```
（⚠️UNVERIFIED：VDF 精确 schema 以 SteamPipe 官方文档为准，入口 https://partner.steamgames.com/doc/sdk/uploading；仓库模板为多年稳定形态。）

## e) CI 集成：steamcmd 凭据边界与"可上传但不自动发布"

当前 `ci.yml` 对 steam 零感知（核实：`CAESURA_HAS_STEAM`/`steamcmd` 均 0 处）。提案：

| 项 | 方案 | 边界 |
|---|---|---|
| 触发 | 新 job `steam-upload`（`workflow_dispatch` + master 推送双触发；缺 secrets 时 skip） | **不是**默认门禁；release job 产物不变（非 Steam 包照常出） |
| SDK 获取 | 私有位置下载 zip → 临时解压 `external/steamworks/` | 凭据经 secrets；不留公开痕迹 |
| 构建 | `-DCAESURA_HAS_STEAM=ON -DSTEAM_INCLUDE_DIR=... -DSTEAM_LIBRARY=...` → CPack ZIP → 解压为 content 树 | 与 0.2 普通 release 完全隔离 |
| 登录 | `secrets.STEAM_CMD_LOGIN`（`用户名 密码`）传 `steamcmd +login @srv …`；用专用构建账号 | 密码日志脱敏（禁 set -x 泄露 / 打印字段过滤） |
| 上传 | `steamcmd +run_app_build_http <vdf> +quit` | 上传 **beta/测试分支**，**永不写 SetLive/默认分支** |
| 自动发布边界 | **CI 只生产可上传产物；发布动作（分支 set default / 商店页 submit）100% 人工**，在伙伴站执行并记录 | 任务书里程碑③的"发行"必须过人工审查 |
| 可上传产物缓存 | `actions/upload-artifact` 保留 `steam-content/`（VDF + 包） | 人工本地 steamcmd 上传双路径皆可 |

（⚠️UNVERIFIED：steamcmd beta branches 参数形态与 GH Actions 可用集成，参考 https://partner.steamgames.com/doc/sdk/uploading；实现前人工核对。）

## f) 通往 Playtest/Demo 页面的最小路径 checklist（步骤级；⏳ 全部需账号）

> 说明：按"引擎工具类 App + 内置 Demo VN"假设（见 g-R9）；Demo 用同一 AppID 的 beta 分支承载，零额外费用。

1. Steamworks 合作伙伴账号注册 + Steam Direct 费用（每 App $100 ⚠️UNVERIFIED 现行费率，入口 partner.steamgames.com/steamdirect）；创建 App 获 `<APPID>`，配置并记下 `<DEPOTID>`。
2. 后台录入成就/统计项（成就 ID 与 Lua 字符串逐字一致——见 a）。
3. 本机构建 `-DCAESURA_HAS_STEAM=ON` 版 → 按 steam-release.md §4 手动上传（模板已备）。
4. 创建 beta 分支（伙伴站 SteamPipe 页）+ 配置分支可访问（key 组/公开申请——Demo 可公开）。
5. 商店页素材：capsule（616×353 / 374×448）+ ≤10 截图 + 简介 + 系统需求（⏳ 按现行后台表单核对）。
6. `steam_appid.txt` 开发联调（`480` 起步）；发布包**不含**（见 c）。
7. Overlay/成就/统计真机往返冒烟（steam-release.md §5 清单）。
8. 上传构建到 `playtest` 分支 → **Steam 客户端**安装测试（裸双击 exe 无有效会话——必须经客户端启动）。
9. （可选官方 Playtest 功能）伙伴站创建 Playtest 页 → 设置门槛 → 邀请/公开链接；本质 = 同 depots + 独立分支。
10. 验证通过：分支 SetLive/设 default（人工，记录操作）→ 商店页提交审核 → 首次发布（时效数天级 ⚠️UNVERIFIED）。

## g) 风险与未决问题清单

| # | 风险/未决 | 等级 | 处置建议 |
|---|---|---|---|
| R1 | SDK 许可禁止公开再分发；任何 git 留档（含 gitignore 目录）都有风险 | 高 | 维持 .gitignore 方案；SDK 只走私有管道；CI 日志脱敏 |
| R2 | 现有 CI 产物（29/29 的 win64 zip/TGZ）**不是** Steam 包；误当 depot 内容上传则 SteamAPI_Init 全灭 | 高 | depot job 独立构建 + verify 加 steam_api64.dll 断言；文档明示两套包不可互换 |
| R3 | `CAESURA_HAS_STEAM` 是 `CaesuraSteam PUBLIC` 编译定义（`CMakeLists.txt:317`）——ABI 敏感，Steam/非 Steam 二进制不可混用混链 | 中 | 双构建矩阵登记；游戏包侧用哪个引擎二进制就随附哪套 dll |
| R4 | SDK 放置路径三套表述并存：steam-release.md 说 `thirdparty/`；.gitignore 只认 `external/steamworks/` + `steamworks_sdk_*.zip` | 中 | 以 .gitignore 为准统一 `external/steamworks/`，并同步 steam-release.md 表述（文档改动属后续 task） |
| R5 | 成就 ID 无仓库登记表；伙伴站与 Lua 字符串漂移 = 静默失败 | 中 | 新增 `docs/design/steam-achievements.md` 登记表（Sprint7 可选） |
| R6 | 云存档未接线（CloudSaveProvider 存在但 SaveManager 只走 HTTP） | 中 | 显式决策：Sprint7 不接线，cloud_* 直调过渡 |
| R7 | steamcmd 在 CI 的不稳定性（验证码/2FA/网络） | 中 | 专用构建账号；workflow_dispatch 手动重试；上传失败不算门禁红（记录+人工接管） |
| R8 | 回调时序（UserStatsReceived 前触发失败） | 低 | 引擎 `steam.ready` 标志（可选）；demo 脚本重试模式 |
| R9 | 未决产品问题：引擎作工具上架 vs 官方 Demo VN 名义上架；创作者游戏与引擎 AppID 的关系 | 待定 | Sprint7 开工决议；本文按"引擎工具 App + 内置 Demo VN"假设 |
| R10 | 外部事实未现场核对（web_search 本会话不可用）：SDK 许可全文、$100 费率、VDF schema、steamcmd 分支参数、Playtest 表单 | 高（动手前） | Sprint7 任何步骤前用可联网会话核对 partner.steamgames.com 文档 |

## 附：本文与既有文档的关系

- `docs/guides/steam-release.md`：引擎侧现状 + 人类操作步骤（权威 ✅/⏳）——本审计是其**自动化/预研补充**。
- `scripts/steam/*.vdf` 模板：本审计未改动，Sprint7 在其上做参数化脚本。
- 任务书 Phase2「Steam」与 §18.4"不假装完成"约束贯穿全文；所有 ⚠️UNVERIFIED 项在 Sprint7 排期里都应有"联网核对"前置步骤。
- **文末附录**：Steamworks 外部事实核对（2026-08-29 快照，t77 落仓版）——关闭 R10；落地前登录 partner 后台复核 appfee 页（约 10 分钟）。

---

## 附录：Steamworks 外部事实核对（2026-08-29 快照）

> 来源：t77 联网核对（web_search 6 批次，官方 partner.steamgames.com + 近期第三方交叉）。
> 快照日期：2026-08-29。**落地前请登录 partner 后台并复核 appfee 页（约 10 分钟）**——费用/审核类条款随时间修订，以登录所见为准。
> 本附录关闭审计 R10（外部事实未核对项）；置信度标注：高=官方文档 + 多方交叉；中=官方确认存在但细节以登录后台为准；UNVERIFIED=未检索到官方确定表述。

### ① SDK 二进制再分发许可
| 事实 | 来源 URL | 置信度 | 备注 |
|---|---|---|---|
| SDK 二进制（steam_api64.dll / libsteam_api.so/dylib）允许作为**你的应用的一部分**随游戏包再分发；无按份授权费；禁止=以独立 SDK 包再分发或公开 SDK 源码 | partner.steamgames.com/documentation/sdk_access_agreement（官方全文，含 schinese 页） | 高 | 与生态实践交叉（castle-engine.io/steam、kb.heathen.group 上传页均按 DLL 随游戏分发叙述）。CAESURA_HAS_STEAM 的 steam_api64.dll 进 Release ZIP/TGZ/CI artifact 合规；SDK 本体永不入 git 纪律不变（R1 不变） |
| 协议要求按要求展示 Valve 标识/许可说明（署名类条款）；是否强制随包附 LICENSE 文本未检索到明确要求 | 同上官方页 | 中高（附 LICENSE 为保险项） | 建议发行时附 SDK 许可说明（三方包或附 LICENSE 文本）；措辞以登录版协议为准 |

### ② Steam Direct（$100 / $1000 可退）
| 事实 | 来源 URL | 置信度 | 备注 |
|---|---|---|---|
| 每 App 一次性 **$100** 入驻费 | partner.steamgames.com/doc/gettingstarted/appfee + datahumble.com/blog/steam-direct-fee-requirements-roi-2026-guide + thegamemarketer.com/insight-posts/how-to-publish-your-game-on-steam-guide（2026）+ gamemakerblog.com（2023） | 高 | 4 处交叉；数值以官方 appfee 页为准 |
| 累计收入达到 **$1,000**（美元等价）后可退/抵扣该费用 | 同上 | 高 | 个别二手表述有 gross/net 差异；登录后台核对当期条款 |
| 审核周期：官网未承诺固定天数（第三方称通常约 2 周内） | 无官方页 | **UNVERIFIED** | 以申请时后台进度为准；替代求证=partner 后台 Help/工单、Steamworks 社区论坛 |
| 所需资料（类别级）：税务表（W-8/W-9 或所在国等效）、银行账户、发布者实体信息 | 官方 gettingstarted 文档链 + 2026 三方指南 | 中高 | 具体表单以登录后台清单为准 |

### ③ steamcmd 上传链路与无头 CI Guard 方案
| 事实 | 来源 URL | 置信度 | 备注 |
|---|---|---|---|
| app_build.vdf（AppID/Desc/BuildOutput/ContentRoot/Depots{DepotID→depot_build.vdf}）+ depot_build.vdf（FileMapping/FileExclusion）+ `steamcmd +login <user> +run_app_build ... +quit` | partner.steamgames.com/doc/sdk/uploading | 高 | 官方权威；本仓 scripts/steam/*.vdf 模板结构与其一致（⏳未执行） |
| 登录=账号+密码+Steam Guard；无头 CI 无 Valve 专用文档——社区实践=专用构建账号 + CI secrets 持凭据；方案 A=交互登录后缓存 config.vdf（含 SteamGuard token 字段），方案 B=每次手机/邮箱码（TOTP 自动化属灰色，官方无接口）；`+set_steam_guard_code <code>` 可为本次登录供码 | 官方 uploading（仅陈述 Guard 存在）+ kb.heathen.group + Miziziziz/Steam-And-Itch-Command-Line-Tools-Guide（社区实践） | 中（需实测） | Sprint7 核心决策：推荐方案 A（专用账号+config.vdf 缓存+定期人工码）；凭据严禁入仓库 |
| 三平台 depot：无强制 schema；惯例=每平台一 depot（win64/linux64/macos），Linux 用 steam-launcher.sh，macOS 可裸二进制（本引擎现状） | 官方 uploading（建议命名清晰）+ 社区惯例 | 中（惯例性） | Linux steam-launcher.sh 合规性以官方 wiki 专页为准（⏳提交前查一次） |

### ④ 成就/统计/云存档：后台定义 vs API 触发分工
| 事实 | 来源 URL | 置信度 | 备注 |
|---|---|---|---|
| 定义面=partner 后台 App Admin（Achievements/Stats/Cloud 页）：成就名/图标/描述、统计键名与范围、云档 enable；触发面=ISteamUserStats（SetAchievement/GetStat/StoreStats/RequestCurrentStats）+ ISteamRemoteStorage（写云档，自动云同步需后台启用） | partner.steamgames.com/doc/api/ISteamUserStats + gdevelop wiki publish-to-steam + castle-engine.io/steam（交叉） | 高 | 引擎 bindings 已就绪；Sprint7 只需后台同名校验（R5 登记表仍建议）；防作弊服务器校验=可选 P2 |

### ⑤ default/beta 分支发布机制
| 事实 | 来源 URL | 置信度 | 备注 |
|---|---|---|---|
| 构建上传到分支；default=玩家所见；beta/preview（password 或不可见）用于验证；往 default 发布需商店审核+构建验证 | partner.steamgames.com/doc/store/application/builds + gamedev.stackexchange.com/questions/186506（beta 上传实践）+ Miziziziz 指南 | 高 | 机制官方明确；首发走 beta→default 路径建议 |
| SetLive/可见性改动的权限门禁细节（独立 vs 团队、构建默认 public 是否需验证）：官方 builds 页确认机制存在，门禁细节未检索到确定表述 | 同上官方页 | **UNVERIFIED（细节）** | 以登录后台为准；替代求证=partner 后台 + 社区 |

---
（附录完。t77 清单全量见任务 t77 output；本节为落仓版。其余正文未改动。）
