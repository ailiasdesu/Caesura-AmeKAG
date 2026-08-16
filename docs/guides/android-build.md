# Android 构建链指南（Android Build Chain）

> 本指南描述 Caesura (AmeKAG) 引擎的 Android 交叉编译就绪度：CMake toolchain、
> NDK 前置、关键依赖（SDL3 / bgfx / SoLoud）在 Android 下的取舍、平台化 CMake
> 选项、已知风险清单与"待设备"真机验证清单。
>
> **现状状态**：构建链文档 + 可执行脚本已就绪；本机无 NDK，未做真实交叉编译
> （如下文"本机验证"所述）。真机验证全部标注为"待设备"（P0-3 剩余项）。
>
> 关联文档：`guides/mobile-pipeline.md`（移动端管线/IMobileAdapter 对接）、
> `guides/getting-started.md`（桌面构建入门）。

---

## 1. 前置要求

### 1.1 工具链版本建议

| 组件 | 建议版本 | 说明 |
|---|---|---|
| Android NDK | **r26+**（推荐 r26 或 r27） | 旧版 r21/r23 的 clang/libc++ 对新 GCC 头文件和 C++20 支持不全；CMake toolchain 位于 `<ndk>/build/cmake/android.toolchain.cmake` |
| Android SDK | 含 platform-tools / build-tools | 构建 APK 时需要；纯交叉编译 .so 只需 NDK |
| CMake | 3.25+（顶层要求 min 3.25） | 需支持 `-DCMAKE_TOOLCHAIN_FILE` 与 NDK toolchain |
| 宿主 host 工具链 | 本机 GCC/Clang/MSVC | 只需 configure 阶段，实际编译走 NDK clang |
| 目标 ABI | `arm64-v8a`（首选） | 亦支持 `armeabi-v7a` / `x86_64`；统一 64 位 arm64 优先 |

> **NDK 发现**：构建脚本优先读 `ANDROID_NDK_HOME`（或 `ANDROID_NDK`），其次常用路径
> `$LOCALAPPDATA/Android/Sdk/ndk/<version>`（Windows）/
> `$HOME/Android/Sdk/ndk/<version>`（Linux/macOS）。也可用 `--ndk <path>` 显式指定。

### 1.2 关键变量

- `ANDROID_NDK_HOME` — NDK 根目录
- `ANDROID_HOME` 或 `ANDROID_SDK_ROOT` — Android SDK 根目录（构建 APK 时必需）
- `SDL3_DIR` — 指向 **Android 版 SDL3** 的 CMake 包目录（见 2.2，必填）

---

## 2. SDL3 的 Android 构建要点

> ⚠️ **仓库内 `external/SDL3/SDL3-3.2.0` 是 Windows x64 预编译包**（含
> `lib/x64/SDL3.dll` + `SDL3.lib` + 头文件），**不是源码树**，无法用于 Android
> 交叉编译。顶层 `find_package(SDL3 CONFIG REQUIRED)` 在 Android 下不会自动命中，
> 必须显式提供 Android 版 SDL3。

### 2.1 官方 Android 集成方式

SDL3 官方 Android 支持采用 **gradle + CMake** 集成（非传统 `ndk-build`）：

- 官方模板 `android-project`（SDL 仓库自带）中，`src/main/java/org/libsdl/app/SDLActivity.java`
  负责 Activity 生命周期、EGL 窗口、输入事件，并以 JNI 加载 SDL 与引擎原生库。
- SDL3 的 CMake 构建系统支持 `-DANDROID_ABI` + NDK toolchain 交叉编译出
  `libSDL3.so`（当 `CMAKE_SYSTEM_NAME=Android` 时启用 `SDL_VIDEO_DRIVER=android`、
  EGL/OpenGLES、触摸/输入集）。
- 引擎是纯 C++ `main()` 入口；Android 下有两种接法：
  1. **SDL3 原生 app 模式**：引擎编译为 `libCaesuraAmeKAG.so`，由 SDLActivity 的
     `SDL_main` 包装层调用——需要引擎的 `main` 用 `SDL_MAIN_HANDLED` + JNI 入口
     包装（引擎已定义 `SDL_MAIN_HANDLED`，见 `cmake/CaesuraModules.cmake`）。
  2. **JNI 桥接**：宿主 Activity 直接持有引擎，将生命周期/触摸事件经
     `IMobileAdapter` 路由进引擎（该仓库已经实现 `MobileAdapter`，见 4 与
     `mobile-pipeline.md`）。

