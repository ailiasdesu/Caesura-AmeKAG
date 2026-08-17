# 示例游戏发布就绪度检查与发布指南（Sample Game Release）

> 目标作品：`demo/example_game/story.ks` —「The Last Letter / 《单程回信》」示例游戏（2 场景：
> `story.ks` + `story_lastletter.ks`，三结局，中英双语，含完整 KAG Neo-Genesis 特性演示）。
>
> 本文档评估两条发布路径（GitHub Releases / itch.io），给出**逐个可跑**的发布前置检查清单，
> 并记录 2026-08-16 实测结果。**本文不执行实际发布**（发布需要用户账号操作，见 §4）。
>
> 参考：`docs/guides/release-process.md`（桌面 Release 全流程）、`docs/guides/packaging-ux.md`
> （Web/itch 一键打包）、ROADMAP-100 round 90「发布流程实测」（Release 构建 + CPack 首测通过）。

---

## 0. 结论速览（2026-08-16 实测）

| 项 | 结果 |
|---|---|
| Release 构建（当前源码） | ✅ 零错误 |
| Release C++ 测试 | ✅ 976/976 用例、8858/8858 断言、0 failed |
| Lua 主套件 + 孤儿套件 | ✅ EXIT=0（round 90 记录 126+19） |
| ks_check（示例游戏） | ✅ OK，零警告 |
| verify_sample_game.sh | ⚠️ 见 §2.4 —— 默认路径指向已不存在的 `story.ks.new`，需带 `SAMPLE_STORY` |
| package_game.sh（Web 站） | ✅ `dist/example_game` = 35,229 KB（~34.4 MiB），2 场景 bundle |
| CPack Release ZIP | ✅ `build/CaesuraAmeKAG-1.0.0-Windows-AMD64.zip` = 87.9 MB，386 文件 + .sha256 |
| 桌面 ZIP 冒烟（--frames 120） | ✅ 干净启动/干净退出，exit 0 |
| 耦合预算 | ✅ PASS（entry 14/14、di 13/14、script 11/14、其余 ≤4） |
| gh（GitHub CLI） | ✅ 已认证 `ailiasdesu`（keyring） |
| butler（itch.io CLI） | ❌ **未安装**（itch.io 路径的首要缺口） |

**发布就绪判定**：桌面 / Web 两条路径的**产物生成链路全部通过**；卡点不是构建，而是
① `verify_sample_game.sh` 默认路径缺陷（发布前应修，或按 §2.4 带参数跑）② itch.io 必须
`butler` 安装 + 用户账号登录。GitHub 路径零账号动作（已认证）。

---

## 1. 两条发布路径对比

### A. GitHub Releases（推荐首选，零新增账号）

- **桌面 ZIP**：`build/CaesuraAmeKAG-1.0.0-Windows-AMD64.zip`（引擎 + 全部 demo + 示例游戏），
  一个 zip 就是完整桌面发行物。`gh release create`（已认证）即可挂到 Release。
- **Web 静态站附件**：`dist/example_game/`（index.html + web-assets + 剧本 bundle + 资产），
  可直接作为 release 的第二个 zip 附件上传，或走仓库自带
  `.github/workflows/deploy-web.yml` 部署 GitHub Pages（workflow_dispatch，无需账号动作）。
- **优点**：gh 已认证、流程与 `release-process.md` 完全一致、CI 有 `release` job（tag 触发）
  可自动打包 Windows；无需额外 CLI。
- **缺点**：GitHub 不是游戏分发社区——触达不到 itch.io 的视觉小说受众；Web 版默认入口场景
  是 `galgame_demo.ks`（仓库主 demo），示例游戏需在下拉框选。

### B. itch.io（游戏分发社区，玩家触达最佳）

- **Web 版（必须）**：把 `dist/example_game/` **内容**压成 zip（顶层就是 `index.html`，
  不要多套一层目录），在 itch.io 建 HTML 项目上传。itch.io 会将其托管为网页游戏。
- **可选桌面版**：用 **butler CLI**（itch.io 官方工具）上传 Windows ZIP（本地或 CI 产物均可），
  平台标记 Win，玩家可下载独立版。
