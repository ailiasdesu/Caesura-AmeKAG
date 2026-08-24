# Mobile Platform Status（Track M / Track I — 执行计划 §12-§20 状态快照）

> 生成：2026-08-24（round 38）。目的：让任何后续会话/Agent 一眼接手。
> 分层纪律（计划 §9/§17）：build-verified ≠ device-verified；无真机不得升格。

## 一句话现状

**计划 STEP 1-16、18、20（web/platform/android build/APK/iOS build/matrix）已闭环；
STEP 17（Android 真机）与 STEP 19（iOS 真机）为硬件门禁，等待设备。**

## STEP 对照

| STEP | 内容 | 状态 | 证据 |
|---|---|---|---|
| 1-8 | Web Audit→Release Gate | ✅ RC-READY | docs/release/web-release-checklist.md 14/14；web_browser_smoke（Chrome/Edge/子路径/离线/压力/挂起）；vitest 318/318 |
| 9-14 | Platform Services (P0-P5) | ✅ | 5 个抽象全部落地：IDisplayService/ILifecycleService/IInputRouter/Storage(STEP13)/IAudioFocusService；接口 31→34；桌面/Web 回归零（C++ 1020/1020、Lua 133+24、coupling PASS） |
| 15 | Android Build | ✅ CI build-verified | android-compile 探针：NDK 27.3 + SDL3 3.2.4 + OpenSSL 3.3.2 android-arm64 切片 → 全量 module graph + 测试链接（红绿 2 红点：SystemDeps ANDROID 分支、executeDebug 显式实例化） |
| 16 | Android APK | ✅ 编译/打包级 | 宿主模块 android/（SDL3 3.2.4 java glue 镜像 11 文件 + MainActivity）+ gradle assembleDebug/assembleRelease（A5 签名 env 驱动）；探针 zip 校验 story.ks/entry.lua/kag/init.lua/so 均入包 |
| 17 | Android Real Device | ⏳ 待设备 | A4 清单=docs/platform/android-device-validation.md；运行语义映射=docs/design/android-runtime-semantics.md（真机日对照表）；R6 胶水编译级（APK 提取→SDL_main 链路未验） |
| 18 | iOS Build | ✅ CI build-verified | ios-compile 探针全绿：SDL3 3.2.4/OpenSSL 3.3.2 iOS slice + SDK sysroot、Metal/QuartzCore/UIKit 分支、xcodebuild arm64 全部 bundle 目标编译链接（11 红点后全绿） |
| 19 | iOS Real Device | ⏳ 待 Mac+设备 | I3 清单=docs/platform/ios-device-validation.md；I2 需真机/模拟器；I4 TestFlight 打印于真机稳定后 |
| 20 | Matrix | ◐ 保持 | docs/release/cross-platform-matrix.md：三平台+Web 实列；Android/iOS 仅 Build/link 列有记录，Boot/Text/… 均 ?（诚实未升格） |

## 捆包布局契约（A3/R6，跨平台共享）

- 规范资源根 `<root>/{scripts,assets,<game>}` + `config.entry_script` → 项目入口；
- `--resource-root`/`CAESURA_RESOURCE_ROOT`/CWD 上探（f5dbac5e）；
- 门禁 `scripts/verify_bundle_boot.sh`：first_vn + demo 双包，Windows 本地 + Linux CI xvfb 真实启动断言（[FirstVN] Ready / [Demo Entry] …active）；
- Android 读胶水：MainActivity 提取 APK assets/game/** → getArguments() 传 --resource-root（compile-level only）。

## 硬件解锁清单（给用户/设备日）

```text
Android 真机（ARM64, USB 调试）:
  adb install android/app/build/outputs/apk/debug/app-debug.apk  ← 探针产物
  logcat -s SDL:V CaesuraAmeKAG:V；按 runtime-semantics.md + device-validation.md 逐项
macOS + Xcode:
  # 模拟器（免签名先跑）：xcodebuild -project … -scheme … -destination simulator + --backend metal --frames 60
  # 真机：按 docs/platform/ios-device-validation.md
```

## 已知问题/边界（诚实）

- Android/iOS 的 Boot/Text/Audio/… 矩阵行仍为 ?——不写 device-verified；
- iOS I4 签名/TestFlight 文档待真机稳定；
- 本机无 NDK（迭代靠 CI 探针）；无 Mac；无 Android 设备——三项本轮均确认未提供。