### 2.2 为 Android 准备 SDL3（必做）

本仓库不自带 Android 版 SDL3，**构建前需先在 `SDL3_DIR` 产出一个 Android 配置的
SDL3 CMake 包**。二选一：

**A. 源码构建 SDL3 → Android 并安装（推荐）**

```bash
# 1. 克隆/解压 SDL3 源码（版本 ≥ 3.2.0 即可）
# 2. 用 NDK toolchain 交叉 configure（生成 install 树）
cmake -S /path/to/SDL3 -B /path/to/SDL3-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DCMAKE_INSTALL_PREFIX=/path/to/SDL3-android-install \
  -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF
cmake --build /path/to/SDL3-android --parallel
cmake --install /path/to/SDL3-android
# 3. 把 install 目录（含 lib/cmake/SDL3/SDL3Config.cmake）传入引擎
export SDL3_DIR=/path/to/SDL3-android-install/lib/cmake/SDL3
```

**B. 直接复用已有 Android SDL3 安装**：若已有预编译 Android SDL3 且含
`SDL3Config.cmake`，直接 `export SDL3_DIR=.../lib/cmake/SDL3`。

> 引擎顶层对 SDL3 target 有归一化（`SDL3::SDL3` ← `SDL3-shared`/`SDL3-static`/裸
> `SDL3`），Android 共享库通常暴露为 `SDL3::SDL3-shared`，会自动解析。

---

## 3. 构建命令

### 3.1 交叉编译引擎目标

> 本仓库顶层把引擎链接为**可执行文件**（`add_executable(CaesuraAmeKAG src/main.cpp)`），
> 不是显式的 `.so`。Android 端若要出共享库，需把组合根编译为 `MODULE` 库目标
> （见"已知风险 R1"）。**纯交叉编译"仅编译不链接完整可执行"** 的冒烟验证请用 3.3
> 的目标级参数。

**核心平台化选项**（必须）：

```bash
cmake -S . -B build-android-arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCAESURA_LIVE2D=OFF \
  -DCAESURA_ENABLE_FFMPEG=OFF \
  -DSDL3_DIR="$SDL3_DIR" \
  -DSOLOUD_BACKEND_OPENSLES=ON
```

说明：
- `CAESURA_LIVE2D=OFF` — Cubism SDK 目前仅桌面渲染后端（D3D11/Metal/OpenGL-桌面），
  Android 无预编译 Core/rendering backend，强制关闭。
- `CAESURA_ENABLE_FFMPEG=OFF` — ffmpeg 无 Android sysroot 构建；关闭后走
  `pl_mpeg`（纯软件解码）兜底，不影响 Android 播放 MP4 的渲染管线。
- `SOLOUD_BACKEND_OPENSLES=ON` — **重要**：顶层 CMake 只为 WIN32 开
  WINMM/WASAPI、并强制 `SOLOUD_BACKEND_SDL2 OFF`。Android 音频后端须显式开启
  OpenSLES（本 vendored SoLoud 内含 `opensles/` backend，无 AAudio——见风险 R5）。
- `ANDROID_PLATFORM=android-24`（Android 7.0+）为 SDL3/OpenSLES + SoLoud 兼容基线。

### 3.2 构建

```bash
cmake --build build-android-arm64 --config Release --parallel
```

脚本化：见 `scripts/build_android.sh`（已封装 configure+build，bash、正斜杠路径）。

### 3.3 冒烟验证（仅编译，不产 APK）

```bash
# 目标级 → 把静态模块库全量编译（验证所有 .cpp 能在 NDK clang 下通过）
cmake --build build-android-arm64 --target CaesuraEngine --config Release --parallel
# 或只编译某模块验证关键依赖可编译
cmake --build build-android-arm64 --target CaesuraRender --config Release --parallel
cmake --build build-android-arm64 --target CaesuraAudio --config Release --parallel
```

> `--target` 指定模块/组合目标可在不产可执行、不产 APK 的前提下验证 Android 交叉
> 编译可行性——这正是本机"仅编译不链接完整 APK"的冒烟目标。

