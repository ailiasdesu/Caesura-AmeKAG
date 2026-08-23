# Android 集成参考（Track M A2/A3 — 宿主层与内容打包）

> 状态：**scaffold-less reference；build-verified 已由 CI 达成**（2026-08-24 更新）。
> 本机无 NDK，但 GitHub Actions `android-compile` 探针已全绿：NDK 27.3 + SDL3 3.2.4 +
> OpenSSL 3.3.2 android-arm64 切片 → 引擎 configure → 模块 graph + 可执行 + 测试全链接。
> 本文仍只做**事实清单与执行路径**；**真机/安装层**结论以
> docs/platform/android-device-validation.md 的分层为准（device-unverified 保持）。

## 1. 现状事实（file:line 依据）

| 项 | 事实 |
|---|---|
| 引擎入口 | `src/main.cpp:886` 普通 `int main(int, char**)`；`cmake/CaesuraModules.cmake:43` 定义 `SDL_MAIN_HANDLED`（SDL3 不再生成 main，可安全包装） |
| 输出形态 | 16 个静态模块库 + 可执行目标（顶层 CMakeLists.txt:187 add_executable）——**无共享库**（R1：Android 需 MODULE/SHARED 出 libCaesuraAmeKAG.so） |
| 平台依赖 | `cmake/CaesuraModules.cmake` SystemDependencies **ANDROID 分支已落地**（2026-08-24，R9 闭环：Threads/log/android + OpenSSL 切片路径）；FFmpeg 仅 WIN32（Android 用 pl_mpeg 回退，build_android.sh 强制 CAESURA_ENABLE_FFMPEG=OFF：129-132） |
| 渲染 | bgfx 默认 D3D11（render/BgfxDeviceCore.cpp:12）——Android 设备必须 `--backend opengl`（main.cpp:939 → Engine.cpp:315 开关） |
| 输入 | `platform/MobileAdapter` + `IMobileAdapter`（touch→mouse、long press、pinch、多触点）已实现；MobileAdapter.cpp:3 明示 **原生接线未做**（native mobile integration NOT wired） |
| 生命周期/焦点 | STEP11 LifecycleService + STEP14 IAudioFocusService——Android JNI/iOS 原生回调直接 `post()` 即接入（无需再改 Engine） |
| 资源 | 引擎按 CWD 找 `assets/`（main.cpp:892, R6）——APK 内资产需 SDL3 AssetManager（assets://）或 CARC 归档方案（A2 侧缺口） |
| 内容 | `tests/projects/first_vn`：story.ks（中/日/英内联 + en/ja/zh i18n, story.ks:39-58）+ 自带 assets/{bg,fg,bgm,se} + entry.lua；共享池 assets/ 含 CJK 字体与 lang/*.lua——结构与 `scripts/package_game.sh:145-173` 同构（**A3 打包无缺口**） |

## 2. 执行路径（装好 NDK r26+、Android SDL3 CMake 包后的推荐顺序）

```bash
# 1) 模块库交叉编译验证（预期卡点 R9 会先出现）
bash scripts/build_android.sh --smoke
# 2) R1：顶层目标改共享 → libCaesuraAmeKAG.so；R9：SystemDependencies 加 ANDROID 分支
#    （SDL3::SDL3 + android/log + EGL/GLESv2/OpenSLES + dl/m/z——按 NDK 实测）
# 3) R6：资产访问（SDL3 AssetManager 或 CARC）
# 4) app 模块（参考 SDL3 官方 android-project）：
#    settings.gradle + app/build.gradle + AndroidManifest(org.libsdl.app.SDLActivity)
#    + jniLibs/<abi>/libSDL3.so,libCaesuraAmeKAG.so + assets/（first_vn 内容合成一个 assets 根）
# 5) gradle assembleDebug → adb install → 真机验证（清单见 android-device-validation.md）
```

## 3. 本机可先行验证的（零 NDK）

- 内容链：`bash scripts/verify_first_vn.sh` + `bash scripts/package_game.sh tests/projects/first_vn`（ks_check/bake/内容一致性全通过）
- 脚本契约：`bash scripts/build_android.sh` 无 NDK/CMake/assets 缺失诊断（本会话已验证退出码与文案）
- CI 建议（零 NDK）：android job 增加 bash -n + ks_check + verify_first_vn.sh 静态门禁

## 4. 变更许可（诚实边界）

- 未创建 gradle 工程、未伪造 .so/APK 产物；A2 宿主层代码（R1/R9/JNI）为**共享耦合点**，
  需在有 NDK 可编译验证的轮次实施，避免“只写不验”。

**R6 读胶水（2026-08-24 落地，编译级）**：APK 内游戏包布局为 `assets/game/`
（= 引擎资源根：scripts + assets + demo/<project>，与 demo/ 同构）；`MainActivity`
首启把 game/** 提取到 `filesDir/caesura_root`（.bundle-version 标记防重复提取），
`getArguments()` 返回 `{"--resource-root", root}`——SDLActivity 的 SDLMain 把它
作为 argv 交给 SDL_main，引擎复用 f5dbac5e 的根解析，**引擎侧零改动**。
