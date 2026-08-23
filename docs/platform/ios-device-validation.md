# iOS Device Validation（Track I I4 — 模拟器/真机验证记录表）

> 状态：**device-unverified**（2026-08-23；无 Mac/真机）。与 android-device-validation.md 同构。
> 分层：build-verified（Xcode 编译出 .app）· install-verified（模拟器/真机安装）· device-verified（验收项跑通）· release-verified（Archive/TestFlight/分发）。

## 环境记录（首次验证时填写）

| 字段 | 值 |
|---|---|
| 机型/模拟器 | *（如 iPhone 15 Pro / iPhone 16 simulator）* |
| iOS 版本 | *（如 17.5 / 18.x）* |
| 架构 | arm64 |
| Build commit | *（git rev-parse HEAD）* |
| Bundle id | *（com.caesura.dev 等）* |
| Xcode 版本 | * |
| 验证人/日期 | *（device-unverified 前不得填）* |

## 验收清单（计划 I3：launch/touch/orientation/safe area/notch/lifecycle/audio interruption/save-load/CJK/memory）

| # | 项目 | 结果 | 备注 |
|---|------|------|------|
| 1 | launch（bundle 启动、title 可见） | ⏳ pending | |
| 2 | touch tap 推进 | ⏳ pending | |
| 3 | orientation（portrait/landscape） | ⏳ pending | |
| 4 | safe area / notch | ⏳ pending | IDisplayService.safeArea |
| 5 | lifecycle（后台/前台） | ⏳ pending | LifecycleService 入口 |
| 6 | audio interruption | ⏳ pending | IAudioFocusService 入口 |
| 7 | save / load | ⏳ pending | iOS AppStorage provider（组合根换行） |
| 8 | CJK rendering | ⏳ pending | 系统字体栈（W3 经验复用） |
| 9 | memory pressure 基本路径 | ⏳ pending | onLowMemory |
| 10 | Metal 渲染冒烟 | ⏳ pending | --backend metal |

## 状态矩阵

| 层级 | 状态 | 依据 |
|---|---|---|
| audit（I0） | ✅ | docs/guides/ios-build.md（SDL3 iOS 包/bgfx Metal/CMake 缺口/资源根） |
| build-verified | ⏳ pending | CI macos-latest 加 iOS 模块编译步骤（I1 红绿判定） |
| install-verified | ⏳ pending | 需模拟器/真机 |
| device-verified | ⏳ pending | 本清单全部 item |
| release-verified | ⏳ pending | Archive/TestFlight（凭据后置） |

## Known Issues

- 当前无（验证前不应断言；审计缺口见 ios-build.md §1——SoLoud COREAUDIO、SDL 入口、CWD 资源根为已知工程项）
