# Caesura (AmeKAG) — Steam 发布链指南

把引擎现有的 Steamworks 抽象（`src/steam/`，条件编译）变成开发者能实际执行的 Steam 发布
（AppID → Depot → steamcmd 上传 → 分支验证）。标注约定：

- **✅ 已验证**：仓库内有证据（源码确认 / CI 门禁 / 文档同步），无账号环境可复现。
- **⏳ 待账号/设备**：需要 Steamworks 合作伙伴账号、付费 AppID 或真机 Steam 环境，**从未执行**
  （对应产品化任务书 §18.4：不假装完成）。

现状基线：引擎抽象与 Lua 触发面就绪且经 no-SDK 门禁验证；**带 SDK 链接、上传与运行时验证全部未做**。
## 1. 前置条件 ⏳

| 项目 | 说明 |
|------|------|
| Steamworks 合作伙伴账号 | partner.steamgames.com 注册（需税务/银行信息）；每款 App 缴纳 Steam Direct 费用（当前 $100） |
| App ID | 后台付费创建后获得；本文记作 `<APPID>`，Depot ID 记作 `<DEPOTID>` |
| steamcmd | Valve 官方上传 CLI（developer.valvesoftware.com 的 SteamCMD 页下载），解压即用 |
| Steamworks SDK 1.60+ | 引擎硬性要求见 `src/steam/SteamBackend.h`；与 Live2D 同模式**不入库**，放 `thirdparty/`（已 gitignore）或任意本地路径 |
| 后台内容配置 | 成就/统计项须在伙伴站逐条录入；**成就 ID 字符串必须与 Lua 调用完全一致**，否则静默失败 |

## 2. 引擎侧准备 ✅（代码已具备，无需改动）

### 2.1 带 SDK 构建

```bash
# STEAM_INCLUDE_DIR / STEAM_LIBRARY 是纯缓存变量，CMake 没有查找逻辑，必须显式传入：
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 \
  -DCAESURA_HAS_STEAM=ON \
  -DSTEAM_INCLUDE_DIR="thirdparty/steamworks_sdk_1.60/public" \
  -DSTEAM_LIBRARY="thirdparty/steamworks_sdk_1.60/redistributable_bin/win64/steam_api64.lib"
cmake --build build --config Release --parallel
```

- ✅ 开关接线正确：`CMakeLists.txt:167` 定义选项；ON 时给 `CaesuraSteam`/`CaesuraEntry` 加宏并应用两个变量（249-254 行）。
- ✅ no-SDK 默认构建安全：`SteamBackend.cpp` 始终编译，Steamworks 符号锁死在 `#ifdef CAESURA_HAS_STEAM` 内，
  `tests/cpp/test_steam.cpp` 静态断言防泄漏（CI 全绿）。⏳ 带 SDK 的完整链接本机从未跑过（无 SDK）。
- ⏳ 发布包须随附运行时 `steam_api64.dll`（SDK `redistributable_bin/win64/` 下）。

### 2.2 运行时行为（源码确认 ✅，真实行为待账号 ⏳）

- 组合根自动装配：`EngineConfig.steam` 未注入时按宏创建 `SteamBackend`/`NullSteamBackend`；
  `init()` → `SteamAPI_Init()` + `RequestCurrentStats()`，注册进 `BackendRegistry`（`Engine.cpp:168/466-467`）。
- `processEvents()` 每帧调 `runCallbacks()`（内含 StoreStats ≤1 次/秒节流）；overlay 激活时暂停输入（`Engine.cpp:894/909`）。
- 成就在 `UserStatsReceived_t` 到达前调用返回 false——引擎不排队，脚本侧需延后或重试触发。
- **Lua 绑定无条件注册**：无 SDK 时每个调用返回安全默认值（false/0/nil），游戏照常运行；
  同一份脚本在 Steam 版与非 Steam 版通用。

### 2.3 开发期 steam_appid.txt ⏳（标准 Steamworks 流程；仓库 0 处引用，grep 已核实）

开发调试期在**可执行文件同级目录**放 `steam_appid.txt`，内容一行 AppID（如 `480` = Spacewar 测试 AppID）。
该文件不应提交进仓库（真实 AppID 属商业信息，建议进 `.gitignore`）；经 Steam 客户端启动的发布版**不需要**它；
裸双击 exe 且无此文件时 `SteamAPI_Init()` 失败 → 优雅降级为 Null 行为，游戏仍能玩。

### 2.4 成就/统计/云存档的 Lua/KAG 触发面 ✅

全局 `steam` 表，19 个 API，与 `docs/api/lua-modules.md` §Steam 一致：

| 类别 | API |
|------|-----|
| 成就 | `unlock_achievement(id)` · `is_achievement_unlocked(id)` · `reset_achievement(id)` · `reset_all_achievements()` |
| 统计 | `set_stat_int(n,v)` · `get_stat_int(n)` · `set_stat_float(n,v)` · `get_stat_float(n)` · `store_stats()` |
| Overlay | `is_overlay_active()` |
| 云存档 | `cloud_write(n,data)` · `cloud_read(n)` · `cloud_file_size/exists/delete(n)` · `cloud_quota_total/used()` · `cloud_list()` |