### 3.4 打包 APK（暂缺，待设备）

本仓库**不含 JNI/Activity 壳**（同 `mobile-pipeline.md`）。APK 组装需外部 app module：
SDL3 的 `SDLActivity` 继承 + 引擎 .so 放 `jniLibs/<abi>/` + gradle 打包。此步骤无设备
无法验证，标注为"待设备"。

---

## 4. 关键模块在 Android 下的分析

### 4.1 bgfx 渲染后端（GLES）

- bgfx 自带 `glcontext_egl.cpp`（EGL），Android GLES2/GLES3 走 OpenGL backend。
- **引擎渲染后端默认值问题**：`src/render/BgfxDeviceCore.cpp` 中
  `s_preferredBackend` 默认初始化为 `Direct3D11`，且 `setPreferredBackend` 只识别
  `vulkan/dx11/dx12/metal/webgpu/opengl`（无显式 "gles" 字符串）。**在 Android 上必须
  以 `--backend opengl` 启动**（bgfx 在 EGL 平台把 OpenGL backend 落到 GLES），或依赖
  `bgfx::init` 失败后的 `RendererType::Count` 自动回退（不可靠）。建议在
  组合根/文档明确 Android 启动参数为 `--backend opengl`。
- 渲染目标：`--frames N`（已实现，`main.cpp` 提供确定性帧上限）可配合
  `--export-replay` 做渲染冒烟——需真实 GPU 窗口。

### 4.2 SoLoud 音频后端（OpenSL）

- vendored SoLoud（`external/soloud`）backend 选项见 `contrib/Configure.cmake`：
  `SOLOUD_BACKEND_OPENSLES`（Android 正解）、无 `AAUDIO` 选项。
- 引擎 `SoLoudAudioEngine::init()` 以 `SoLoud::Soloud::AUTO` 选后端——只要把
  OpenSLES 编译进去（`-DSOLOUD_BACKEND_OPENSLES=ON`），运行期自动命中。
- 需链接 `OpenSLES` 库（NDK sysroot 自带，脚本已通过 target 链接处理）。

### 4.3 SDL3 平台后端

- `SDL3PlatformBackend` 用 `SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS)`
  + `SDL_CreateWindowWithProperties`。Android 下 SDL3 自动用 android 视频/音频驱动。
- 触摸输入：`MobileAdapter` 已实现 `onFingerDown/Motion/Up`（→ SDL 鼠标事件）、
  `onPinch`（→ 滚轮）、`onLongPress`（→ 右键）、DPI 缩放。但**原生 JNI/Activity
  → MobileAdapter 的接线尚未完成**（`MobileAdapter.cpp` 注释明示"native mobile SDK
  integration is not wired"），需宿主侧桥接。

### 4.4 存档路径

- `SaveManager::init(saveDir)` 接收显式目录，无内置平台路径发现。Android 上应在
  组合根把 `saveDir` 设到应用私有目录 `SDL_AndroidGetInternalStoragePath()`/
  `saves`（无需外部权限）。当前组合根按桌面 CWD 解析 —— Android 端需平台分支。

### 4.5 资源目录

- `main.cpp` 启动时沿 CWD 向上找 `assets/` 并 chdir。Android APK 内资源不在文件系统
  CWD，需改用 SDL3 的 `assets:` URI / `SDL_GetBasePath` / 打包 CARC。当前引擎资源
  加载走文件路径，Android 资产封装（assets:// 或 CARC 挂载）为待办。

---

## 5. 已知风险清单