- **优点**：视觉小说/独立游戏玩家聚集地，支持 HTML 内嵌试玩，「Web 先试玩 → 桌面下载」
  是 galgame 社区标准转化路径。
- **缺点**：**butler 未安装**（§4B）；需要用户 itch.io 账号 + `butler login`；itch.io 页面上
  传是手动操作（Web zip 拖拽 / butler push 命令），无法纯脚本化完成登录令牌。

**推荐**：先走 A 完成一次 GitHub Release（产物已就绪，30 分钟内），同时把 B 的 butler 安装
列为用户侧待办；示例游戏的目标受众分发放在 B。

---

## 2. 发布前置检查清单（命令逐个可跑，2026-08-16 实测）

> 全部命令在仓库根目录（git bash）执行。勾选 = 实测通过。

### 2.1 构建（Release）
```bash
cmake -B build -DCAESURA_LIVE2D=OFF          # 已配置 Visual Studio 17 2022 多配置生成器
cmake --build build --config Release --parallel   # ✅ 实测零错误（增量）
```
产物：`build/Release/CaesuraAmeKAG.exe`、`build/tests/Release/CaesuraTests.exe`。

### 2.2 C++ 测试套件（Release 门禁）
```bash
cd build/tests/Release && ./CaesuraTests.exe
# ✅ 实测：test cases 976 | 976 passed | 0 failed | 0 skipped
#          assertions 8858 | 8858 passed | 0 failed
```

### 2.3 Lua 脚本套件（仓库根）
```bash
external/lua/lua.exe tests/scripts/run_lua_tests.lua      # ✅ EXIT=0（主套件，round 90: 126）
external/lua/lua.exe tests/scripts/run_orphan_tests.lua   # ✅（孤儿套件，round 90: 19）
```

### 2.4 ks_check + verify_sample_game（示例游戏专属）
```bash
# 静态契约（示例游戏两场景）——✅ OK: all scenes pass contract checks
external/lua/lua.exe scripts/ks_check.lua demo/example_game/story.ks demo/example_game/story_lastletter.ks

# 端到端验证 —— ✅ 实测 PASS 5/5（需显式指定 SAMPLE_STORY，见下方警告）
SAMPLE_STORY=demo/example_game/story.ks bash scripts/verify_sample_game.sh
```
> ⚠️ **已知缺陷（发布前应修）**：`scripts/verify_sample_game.sh` 里默认
> `STORY="${SAMPLE_STORY:-demo/example_game/story.ks.new}"` —— 指向已不存在的
> `story.ks.new`（round 102 定稿已改名 `story.ks`）。裸跑 `bash scripts/verify_sample_game.sh`
> 会 **5/5 全 FAIL**（cannot open file），产生**假红**。发布前把默认值改为
> `demo/example_game/story.ks`（并同步 `docs/guides/sample-game-verification.md` 的 `.new` 提法）；
> 在此之前**必须带 `SAMPLE_STORY=...` 运行**。实测带参数后：主路径 RESULT DONE
> （token=331, clicks=8080）+ 三结局（ending_zero/companion/promise）各自 RESULT DONE。

### 2.5 耦合预算与生成物新鲜度
```bash
python scripts/count_coupling.py --ci        # ✅ PASS
node web/gen-index.mjs --check               # 新鲜度守卫（CI 三平台跑；改动过 scripts/*.lua 需先 regen）
python scripts/api_stats.py                  # 生成文档（如脚本/接口变化）
```

### 2.6 Web 站打包（示例游戏）
```bash
bash scripts/package_game.sh demo/example_game        # ✅ 实测 EXIT=0（本机自动 vite build）
# 或复用已有 web/dist 加速：
bash scripts/package_game.sh --no-web-build demo/example_game
```
> ⚠️ **Windows 部署坑（实测遇到）**：若 `dist/<game>` 报 `rm: Device or resource busy`，
> 说明有残留 `python -m http.server`（或资源管理器）把该目录当 CWD 占用；先按 PID 精确
> kill 掉对应 python/bash 进程（**不要** taskkill 所有 node.exe，会杀 DSH 宿主）再重跑。

