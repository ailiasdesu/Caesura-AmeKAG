# iOS Build 参考（Track I I0 审计结论 / I1 执行路径）

> 状态：**build-verified (CI probe)**（2026-08-24）。机器本机 Windows；iOS 编译由 GitHub Actions macos-latest 探针验证——SDL3 3.2.4 + OpenSSL 3.3.2 的 iOS arm64 切片由探针构建，引擎全模块图（含 bgfx-Metal/SoLoud-CoreAudio/Lua/归档加密）+ carc_pack/lua_cli/CaesuraTests/CaesuraAmeKAG bundle 链接全部通过（commit d5267312..cca6a205 系列，红点链 SDL3→签名→Cocoa→BUNDLE→bundle-id→engine 签名→Lua system()→OpenSSL→切片架构→sysroot 全部闭环）。真机/模拟器（I4）仍待 Mac 硬件。
> 一切设备结论以 docs/platform/ios-device-validation.md 分层为准。

## 1. 事实清单（file:line）

| 项 | 事实 |
|---|---|
| SDL3 | 仓库经 `find_package(SDL3 CONFIG)` 引入（CMakeLists.txt:43；WIN32 默认用 vendored:33-39）；`external/SDL3/SDL3-3.2.0` 仅 Windows x64 预编译（lib/x64/*.dll+*.lib，**无源码无 iOS slice**）；全仓无 SDL3_SOURCE/FetchContent——iOS 必须提供外部 **iOS 版 SDL3 CMake 包**（复用 build_android.sh 的 `SDL3_DIR` 模式） |
| bgfx | vendored 含 **renderer_mtl.cpp**（API v144，defines.h:18；add_subdirectory 于 CMakeLists.txt:135-141）；`--backend metal` 可选（BgfxDeviceCore.cpp:23-24，main.cpp:915-940→Engine.cpp:315-318，失败回退 :76-77）；Metal 嵌入着色器就绪（CaesuraModules.cmake:227；BgfxShaderManager.cpp:148） |
| CMake/Apple | Apple 分支仅桌面向 3 处（CMakeLists.txt:128 SoLoud 排除 APPLE→**COREAUDIO 缺失**、:138 bimg、:344-358 Live2D）；**零 iOS 分支**（无 CMAKE_SYSTEM_NAME/iOS/MACOSX_BUNDLE/Info.plist）；可执行目标 CMakeLists.txt:187 |
| 入口 | `SDL_MAIN_HANDLED`@CaesuraModules.cmake:43（iOS 需改入口适配，同 Android SDL 原生 app 模式） |
| 资源 | main.cpp:891-909 CWD 上探找 assets/ 在 iOS 沙盒失效（下游 main.cpp:60、Engine.cpp:431 相对路径）——需 resourceRoot 注入（SDL_GetBasePath=bundle），与 Android R6 同根因 |
| 复用层 | IMobileAdapter / LifecycleService / IDisplayService / IAudioFocusService 已就绪，iOS 原生回调直接 post() |

## 2. 执行路径（审计建议序 I0→I3→I1→I2→I4）

1. **I3 资源抽象**（✅ 已落地 2026-08-23，commit f5dbac5e）：`--resource-root <dir>` / `CAESURA_RESOURCE_ROOT` 注入（顺序：显式 > 环境变量 > 原 CWD 上探，行为向后兼容；显式根必须含 assets/，否则清晰报错退出 1）。iOS 启动器传 `SDL_GetBasePath()`（bundle），Android JNI 传安装目录——与 Android R6 同一机制；APK 压缩资产仍是 R6 扩展项（SDL3 资产回调 / CARC）。
2. **I1 toolchain（CI 可红绿）**：在 GitHub Actions `macos-latest` 加 `cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS ...` 模块编译步骤（模块图=静态库，无需 bundle）——本机 Windows 无法验，用 CI 判定；预估首个红点=SoLoud COREAUDIO 缺失。
3. **I2 First VN**：`tests/projects/first_vn` 内容与 Android 同一资产根（同构）。
4. **I4 真机/模拟器**：唯一需 Mac 硬件项（**模拟器免签名**；App Store 才需账号）——Metal 冒烟 + 触摸/方向/安全区/audio interruption/save-load/CJK。

## 3. 阻塞与诚实边界

- 最小阻塞 = **Mac/Xcode**（无可绕过；CI macos-latest 可部分代验编译）。
- 签名凭据（I4 TestFlight 前）后置；不提交敏感配置。