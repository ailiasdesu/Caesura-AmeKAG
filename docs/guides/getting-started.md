# Getting Started with Caesura (AmeKAG)

## Requirements

- **Windows**: Visual Studio 2022, CMake 3.25+, vcpkg
- **Linux (Ubuntu 24.04)**: GCC 13+, CMake 3.25+, SDL3, FreeType, zstd, OpenSSL
- **macOS**: Xcode 15+, CMake 3.25+, Homebrew

## Quick Build

### Windows

```powershell
# Install SDL3 via vcpkg
vcpkg install sdl3 --triplet x64-windows

# Configure
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"

# Build + Run
cmake --build build --config Debug --parallel
```

> **运行要求（重要）**：引擎从 **项目根目录** 启动（`assets/` 等资源
> 路径是相对 CWD 解析的）。从构建目录启动会导致资源加载失败
> （背景/音频缺失，画面异常）：
>
> ```bash
> # 从项目根运行（Windows）
> ./build/Debug/CaesuraAmeKAG.exe
> # 从项目根运行（macOS/Linux）
> ./build/CaesuraAmeKAG
> ```

### Linux

```bash
# Install dependencies
sudo apt-get install cmake build-essential libfreetype-dev libzstd-dev \
  libssl-dev libx11-dev libpulse-dev libasound2-dev

# Build SDL3 from source
wget https://github.com/libsdl-org/SDL/releases/download/release-3.2.0/SDL3-3.2.0.tar.gz
tar xzf SDL3-3.2.0.tar.gz && cd SDL3-3.2.0
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local && sudo cmake --install build

# Build engine（FFmpeg 未安装时自动回退 pl_mpeg，仅视频解码受限、不阻断构建）
cd .. && cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

> **FFmpeg 视频解码**：`CAESURA_ENABLE_FFMPEG`（默认 `ON`）在未找到 libavcodec/avformat/
> swscale/avutil/swresample 时优雅回退到 pl_mpeg（仅 MPEG-1）。如需 FFmpeg 全格式解码，
> 安装 dev 包后重配：
> `sudo apt-get install libavcodec-dev libavformat-dev libswscale-dev libavutil-dev libswresample-dev`。
> `CAESURA_VIDEO_FFMPEG` 宏只在找到 FFmpeg 时才定义。`CAESURA_LIVE2D=OFF` 是本仓库常规缺省。

### macOS

```bash
brew install cmake sdl3 freetype zstd openssl@3 ffmpeg

cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$(brew --prefix openssl@3)"
cmake --build build --parallel
```

> 若省略 `ffmpeg`，构建会以 `CAESURA_ENABLE_FFMPEG=ON` 却找不到库而回退到 pl_mpeg
> （仅 MPEG-1 视频）——不会失败，仅视频解码能力受限。

## Run Tests

```bash
# From project root
cd build/tests/Debug   # Windows
# cd build/tests        # Linux/macOS
./CaesuraTests
```

## Project Structure

```
Caesura(AmeKAG)/
├── src/                  # C++ engine source
│   ├── audio/            # SoLoud audio backend
│   ├── archive/          # CARC format + crypto
│   ├── entry/            # Engine composition root
│   ├── render/           # bgfx rendering
│   ├── script/           # Lua VM + KAG bindings
│   ├── storage/          # Save/load system
│   └── ...
├── scripts/              # Lua game scripts
│   ├── kag/              # KAG command parser + scheduler
│   ├── demo.lua          # Demo scene
│   └── demo_story.ks     # KAG demo script
├── tests/                # Test suite
│   ├── cpp/              # C++ tests (doctest)
│   └── scripts/          # Test KAG scripts
├── external/             # Third-party libraries
│   ├── SDL3/             # SDL3 prebuilt (Windows)
│   ├── bgfx/             # bgfx rendering library
│   ├── soloud/           # SoLoud audio engine
│   └── lua/              # Lua 5.4
└── docs/                 # Documentation
    ├── api/              # API references
    └── guides/           # User guides