### 2.7 桌面 CPack 打包（Release）
```bash
cd build && cpack -C Release -G ZIP && cd ..
# ✅ 实测：package generated + sha256 generated
```

### 2.8 桌面 ZIP 冒烟（解压后从归档根运行）
```bash
mkdir -p /tmp/caesura-smoke && cd /tmp/caesura-smoke
unzip "$OLDPWD/build/CaesuraAmeKAG-1.0.0-Windows-AMD64.zip"
cd CaesuraAmeKAG-1.0.0-Windows-AMD64
./CaesuraAmeKAG.exe --frames 120
# ✅ 实测：Direct3D 11 + SoLoud + SDL3 启动，demo 加载，干净 shutdown，exit 0
# 期望的两条良性 WARN：HotReload scan failed: "assets/script/"（dev-only，不进 ZIP）
```

---

## 3. 产物清单

### 3.1 桌面 ZIP（`build/CaesuraAmeKAG-1.0.0-Windows-AMD64.zip`）

- **大小**：87,954,428 字节（**87.9 MB**）
- **文件数**：386 个文件
- **校验**：伴随 `.sha256`（105 字节）
- **归档根**（版本化目录 `CaesuraAmeKAG-1.0.0-Windows-AMD64/`）：

| 内容 | 说明 |
|---|---|
| `CaesuraAmeKAG.exe` | 引擎可执行（Release，3.8 MB） |
| `SDL3.dll` + `avcodec-62/avformat-62/avutil-60/swresample-6/swscale-9.dll` | 运行时（SDL3 + FFmpeg） |
| `assets/` | 共享资产池（bg/bgm/fg/fonts/lang/se/voice） |
| `demo/` | 全部 demo（含 `example_game/story.ks` 等） |
| `scripts/` | 引擎 Lua 运行时 |
| `shaders/` | bgfx shader 产物 |
| `include/` `lib/` `cmake/` | 随包依赖树（freetype/soloud/zstd，CPack 预期产物） |
| `README.md` + `LICENSE` | 授权与说明 |

> 注意：ZIP 内置的 demo 启动入口是 `demo/galgame_demo.ks`（默认场景）；示例游戏用
> `demo/example_game/entry.lua` 启动。

### 3.2 Web 静态站（`dist/example_game/`，35,229 KB ≈ 34.4 MiB）

文件树要点（完整 MANIFEST 见 `dist/example_game/MANIFEST.txt`，2 场景、6 资产）：

| 路径 | 说明 |
|---|---|
| `index.html` | 播放器壳（4.8 KB） |
| `web-assets/index-C09s9ni4.js` 等 | vite 构建的播放器运行时（158 KB + 33 B） |
| `cache/story/story.lua` | **预编译剧本 bundle**（41.7 KB，播放器零解析直接跑） |
| `demo/example_game/story.ks` + `story_lastletter.ks` | 场景源码（兜底） |
| `assets/` | 游戏资产（bg/bgm/fg/fonts/lang/se/voice；Noto 字体 16 MB、daily.wav 12 MB 是大头） |
| `scripts/` | 引擎 Lua 运行时 |
| `MANIFEST.txt` | 文件树 + 大小清单 |

> itch.io 上传：把 `dist/example_game/` 的**内容**压 zip（顶层即 index.html）。34.4 MiB 完全在
> itch.io 免费档位内；若想再小，可 `--assets` 指向游戏专用资产目录跳过共享池里未引用的大文件。

---

## 4. 账号依赖标注

### A. GitHub Releases —— 零新增账号 ✅
- `gh` 已认证：`gh auth status` → Logged in to github.com account **ailiasdesu**（keyring），
  protocol https，Token gho_…。