| ID | 风险 | 影响 | 缓解/现状 |
|---|---|---|---|
| R1 | 顶层把引擎链为可执行文件而非 `.so` | Android 需要 `libCaesuraAmeKAG.so` 供 SDLActivity JNI 加载 | 需新增一个 `MODULE` 库目标包装组合根（或把 main 拆出为库+壳）；属于源码改造，本任务不改源码 |
| R2 | 渲染后端默认 D3D11 | Android 无 D3D11，首帧必失败 | 启动加 `--backend opengl`，或改 `s_preferredBackend` 默认按平台分支（建议源码改造） |
| R3 | `external/SDL3` 是 Windows 预编译包，非源码 | Android 交叉编译找不到 SDL3Config.cmake | 必须提供 `SDL3_DIR` 指 Android 版 SDL3（见 2.2），文档已明确 |
| R4 | `CAESURA_LIVE2D / CAESURA_ENABLE_FFMPEG` 无 Android 构建 | 开启会 configure 失败或链接失败 | 构建命令强制 `=OFF`（脚本已内置），Android 用 pl_mpeg 兜底视频 |
| R5 | SoLoud 无 AAudio，只有 OpenSLES | 音频后端选择受限 | 显式 `-DSOLOUD_BACKEND_OPENSLES=ON`；Android 7.0+ OpenSLES 稳定 |
| R6 | `main.cpp` chdir 找 `assets/`，Android 无 CWD 文件系统 | 资源找不到、启动失败 | 资产封装（assets:// 或 CARC）为待办，需宿主桥接或源码改造 |
| R7 | 存档 saveDir 按桌面 CWD 解析 | Android 上写错目录、无权限 | 组合根需按 `SDL_AndroidGetInternalStoragePath()` 设置 |
| R8 | JNI/Activity 壳缺失 | 无法产可用 APK | 仓库不含壳；APK 组装需外部 app module（待设备） |
| R9 | 链接目标：SystemDependencies 的 `X11` 等桌面库在 Android 无 | configure 阶段报错 | Android 分支需在 `CaesuraSystemDependencies` 增加 `ANDROID` 分支（链接 `OpenSLES`、`EGL`、`GLESv3`、`android`、`log`）；属于源码改造 |
| R10 | bimg SSE4.1（`CMAKE_SYSTEM_PROCESSOR` x86_64 分支） | arm64 无影响，但 x86_64 模拟器 ABI 可能缺 | 仅 x86_64 ABI 有 `-msse4.1`，arm64 不受影响；x86_64 模拟器建议关闭或用 arm64 真机 |
| R11 | 真机行为（触摸/IME/音频/生命周期/DPI）完全未验证 | 高不确定性 | 见第 6 节真机清单（全部"待设备"） |

---

## 6. 真机验证清单（待设备）

> 这些项需要实体 Android 设备或模拟器 + 一个 JNI/Activity 壳。当前无设备环境，
> 全部"待设备"。

- [ ] **APK 构建**：`scripts/build_android.sh` 产 `libCaesuraAmeKAG.so`（arm64-v8a），
      外部 app module `gradle assembleDebug` 产出可安装 APK
- [ ] **`--frames` 冒烟**：真机启动后以 `--frames 60` 跑满一屏不崩溃、无
      `bgfx::init` 失败、日志无 D3D11 相关报错
- [ ] **触摸输入**：单指触摸推进 KAG 对话；长按触发右键/历史；捏合缩放（滚轮模拟）
      生效；DPI 缩放正确
- [ ] **音频**：BGM/VOICE/SE 三总线出声；OpenSLES 后端启动无 `SoLoud_init` 失败；
      来电/切后台暂停-恢复正确
- [ ] **生命周期**：Activity 暂停/恢复循环后状态一致（`onPause`/`onResume` 回调、
      音频 suspend/resume）
- [ ] **IME**（中文/日文）软键盘输入端到端（SDLVN 文本插值 + CJK 字体渲染）
- [ ] **存档**：存/读档写入 `getInternalStoragePath()/saves`，无外部权限告警
- [ ] **资源**：APK assets 或 CARC 资产能被引擎加载（`assets:` URI 或 CARC 挂载）

---

## 7. 本机验证结果

- **NDK 状态**：本机 `ANDROID_NDK_HOME` / `ANDROID_NDK` / `ANDROID_HOME` /
  `ANDROID_SDK_ROOT` 均未设置；常见路径（`$LOCALAPPDATA/Android/Sdk/ndk` 等）
  未发现任何 NDK 版本。
- **结论**：**无 NDK → 跳过真实交叉编译验证**。交付文档 + 可执行脚本
  `scripts/build_android.sh`；脚本在无 NDK 时给出明确报错与下载指引，不会误跑。
- 一旦安装 NDK，先跑 `scripts/build_android.sh`（arm64-v8a），预期首次会卡在
  SDL3 缺失（见 R3）或链接目标缺 Android 分支（见 R9），这些是已知待源码改造点。