```

## Your First Scene

Create a file `my_first_scene.ks`:

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

## Running Your Script

The engine loads scripts from the `scripts/` directory. Place your `.ks` files there
and reference them from `scripts/demo.lua` or your own game entry point.

## Next Steps

- Read the [KAG Language Tour](../guides/kag-language-tour.md) — complete syntax guide
- Read the [Command Contracts](../api/command-contracts.md) for all available commands (auto-generated, authoritative)
- Read the [Lua Module API](../api/lua-modules.md) for scripting APIs
- Study `scripts/demo_story.ks` for a complete example
- Study `scripts/demo_tutorial.ks` for a capability-by-capability tour scene
- Run the full example game `demo/example_game/` ("The Last Letter" —
  multi-chapter flow, choices, three endings, macros, Lua hybrid):

```bash
# from the repo root
lua demo/example_game/entry.lua
# or from the build output (assets/scripts copied)
cd build/Debug && lua ../../demo/example_game/entry.lua
```

> **Windows 上的 `lua`**：仓库自带 Lua 5.4 解释器，位于 `external/lua/lua.exe`
> （已 vendored，无需单独安装）。若 `lua` 不在 PATH，改用 `external/lua/lua.exe`，例如：
> `external/lua/lua.exe demo/example_game/entry.lua`。

- Migrate legacy KAG3 scripts: `lua scripts/kag3_import.lua <scene.ks>`
  (see [kag3-import](../guides/kag3-import.md); CARC archives supported
  via `--carc game.carc --path assets/script/main.ks`)

## Demo/视频导出（差异化功能）

录制的回放（见 replay 系统）可以驱动游戏自身并逐帧导出 PNG 序列：

```bash
# 1) 录制一段输入（游戏中执行 [replay mode="record"] 或配置回放）
# 2) 导出为帧序列（需要真实 GPU 窗口；--headless 会被自动忽略）
./build/Debug/CaesuraAmeKAG.exe --export-replay demo_replay.json --export-dir export_out --frames 300
# 3) 用 ffmpeg 合成视频
ffmpeg -framerate 60 -i export_out/frame_%05d.png -c:v libx264 -pix_fmt yuv420p trailer.mp4
```

引擎截图回调（`BgfxDebugCallback::screenShot`）把 bgfx readback 写为
PNG（RGBA/BGRA 自动处理），修复了此前 RPC `getFrame` 恒失败的缺陷。

## 快速验证清单（Smoke Checklist）

从克隆源码到 Demo 可跑，新开发者按序自检（每步应无错误）：

### 1. 构建

- [ ] `cmake -B build -DCAESURA_LIVE2D=OFF`（Windows MSVC 缺省单配置 Debug）
- [ ] `cmake --build build --config Debug --parallel` 零错误
- [ ] 产物存在：`build/Debug/CaesuraAmeKAG.exe`（Windows）/ `build/CaesuraAmeKAG`（单配置）
- [ ] 从**项目根目录**启动（资源路径相对 CWD 解析），不是从 build 目录：
      `./build/Debug/CaesuraAmeKAG.exe` → 应看到引擎窗口/日志，无资源加载错误

### 2. 测试

- [ ] C++ 测试：`cd build/tests/Debug && ./CaesuraTests.exe` → 0 failed
- [ ] Lua 主套件：`external/lua/lua.exe tests/scripts/run_lua_tests.lua` → 全绿
- [ ] Lua 孤儿套件：`external/lua/lua.exe tests/scripts/run_orphan_tests.lua` → 全绿
- [ ] CTest：`ctest -C Debug --test-dir build --output-on-failure`

### 3. Demo 运行

- [ ] KAG 示例游戏：`lua demo/example_game/entry.lua`（无 `lua` 用 `external/lua/lua.exe`）
      → 打印 `[ExampleGame] Loading: demo/example_game/story.ks` 且无 FATAL
- [ ] 打开 KAG 语言向导剧本：`lua scripts/kag_demo_entry.lua` / `scripts/demo_story.ks`
- [ ] 脚本契约校验：`lua scripts/ks_check.lua demo/example_game/story.ks` → 0 violations

### 4. KAG3 导入器烟测（可选）

- [ ] 转换+检查：`lua scripts/kag3_import.lua --strict <scene.ks>` → 退出码 0（干净）
- [ ] 转换输出：`lua scripts/kag3_import.lua -o out/ <scene.ks>` 生成 `<name>.imported.ks`
- [ ] CARC 提取导入：`lua scripts/kag3_import.lua --carc game.carc --path assets/script/main.ks`
      （依赖 `bin/Debug/carc_pack.exe`，见 [carc-packaging](../guides/carc-packaging.md)）
- [ ] 导入器回归：`external/lua/lua.exe tests/scripts/test_kag3_import.lua` → 93 passed

### 5. 编辑器 / 视频导出（可选）

- [ ] 编辑器：`./build/Debug/CaesuraAmeKAG.exe --editor`（HTTP :9876）/ `--editor-stdio`（stdio JSON-RPC）
- [ ] 视频导出：`./build/Debug/CaesuraAmeKAG.exe --export-replay r.json --export-dir out --frames 300`
      （需真实 GPU 窗口；`--headless` 在有 `--export-replay` 时被自动忽略）

> `lua` 指仓库 vendored 的 `external/lua/lua.exe`；把常用命令固化进脚本可省去每次重复。