- `origin` = `ssh://git@ssh.github.com:443/ailiasdesu/Caesura-AmeKAG.git`（推送可用）。
- 已有 tag：`v1.0.0-alpha`（下一版 tag 建议 `v1.0.0` 或 `v1.1.0`，与 CMake `VERSION 1.0.0` 对齐）。
- **发布动作（发布者执行，勿在本会话做）**：
```bash
git tag -a v1.0.0 -m "Caesura (AmeKAG) v1.0.0"
git push origin v1.0.0
gh release create v1.0.0 build/CaesuraAmeKAG-1.0.0-Windows-AMD64.zip \
  --title "Caesura (AmeKAG) v1.0.0" --notes-file CHANGELOG.md --draft
# 如需附 Web 站：把 dist/example_game 压 zip 后追加：
# gh release upload v1.0.0 example_game-web.zip
```
- **Web 自动部署**：仓库 Actions → "Deploy Web Player (GitHub Pages)" → Run workflow
  （`game` 填 `demo/example_game`）——Pages 需先在 Settings → Pages 选 **GitHub Actions** 源。

### B. itch.io —— 需要用户动作 ⚠️
| 步骤 | 需要什么 |
|---|---|
| 1. 安装 butler | **尚未安装**（`which butler` 无结果）。`winget install itch.butler` 或
  `curl -L -o butler.zip https://broth.itch.itch.io/butler/win/zip`（选其一） |
| 2. 登录 | 用户 itch.io 账号；`butler login`（会打开浏览器/弹 token，人工确认） |
| 3. 建项目 | itch.io 网页 → Upload new project → Kind: **HTML**（用 §3.2 的 zip） |
| 4. 传桌面版（可选） | `butler push build/CaesuraAmeKAG-1.0.0-Windows-AMD64.zip <user>/<game>:windows` |

> Web 版也可**只用网页拖拽上传**（不装 butler）：itch.io 上传页直接拖 §3.2 zip 即可。
> butler 只在「桌面版 + 后续版本自动化」时必需。

---

## 5. 预估时间与步骤数

| 路径 | 步骤数 | 预估时间（发布者熟练操作） |
|---|---|---|
| A. GitHub Releases（桌面） | 5 步（tag → push → changelog → release --draft → 复核发布） | **20–30 分钟**（不含构建，构建增量约 5–15 分钟） |
| A'. GitHub Pages（Web，deploy-web.yml） | 1 步（Actions → Run workflow） | 5–10 分钟 |
| B. itch.io（Web） | 4 步（zip → 建项目 → 上传 → 页面信息） | 15–25 分钟 |
| B'. itch.io（桌面，butler） | 3 步（装 butler → login → push） | 15–30 分钟 |
| **A+B 完整双渠道** | 约 12–14 步 | **约 1–1.5 小时** |

---

## 6. 发布前待办（阻碍项汇总）

1. **[必修] `scripts/verify_sample_game.sh` 默认 `story.ks.new` → `story.ks`**（§2.4）。
   同时清理 `docs/guides/sample-game-verification.md` 与脚本内残留的 `.new` 提法。
2. **[可选] 装 butler + 用户 itch.io 登录**（B 路径前提，§4B）。
3. **[可选] README.md 有未提交改动**（本会话外产生），发布提交时一并确认归属。
4. **[可选] 示例游戏命名一致性**：`demo/example_game/README.md` 标题为 "The Last Letter"，
   `story.ks` 头注为 "The One-Way Reply"/《单程回信》——发布页文案前统一对外名称。
5. **[建议] 发布前跑一次 `python scripts/gen_changelog.py --from-tag v1.0.0-alpha --tag v1.0.0`
   --dry-run` 预审 changelog**（round 90 已实测 --dry-run 合理）。

---

## 7. 相关文档

- `docs/guides/release-process.md` — 桌面 Release + CPack + gh release 全流程（round 90 已实测修正）
- `docs/guides/packaging-ux.md` — 一键打包与 itch/GitHub Pages/Netlify 分发
- `docs/guides/sample-game-verification.md` — 示例游戏双端验证（含 `.new` 过期提法，待更新）
- `.github/workflows/deploy-web.yml` — Web 播放器 GitHub Pages 自动部署
- ROADMAP-100 round 90 — 首次 Release 流程实测记录（Release 构建/C++849/Lua 126+19/CPack 87.9MB/ZIP 359 文件冒烟）