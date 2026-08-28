# Getting Started with Caesura (AmeKAG)

> 本指南带你**从克隆仓库到跑通示例游戏**：逐平台环境准备 → 构建 → 运行 →
> 测试 → 常见问题。全程约 30–60 分钟（构建占大头）。
>
> 配套文档：[KAG 语言教程](kag-language-tour.md)（语法）· [命令契约](../../docs/api/command-contracts.md)
> （134 命令权威参考）· [资源管线](asset-pipeline.md)（资产格式与目录）·
> [示例库](sample-library.md)（16 教程 + 完整游戏）。

---

## ⭐ 30 分钟速通：从零到第一个可运行 VN（KPI-1 路径）

> 这是新手最短路径（产品化 KPI-1：陌生开发者 ≤30 分钟完成首个 Tiny VN）。
> 完整细节见后文 §1-§10；本速通覆盖「下载 → 创建 → 编辑 → 运行 → 打包」。

### ⚡ 五个步骤

> **⚠️ 当前实测口径（2026-08-27）**：下面这条路径的起点是 **克隆源码 + 构建一次**，
> 不是"下载 Release 双击就有 IDE"。
>
> - **已发布的 [v1.0.1 ZIP](https://github.com/ailiasdesu/Caesura-AmeKAG/releases)**（403 文件）
>   里 `web-editor` 与 `project_templates` 命中数均为 **0**，`--editor` 打印
>   `web-editor/dist not found; serving API only`，`/api/project/*` 全部 404。
> - **Sprint 4 已修好的部分（本机在安装树实测）**：`install()` 现在会装
>   `web-editor/dist/`，静态壳免令牌，启动日志直接给出可点开的地址
>   （`[EditorServer] Open the editor: http://127.0.0.1:9876/?token=...`），
>   带令牌请求 `/api/project/templates` 返回 200。
> - **Sprint 4 已闭环（模拟发布包实测）**：`tools/project_templates/`
>   （5 模板 + manifest.json）已随发布包安装，且 `caesura.py create` 与编辑器
>   `ProjectContext::sourceRoot()` 都接受发布包布局——模板按「可执行文件 /
>   CLI 脚本所在目录」优先解析（回退 CWD 上探），不再依赖编译期
>   `CAESURA_SOURCE_DIR` 指向开发机源码树。见
>   [release-process.md](release-process.md) §5。
>
> 因此"解压即建项目"成立：`python scripts/caesura.py create my_game` →
> `python scripts/caesura.py build my_game` → 运行 `dist/my_game-game`；
> 下面写的是**当前真的能走通**的路径。

1. **构建引擎**（一次性，§1–§2）：`cmake -B build` → `cmake --build build --config Debug --parallel`。
   Windows 上仓库自带 SDL3 预编译包（`external/SDL3/SDL3-3.2.0`），
   **不传 `-DCMAKE_TOOLCHAIN_FILE` 时会自动使用它**——vcpkg 不是必需项（本机
   `build/CMakeCache.txt` 里 `SDL3_DIR` 就指向仓库内置包，vcpkg 命中数为 0）。
2. **创建项目**：`python scripts/caesura.py create my_vn --template basic`
   （从 `tools/project_templates/basic` 复制，产出 `story.ks` 两场景双分支 +
   `caesura.project.json`）。模板按 CLI 脚本自身位置自动解析（仓库根或解压的
   发布包内均可），不再要求从仓库根执行。
3. **编辑**：用任意文本编辑器改 `my_vn/story.ks`（`[bg]`/`[ch]`/`[sel]` 语法见
   [KAG 语言教程](kag-language-tour.md)）。**Monaco 编辑器 + Project Manager +
   Build Manager 面板属于 `editor/` 这个 React IDE**，需要 `cd editor && npm ci &&
   npm run dev`（http://localhost:5173）后在浏览器里连引擎——它**不在**引擎二进制里，
   `editor/dist/` 也不入库（`.gitignore:35`）。
4. **运行**：从仓库根 `./build/Debug/CaesuraAmeKAG.exe`；静态校验
   `build/lua/Debug/lua.exe scripts/ks_check.lua my_vn/story.ks`；
   无 GPU 也能跑逻辑：`build/lua/Debug/lua.exe scripts/kag_runner.lua my_vn/story.ks`。
5. **打包**（两种产物，都实测可用）：
   - **桌面 game-only 包**：`python scripts/caesura.py build my_vn -o dist/my_vn-game`
     —— 产出可双击的自包含目录（引擎 exe + SDL3/FFmpeg DLL + 只属于该游戏的
     `assets/` + 预编译 `cache/ksc` + `HOW-TO-PLAY.txt` + `BUILD-INFO.json`）。
     实测（`tests/projects/first_vn`）：`./CaesuraAmeKAG.exe --frames 60` 退出码 0、
     日志无 FATAL。`caesura package --target windows|web|both` 进一步压成可分发归档。
   - **静态 Web 站**：`bash scripts/package_game.sh --out dist/my_vn my_vn`
     （itch.io / GitHub Pages / Netlify 直接托管）。

**引擎自带的 `--editor` 是什么**：`./build/Debug/CaesuraAmeKAG.exe --editor` 在
:9876 起 HTTP 服务并提供 `web-editor/dist/index.html`——一个**单文件调试面板**
（脚本 / 调试 / 帧捕获 / 日志四个页签），**没有** Project Manager、没有 Monaco。
它默认要求 bearer 令牌：启动时令牌打印在 stderr 并写入启动目录的
`.caesura-editor-token`。

> **令牌与浏览器（两种版本行为不同，先认版本）**：
> - **Sprint 4 之后**（判据：启动日志出现
>   `[EditorServer] Open the editor: http://127.0.0.1:9876/?token=...`）——
>   静态壳（`/` 与 `/index.html`）免令牌，页面读取 `?token=` 存入 localStorage，
>   之后所有 `/api/*` 带 bearer；直接点开日志里那条地址即可。安装树实测：
>   `GET /` 无令牌 200、带令牌 200（`<title>Caesura Web Editor</title>`），
>   `/api/project/templates` 带令牌 200。
> - **Sprint 4 之前的二进制**——门禁覆盖包括静态页在内的所有非 OPTIONS 请求：
>   `curl -H "Authorization: Bearer <令牌>" http://127.0.0.1:9876/` 返回 200，
>   但浏览器地址栏访问 `/` 或 `/?token=<令牌>` 都 **401**（地址栏无法附带
>   Authorization 头，服务端也不解析 `?token=`），唯一浏览器入口是
>   `--editor-insecure`。

### ✅ 完成判定（Tiny VN 必须包含）

background ✓ · character ✓ · dialogue ✓ · choice ✓ · BGM ✓ · save/load ✓

> 现成验收范例：`tests/projects/first_vn/`（包含上述全部要素，13 项验收门禁
> `bash scripts/verify_first_vn.sh`）。新手可复制它的写法快速上手。

---

## 0. 你需要什么

Caesura (AmeKAG) 是 **C++20 + bgfx 渲染 + SDL3 窗口 + Lua 5.4 脚本** 的跨平台视觉小说引擎。
克隆源码后你不需要任何游戏引擎 IDE——一个文本编辑器 + 命令行即可起步。

| 需求 | 最低版本 | 说明 |
|------|---------|------|
| Git | 任意 | 克隆仓库、语义提交 |
| CMake | **3.25+** | 顶层 CMakeLists 要求 min 3.25 |
| C++ 编译器 | VS2022 (MSVC v143) / GCC 13+ / Xcode 15+ (Apple Clang) | 见 §1 各平台 |
| Python | 3.8+ | 仅标准库：api_stats / gen_changelog / 耦合计数脚本 |
| Lua | 不需安装 | CMake 构建自带 `lua_cli` 目标（产物 `build/lua/<配置>/lua.exe`，Lua 5.4）；发布包内置 `external/lua/lua.exe`。CLI 的 `find_lua` 依次探测两处再回退 PATH |
| Node.js | 18+ | 仅 Web 播放器 / editor 需要（§6） |

> **磁盘与内存**：全量 Debug 构建约需 3–5 GB 磁盘（thirdparty + build 目录）；
> 并行构建建议 16 GB 内存。Release 构建额外 ~2 GB。

---

## 1. 逐平台环境准备

### 1.1 Windows（主开发平台）

```powershell
# 1) 安装 Visual Studio 2022（含 "Desktop development with C++" 工作负载，
#    即 MSVC v143 工具集 + Windows SDK）
winget install Microsoft.VisualStudio.2022.BuildTools

# 2) 安装 CMake（或从 https://cmake.org/download 装二进制）
winget install Kitware.CMake

# 3) 安装 Git
winget install Git.Git

# 4) 安装 vcpkg 并装 SDL3（引擎 Windows 构建依赖 SDL3 的 CMake 包）
git clone https://github.com/microsoft/vcpkg C:/vcpkg
cd C:/vcpkg && .\bootstrap-vcpkg.bat
.\vcpkg install sdl3 --triplet x64-windows
# 记住 vcpkg 根目录，稍后配置时用

# 5) （可选，不阻塞）git bash：仓库内脚本约定用 git bash 运行
```

> **vcpkg 位置**：以下假设 `C:/vcpkg`，配置命令里的 `$env:VCPKG_INSTALLATION_ROOT`
> 会自动指向。若装在别处，改路径即可。

### 1.2 Linux（Ubuntu 24.04 参考）

```bash
# 1) 基础 + 编译工具
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config

# 2) 引擎依赖（FreeType / zstd / OpenSSL / X11 / 音频）
sudo apt-get install -y libfreetype-dev libzstd-dev libssl-dev \
  libx11-dev libpulse-dev libasound2-dev

# 3) SDL3 —— 推荐源码装 3.2.0：
wget https://github.com/libsdl-org/SDL/releases/download/release-3.2.0/SDL3-3.2.0.tar.gz
tar xzf SDL3-3.2.0.tar.gz && cd SDL3-3.2.0
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
sudo cmake --install build
cd ..

# 4) （可选）FFmpeg 全格式视频解码：
sudo apt-get install -y libavcodec-dev libavformat-dev libswscale-dev \
  libavutil-dev libswresample-dev
# 不装也没关系：引擎自动回退 pl_mpeg（仅 MPEG-1 视频），构建不失败
```

### 1.3 macOS（Xcode + Homebrew）

```bash
# 1) 安装 Command Line Tools（Xcode 打开时会提示，或：）
xcode-select --install

# 2) Homebrew（若未安装）：https://brew.sh

# 3) 依赖
brew install cmake sdl3 freetype zstd openssl@3   # ffmpeg 可选，见上

# 4) 构建时用 CMAKE_PREFIX_PATH 让 CMake 找到 openssl@3（Homebrew 把
#    openssl 装成 keg-only，不加入默认搜索路径）
```

### 1.4 三平台差异速查

| 维度 | Windows | Linux | macOS |
|------|---------|-------|-------|
| 生成器 | Visual Studio 17 2022（多配置） | Unix Makefiles / Ninja（单配置） | Xcode / Unix Makefiles |
| 构建命令 | `cmake --build build --config Debug` | `cmake --build build`（配置在 configure 时定） | 同 Linux |
| 可执行产物 | `build/Debug/CaesuraAmeKAG.exe` | `build/CaesuraAmeKAG` | `build/CaesuraAmeKAG` |
| 测试二进制 | `build/tests/Debug/CaesuraTests.exe` | `build/tests/CaesuraTests` | `build/tests/CaesuraTests` |
| SDL3 来源 | vcpkg | 源码装（见上） | Homebrew |
| 默认渲染后端 | D3D11（bgfx auto） | OpenGL 4.3 | Metal（D3D11 失败后 auto-select） |
| FFmpeg | vcpkg 装或内置 | apt dev 包（可选） | brew ffmpeg（可选） |
| 注意 | 必须从**项目根**启动（资源相对 CWD） | 同左 | 同左 |

---

## 2. 克隆与首次配置

```bash
# 推荐 1：部分克隆（仅按需下载当前提交的 blob，保留完整分支历史，下载量仅 ~25MB，速度提升 15 倍）
git clone --filter=blob:none https://github.com/ailiasdesu/Caesura-AmeKAG.git CaesuraAmeKAG
cd CaesuraAmeKAG

# 推荐 2：浅克隆（仅拉取最新 HEAD，适合 CI 或快速体验）
# git clone --depth=1 https://github.com/ailiasdesu/Caesura-AmeKAG.git CaesuraAmeKAG

# 确认在 master 分支
git branch --show-current          # -> master
```

**目录速览**（完整结构见 README）：

```
Caesura(AmeKAG)/
├── src/                  # C++ 引擎（16 个静态模块库 + 15 个 API-only CMake targets）
│   ├── entry/            # 组合根（唯一 new 具体后端的地方）
│   ├── render/           # bgfx 渲染（IRenderDevice）
│   ├── script/           # Lua VM + KAG 绑定（ILuaManager）
│   ├── resource/         # 异步资源加载（IAssetProvider）
│   ├── archive/          # CARC 加密归档
│   └── ...（audio/storage/platform/input/job/live2d/minigame/steam/debug/rpc/di）
├── scripts/              # Lua 运行时（kag/ 命令处理器 + scheduler + 工具脚本）
├── demo/                 # 教程 tutorial_01–16、示例游戏 example_game/、模板 template/
├── assets/               # 共享资产池（bg/fg/bgm/se/voice/fonts/lang）
├── tests/                # C++ (doctest) + Lua 套件
├── external/             # vendored 第三方库（SDL3/bgfx/soloud/lua/…）
├── thirdparty/           # 本地可选依赖（如 Cubism SDK，不入库）
├── web/                  # Web 播放器（wasmoon + vite）
├── editor/               # 网页编辑器（React + TS）
├── docs/                 # 文档（api/design/guides/plans/solutions）
└── CMakeLists.txt        # 顶层构建（min CMake 3.25）
```

### 2.1 构建（Windows 为例）

```powershell
# 1) 配置（一次性；生成 Visual Studio 解决方案）
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"

# 2) 构建 Debug（主开发配置）
cmake --build build --config Debug --parallel
# 预期：零错误；产物 build/Debug/CaesuraAmeKAG.exe
```

**常用 CMake 选项**（完整表见 CLAUDE.md）：

| 选项 | 默认 | 作用 |
|------|------|------|
| `CAESURA_LIVE2D` | `OFF` | Live2D Cubism SDK（需手动下载 SDK，见 live2d-setup.md） |
| `CAESURA_ENABLE_FFMPEG` | `ON` | FFmpeg 视频解码（找不到时自动回退 pl_mpeg） |
| `CAESURA_DEBUG` | ON (Debug) | Debug 日志与断言宏 |

> **首次构建耗时**：Debug 全量约 5–15 分钟（高核数）到 30+ 分钟（低核数）。
> 之后是增量构建，几秒到几十秒。

### 2.2 构建（Linux / macOS）

```bash
# 配置（单配置生成器：Debug 是默认 CMAKE_BUILD_TYPE）
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  # macOS 追加： -DCMAKE_PREFIX_PATH="$(brew --prefix openssl@3)"
  # 无 FFmpeg 时追加： -DCAESURA_ENABLE_FFMPEG=OFF

# 构建
cmake --build build --parallel
# 产物：build/CaesuraAmeKAG
```

---

## 3. 运行

> **最重要的一条规则：从项目根目录启动。** 引擎把所有资源路径
> （`assets/`、`demo/`、`scripts/`）相对**当前工作目录 (CWD)** 解析。
> 从 build 目录启动会找不到资源，导致背景/音频缺失、画面异常。

```bash
# Windows（在仓库根目录执行）
./build/Debug/CaesuraAmeKAG.exe

# Linux / macOS
./build/CaesuraAmeKAG
```

你会看到一个窗口渲染 KAG 默认 demo（`demo/galgame_demo.ks`），日志打印
`[Engine]` 各子系统注册行。按 **Esc** 或关闭窗口退出。

**命令行参数**（`--help` 列出全部）：

| 参数 | 作用 |
|------|------|
| `--editor` | HTTP 编辑器（:9876）。未设 `CAESURA_EDITOR_TOKEN` 时引擎自动生成令牌，打到 stderr 并写入启动目录的 `.caesura-editor-token` |
| `--editor-insecure` | 关闭编辑器鉴权（仅本机调试；会大声告警） |
| `--editor-stdio` | stdio JSON-RPC 编辑器 |
| `--backend <type>` | 强制渲染后端（metal/opengl/vulkan/dx11/dx12/webgpu） |
| `--frames N` | 渲染 N 帧后确定性退出（CI/冒烟用） |
| `--export-replay r.json --export-dir out` | 逐帧导出 PNG 序列（§8） |
| `--headless` | 无窗口模式（无 GPU 时给 test/determinism 用） |

---

## 4. 测试（跑通 Demo 前的自检）

> **测试基线（本机实测，2026-08-28 master `4e4abf57` + 本轮 N1 用例）**：C++ 用例 **1120**（385790 断言）·
> Lua 主套件 **143** + 孤儿套件 **24** · Web vitest **369** · Editor vitest **615** ·
> CTest **14** 个 target · 16 教程 · **134** 命令契约 · **34** 个 C++ 接口头（`docs/api/api-stats.md`，由 `python scripts/api_stats.py` 生成）。
> 任何 PR 合入前这些必须全绿。基线数字随开发增长，**以本地实跑输出为准**，不要把本行当门禁。

### 4.1 C++ 测试（doctest / CTest）

```bash
# Windows：从 build/tests/Debug 运行（CWD 影响资源路径，别从仓库根跑）
cd build/tests/Debug
./CaesuraTests.exe
# 预期输出尾部（2026-08-28 本机实测；数字随开发增长，关键是 0 failed / 0 skipped）：
#   [doctest] test cases:   1120 |   1120 passed | 0 failed | 0 skipped
#   [doctest] assertions: 385790 | 385790 passed | 0 failed

# Linux / macOS
cd build/tests && ./CaesuraTests
```

**doctest 过滤器**（调试单个模块时很有用）：

```bash
./CaesuraTests.exe -tc="*SaveManager*"   # 按测试用例名（支持通配符）
./CaesuraTests.exe -ts="*Render*"        # 按测试套件名
./CaesuraTests.exe -tce="*Slow*"         # 排除匹配用例
./CaesuraTests.exe -s                    # 显示通过用例的输出
./CaesuraTests.exe -d                    # 显示每个用例耗时
```

或经 CTest：

```bash
ctest -C Debug --test-dir build --output-on-failure
# 14 个 target（`ctest --test-dir build -N` 列出）：资产同步 + 6 组分模块 doctest 分片
# + headless CLI/RPC/HTTP 冒烟 + AI smoke（无 Ollama 自动跳过）+ RC/平台矩阵对抗测试
# + 构建 CLI 契约（无引擎时 SKIP 77）
```

### 4.2 Lua 套件（两种：主套件 + 孤儿套件）

```bash
# 从仓库根运行；解释器用构建产物 build/lua/<配置>/lua.exe——全新克隆没有
# external/lua/lua.exe（那是发布包内置路径，find_lua 会依次探测两处）
build/lua/Debug/lua.exe tests/scripts/run_lua_tests.lua      # 主套件（实测 143 passed / 143 total）
build/lua/Debug/lua.exe tests/scripts/run_orphan_tests.lua   # 孤儿套件（实测 24 passed / 24 total）

# 主套件 = 大部分测试；孤儿套件 = 会创建全局 mock 的测试（沙箱中途锁全局），
# 每个测试子进程隔离运行。两套都必须全绿，孤儿永远不能并入主套件。
```

> **为什么分两个套件**：主套件的 sandbox 会在运行中途锁定全局表；
> 创建全局 mock 的测试（如 test_tween / test_layout_cmds）必须在孤儿套件里
> 以独立子进程运行，否则会互相污染。

### 4.3 静态契约校验（.ks 场景）

```bash
# 校验单个场景（引擎+Web 双端在跑的脚本契约）
build/lua/Debug/lua.exe scripts/ks_check.lua demo/galgame_demo.ks
# 校验全部 16 个教程
build/lua/Debug/lua.exe scripts/ks_check.lua demo/tutorial/*.ks
# 预期：OK — all scenes pass contract checks（0 violations）
```

### 4.4 Web / 脚本索引保鲜（CI 守卫）

```bash
node web/gen-index.mjs --check    # 三平台 CI 都跑；改过 scripts/*.lua 必须先
                                  # node web/gen-index.mjs 重生成并提交
```

---

## 5. 你的第一个场景

### 5.1 写剧本

创建 `my_first_scene.ks`：

```kag
; my_first_scene.ks — Your first KAG scene

*start
[bg storage="bg/school.png"]
[wait time=500]

[ch name="Hero" text="Hello, this is my first visual novel scene!"]
[p]

[playbgm storage="bgm/title.ogg" volume=0.8]
[ch name="Hero" text="Background music starts playing..."]
[p]

[fg storage="chara/hero_smile.png" layer=1]
[position layer="fg0" x=0.5 y=0.3 scale=1.0]
[ch name="Hero" text="Here is a character sprite!"]
[p]

[stopbgm time=500]
[end]
```

> `storage=` 路径相对**游戏根**（即 `assets/` 池）。引擎找不到资产时
> **不会崩溃**：缺图显示占位纹理（开发紫/发布灰），缺音频静音。
> ——所以你可以先写剧本、后补资产。

### 5.2 校验与运行

```bash
# 1) 静态契约校验（推荐每次都跑）
build/lua/Debug/lua.exe scripts/ks_check.lua my_first_scene.ks

# 2) 用 KAG runner 直接驱动到 [end]（无 GPU 也能验证逻辑）
build/lua/Debug/lua.exe scripts/kag_runner.lua my_first_scene.ks

# 3) 在引擎里跑：把 .ks 放进 scripts/ 或 demo/，从引擎入口引用；
#    或直接改 demo/example_game/story.ks 的剧情（§7 模板法）
```

### 5.3 从模板起步（推荐路线）

不想从零组织脚本/资产？两条现成路径：

- **项目模板** `demo/template/`：两场景 + 一次选择的最小骨架。
  ```bash
  cp -r demo/template my_game
  # 改 my_game/story.ks → lua my_game/entry.lua 打开窗口跑
  # 全链路校验：bash scripts/verify_template.sh
  # 打包：bash scripts/package_game.sh --out dist/my_game --assets my_assets my_game
  ```
  详见 [template-quickstart.md](template-quickstart.md)。
- **完整示例游戏** `demo/example_game/`（《单程回信 The One-Way Reply》）：
  三结局 + 选择分支 + i18n 双语 + SMA 骨骼动画 + 双存档点。
  ```bash
  build/lua/Debug/lua.exe demo/example_game/entry.lua
  # 或从 build 输出目录：cd build/Debug && lua ../../demo/example_game/entry.lua
  ```

教程按 **tutorial_01_hello → tutorial_16_tween** 顺序学习全部能力（16 个教程，
每个独立可跑、逐行注释、引擎 + Web 双验证）。运行方式：

```bash
for f in demo/tutorial/tutorial_*.ks; do
  build/lua/Debug/lua.exe scripts/ks_check.lua "$f"
done
```

---

## 6. Web 播放器（可选）

`web/` 是纯前端的 KAG 播放器（wasmoon Lua VM + vite），直接在浏览器跑 `.ks`，
**无需构建 C++ 引擎**：

```bash
cd web
npm ci                 # 首次装依赖
npm run dev:web        # 开发服务器 http://127.0.0.1:5174
# 打开页面 → 场景下拉框选你的 .ks → Run
```

生产构建与测试：

```bash
npx vite build         # 输出 web/dist/
npm test               # vitest 单元/集成测试
node gen-index.mjs     # 刷新 scripts-index.json（改了 scripts/*.lua 就要重跑）
```

---

## 7. 视频导出（差异化能力）

录制的回放可以驱动游戏逐帧导出 PNG 序列（见 [replay] 系统）：

```bash
# 1) 录制输入（游戏内 [replay mode="record"]）→ demo_replay.json
# 2) 导出帧序列（需真实 GPU 窗口；--headless 会被自动忽略）
./build/Debug/CaesuraAmeKAG.exe --export-replay demo_replay.json \
  --export-dir export_out --frames 300
# 3) ffmpeg 合成视频
ffmpeg -framerate 60 -i export_out/frame_%05d.png -c:v libx264 -pix_fmt yuv420p trailer.mp4
```

---

## 8. 常见问题（FAQ）

### 构建失败

**Q: `CMake Error: Could not find a package configuration file provided by "SDL3"`**
A: Windows 上未装 vcpkg SDL3 或没传 toolchain 文件。`vcpkg install sdl3 --triplet x64-windows`
   后，configure 时带 `-DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"`。
   Linux/macOS 上确认已装 SDL3（§1.2/§1.3），或改用 Homebrew `brew install sdl3`。

**Q: CMake 报 `CMake 3.25 or higher is required`**
A: 版本太旧。`cmake --version` 查看；Windows 用 winget 装最新，Linux `sudo apt install cmake`
   后 Ubuntu 24.04 自带 3.28，macOS `brew upgrade cmake`。

**Q: 编译报 `C4715: not all control paths return a value`（MSVC 警告）**
A: 见仓库记忆：修改 `main.cpp` 的 if constexpr RPC 分支链时**务必保留每个分支的
   return 收尾**——这条警告往往是静默丢 return 的前兆，运行期会 0xC0000005。

**Q: 构建卡住/内存不足**
A: 降低并行度：`cmake --build build --config Debug -j 4`。或先清理 `build/` 重来
   （`rm -rf build` 后重新 configure）。

**Q: 中文路径/带括号路径构建失败**
A: 本项目路径含 `( )`（`Caesura(AmeKAG)`），git bash 下子进程 argv 会被 cmd 分词截断
   ——用 `call "..."` 包裹，或把仓库放到无括号路径。

### 运行问题

**Q: 窗口打开但背景/立绘全缺 / 报一大片资源加载错误**
A: 从**构建目录**启动了。回到仓库根目录再运行（§3 的关键规则）。

**Q: `cannot open file: assets/...`**
A: 资源路径相对游戏根。确认 `storage=` 的路径在 `assets/` 下真实存在
   （`test -f assets/xxx` 验证），缺失时引擎会占位但不报错。

**Q: 视频不播 / 只播 MPEG-1**
A: 构建时没找到 FFmpeg（`CAESURA_ENABLE_FFMPEG=ON` 但库缺失会静默回退 pl_mpeg）。
   装 dev 包后**重新 configure + 构建**；验证：`CMakeCache.txt` 里 grep
   `CAESURA_VIDEO_FFMPEG`（只有找到 FFmpeg 才定义）。

**Q: `lua` 命令找不到**
A: 无需安装：构建后解释器在 `build/lua/<配置>/lua.exe`（`lua_cli` 目标产物），
   发布包内置 `external/lua/lua.exe`（注意：该文件不入库，全新克隆里没有，
   须先构建）。文档命令里的 `lua` 指其中任意一个；PATH 里没有时用完整路径。

**Q: 引擎窗口闪退**
A: 在终端里直接运行看输出（不要双击 exe）。常见：缺 SDL3.dll（Windows Release
   运行时需把 DLL 放同目录）、从错误 CWD 启动、`--editor` 缺 token。

### 测试问题

**Q: `CaesuraTests` 报一堆资源路径错误**
A: 必须在 `build/tests/Debug` 目录下运行（CWD 需匹配资源路径），不是仓库根。

**Q: Lua 套件在 test_tween 处失败**
A: test_tween 创建全局 mock，必须在**孤儿套件**（run_orphan_tests.lua）运行。
   确认它登记在孤儿清单里；切套件由 run_orphan_tests.lua 管理。

**Q: 改了 scripts/*.lua 后 CI 报 gen-index 红**
A: `node web/gen-index.mjs` 重生成 `web/scripts-index.json` 并提交。

---

## 9. 快速验证清单（Smoke Checklist）

从克隆源码到 Demo 可跑，新开发者按序自检（每步应无错误）：

### 1. 构建

- [ ] `cmake -B build -DCAESURA_LIVE2D=OFF`（Windows MSVC 缺省单配置 Debug）
- [ ] `cmake --build build --config Debug --parallel` 零错误
- [ ] 产物存在：`build/Debug/CaesuraAmeKAG.exe`（Windows）/ `build/CaesuraAmeKAG`（单配置）
- [ ] 从**项目根目录**启动（资源路径相对 CWD 解析），不是从 build 目录：
      `./build/Debug/CaesuraAmeKAG.exe` → 应看到引擎窗口/日志，无资源加载错误

### 2. 测试

- [ ] C++ 测试：`cd build/tests/Debug && ./CaesuraTests.exe` → **0 failed, 0 skipped**（2026-08-28 实测 1120 用例）
- [ ] Lua 主套件：`build/lua/Debug/lua.exe tests/scripts/run_lua_tests.lua` → 0 failed（实测 143）
- [ ] Lua 孤儿套件：`build/lua/Debug/lua.exe tests/scripts/run_orphan_tests.lua` → 0 failed（实测 24）
- [ ] CTest：`ctest -C Debug --test-dir build --output-on-failure`（14 target）
- [ ] 耦合门禁：`python scripts/count_coupling.py --ci` → `PASS: All modules within thresholds`
- [ ] Web 脚本索引守卫：`node web/gen-index.mjs --check` → `CHECK OK: N modules up to date`
- [ ] 平台矩阵新鲜度：`python scripts/generate_platform_status.py --check` → `[OK] ... up-to-date`
- [ ] 首个 VN 用户旅程：`bash scripts/verify_first_vn.sh` → `RESULT: PASS (13/13 checks)`

### 3. Demo 运行

- [ ] KAG 示例游戏：`lua demo/example_game/entry.lua`（无 `lua` 用 `build/lua/Debug/lua.exe`）
      → 打印 `[ExampleGame] Loading: demo/example_game/story.ks` 且无 FATAL
- [ ] 打开 KAG 语言向导剧本：`lua scripts/kag_demo_entry.lua` / `scripts/demo_story.ks`
- [ ] 脚本契约校验：`lua scripts/ks_check.lua demo/example_game/story.ks` → 0 violations
- [ ] Web 播放器：`cd web && npm run dev:web` → 打开 http://127.0.0.1:5174 能跑剧本/看到画面

### 4. KAG3 导入器烟测（可选）

- [ ] 转换+检查：`lua scripts/kag3_import.lua --strict <scene.ks>` → 退出码 0（干净）
- [ ] 转换输出：`lua scripts/kag3_import.lua -o out/ <scene.ks>` 生成 `<name>.imported.ks`
- [ ] CARC 提取导入：`lua scripts/kag3_import.lua --carc game.carc --path assets/script/main.ks`
      （依赖 `bin/Debug/carc_pack.exe`，见 [carc-packaging](../guides/carc-packaging.md)）
- [ ] 导入器回归：`build/lua/Debug/lua.exe tests/scripts/test_kag3_import.lua` → 全绿（99 check）

### 5. 编辑器 / 视频导出（可选）

- [ ] 编辑器（单文件调试面板）：`./build/Debug/CaesuraAmeKAG.exe --editor` → 日志出现
      `[EditorServer] Serving web editor from: <repo>/web-editor/dist` 与令牌行；
      带令牌请求 `curl -H "Authorization: Bearer $(cat .caesura-editor-token)" http://127.0.0.1:9876/api/ping`
      → `{"status":"ok",...}`，不带令牌 → `401`
- [ ] React IDE（Project Manager / Monaco / Build Manager）：`cd editor && npm ci && npm run dev`
      → http://localhost:5173 出现 `Caesura Editor`，在页头填入令牌后 Connect
- [ ] 视频导出：`./build/Debug/CaesuraAmeKAG.exe --export-replay r.json --export-dir out --frames 300`
      （需真实 GPU 窗口；`--headless` 在有 `--export-replay` 时被自动忽略）

> `lua` 指 `lua_cli` 构建产物 `build/lua/<配置>/lua.exe`（发布包内则是内置的 `external/lua/lua.exe`）；把常用命令固化进脚本可省去每次重复。

---

## 10. Next Steps

- [KAG 语言教程](kag-language-tour.md) — 完整语法 + 命令分类 + 五段常用模板
- [命令契约](../../docs/api/command-contracts.md) — 134 命令权威参考（`lua scripts/schema_doc.lua` 生成）
- [Lua 模块 API](../../docs/api/lua-modules.md) — Lua 绑定 API
- [资源管线](asset-pipeline.md) — 资产格式支持矩阵 + 目录规范
- [一键打包](packaging-ux.md) — 从 .ks 到可分发 Web 站的单条命令
- [桌面发布](release-process.md) — Release + CPack + GitHub Release 全流程
- [首次贡献](../../CONTRIBUTING.md) — 参与开发的流程与门禁
