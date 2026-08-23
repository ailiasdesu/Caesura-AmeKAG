# Android Device Validation（Track M A4 — 真机验证记录表）

> 状态：**device-unverified**（2026-08-23。本机仅 adb，无 NDK、无真实 Android 设备——任何项在真机验证前一律不得标为已验证）
> 分层语义（计划 §9）：**build-verified** = NDK 交叉编译产物生成；**install-verified** = APK 已安装到设备；**device-verified** = 真机跑通验收项；**release-verified** = 发布级签名/分发产物。

## 环境记录（首次真机验证时填写）

| 字段 | 值 |
|---|---|
| Device model | *（如 Pixel 8 / Xiaomi 14）* |
| Android version | *（如 14 / 15）* |
| ABI | arm64-v8a |
| Build commit | *（git rev-parse HEAD）* |
| APK hash | *（sha256sum app-release.apk）* |
| NDK version | *（装 NDK 后记录，如 r26d）* |
| SDL3 android pkg | *（SDL3_DIR 来源与版本）* |
| 验证人/日期 | *（device-unverified 前不得填）* |

## 验收清单（计划 A4：launch/touch/long press/pinch/portrait-landscape/lifecycle/IME-CJK/save-load/audio/memory）

| # | 项目 | 结果 | 备注（复现步骤/截图/日志） |
|---|------|------|------|
| 1 | launch（APK 安装、启动、title 可见） | ⏳ pending | |
| 2 | touch tap 推进 | ⏳ pending | |
| 3 | long press（右键语义） | ⏳ pending | MobileAdapter::onLongPress → mouse right |
| 4 | pinch 缩放（滚轮语义） | ⏳ pending | submitPointer Pinch 路径 |
| 5 | portrait / landscape 切换 | ⏳ pending | SDL_EVENT_DISPLAY_ORIENTATION → onOrientationChanged |
| 6 | lifecycle（后台/前台、音频中断） | ⏳ pending | LifecycleService / IAudioFocusService 入口 |
| 7 | IME / CJK 输入 | ⏳ pending | 文本输入需 IME 桥（未实现则记录为已知缺口） |
| 8 | save / load（跨重启） | ⏳ pending | AppStorage provider（当前组合根为 LocalFile——Android 需换实现，A2） |
| 9 | audio（BGM/SE/voice） | ⏳ pending | SOLOUD_BACKEND_OPENSLES |
| 10 | memory pressure 基本路径 | ⏳ pending | onLowMemory → _G.onLowMemory |

## 状态矩阵（如实更新，勿越级）

| 层级 | 状态 | 依据 |
|---|---|---|
| script-contract | ✅ | scripts/build_android.sh 单入口 + 清晰错误（本会话验证） |
| build-verified | ⏳ pending | 需 NDK + Android SDL3 CMake 包；命令 bash scripts/build_android.sh --smoke |
| install-verified | ⏳ pending | 需 A2 宿主层（gradle app 模块 + SDLActivity） |
| device-verified | ⏳ pending | 本表清单全部 item |
| release-verified | ⏳ pending | TestFlight/分发链约束 |

## Known Issues（一经真机发现即记录，勿删除历史）

- 当前无（真机验证前）