**没有专用 KAG 标签**（如 `[achievement]`）——触发写在 `[eval]`/`[iscript]` 或游戏 Lua 模块中：

```lua
-- 例：结局分支里解锁（ks 场景 [eval] 或 .lua 均可）
steam.unlock_achievement("ACH_ENDING_TRUE")
steam.set_stat_int("endings_seen", endingsSeen); steam.store_stats()
```

另注 ✅：`src/storage/CloudSaveProvider`（ISaveProvider ← ISteamBackend，64MB 分块）已存在但**未自动接线**
——`SaveManager` 云同步目前只支持 HTTP provider。现阶段云存档直接用 `steam.cloud_*`。

## 3. 包体准备

### 3.1 Windows ZIP ✅

复用 `docs/guides/release-process.md` 全流程（Release 门禁 → `cd build && cpack -C Release -G ZIP` → 解压冒烟）。
Steam 版唯一差异：配置加 `-DCAESURA_HAS_STEAM=ON`，并把 `steam_api64.dll` 放进归档根（与 exe 同级）。

### 3.2 Depot 布局建议 ⏳（结构合理但未经后台实测）

v1 建议**单一 Windows depot**（不拆 DLC/语言）：

```
steam-release/                  # 本地工作目录（不入库；真实 AppID 不进 git）
├── content/windows/            # CPack ZIP 解压内容平铺于此（exe + steam_api64.dll + scripts/demo/assets/shaders）
├── output/                     # steamcmd 日志/缓存
└── scripts/
    ├── app_build_<APPID>.vdf
    └── depot_build_<DEPOTID>.vdf
```

## 4. steamcmd 上传 ⏳（通用模板，按官方格式写，未执行过）

模板文件已就绪：`scripts/steam/app_build_template.vdf`（复制后填入 AppID）与 `scripts/steam/depot_build_windows_template.vdf`。

```
"appbuild"
{
    "appid"       "<APPID>"
    "desc"        "Caesura AmeKAG v<version>"
    "buildoutput" "output"
    "contentroot" "content"
    // 首传不要写 "setlive"：上传成功后在伙伴站手动设分支更稳
    "depots"
    {
        "<DEPOTID>" "depot_build_<DEPOTID>.vdf"
    }
}
```

`scripts/depot_build_<DEPOTID>.vdf`：

```
"DepotBuild"
{
    "DepotID" "<DEPOTID>"
    "FileMapping"
    {
        "LocalPath" "windows\\*"
        "DepotPath" "."
        "recursive" "1"
    }
    "FileExclusion" "\\*.pdb"
}
```

上传命令（在 `steam-release/` 根执行；首次会向构建账号邮箱要验证码）：
```bash
steamcmd +login <BUILD_ACCOUNT> +run_app_build_http ./scripts/app_build_<APPID>.vdf +quit
```

## 5. 发布前检查清单 ⏳（全部需账号/真机环境）

- [ ] 归档根含 `steam_api64.dll`；从 **Steam 客户端**安装启动（裸双击 exe 不产生有效会话）
- [ ] Overlay 冒烟：Shift+Tab 呼出 → 游戏输入暂停 → 关闭恢复
- [ ] 成就：触发一个 → Steam 弹窗/个人页出现；`reset_all_achievements()` 后可重复触发
- [ ] 统计：`set_stat_int` + `store_stats` → 伙伴站后台数值更新
- [ ] 云存档往返：A 机 `cloud_write` → B 机（或重装后）`cloud_list`+`cloud_read` 读回一致；quota 非 0
- [ ] beta 分支：上传至 beta 类分支 → 参与者验证通过 → 再切 public/default
- [ ] 回归保护 ✅：不带 SDK 的默认 CI 构建保持全绿（`test_steam.cpp` 已守护）

## 6. 当前状态表（任务书 §18.4 口径）

| 事项 | 状态 | 证据 |
|------|------|------|
| `src/steam/` 抽象 + Null 回落 + no-SDK 测试 | ✅ 已验证 | CI 门禁全绿；`tests/cpp/test_steam.cpp` |
| Lua 19 API 无条件注册、生命周期接线 | ✅ 已验证（源码级） | `SteamBinding.cpp`、`Engine.cpp:168/466/894/909` |
| `CAESURA_HAS_STEAM=ON` + SDK 完整链接 | ⏳ 需 Steamworks SDK | 本机无 SDK，链接未验证 |
| steam_appid.txt 开发联调 | ⏳ 需 AppID（480 可先用） | 标准 Steamworks 流程，非仓库验证 |
| depot 配置 / steamcmd 上传 / setlive | ⏳ 需合作伙伴账号 | 本文模板未执行 |
| overlay / 成就 / 统计 / 云存档真机往返 | ⏳ 需账号 + 设备 | 能力矩阵 C8 同口径标注 |
| 商店页、定价、审核提交流程 | ⏳ 需账号 | 超出本文范围 |

相关：`docs/guides/release-process.md`（ZIP 流程）· `docs/api/lua-modules.md` §Steam ·
`docs/design/engine-capability-matrix.md` C8 · 产品化任务书 Phase2「Steam」。
