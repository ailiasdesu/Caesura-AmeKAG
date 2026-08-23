# Cross-platform Regression Matrix（计划 §8 / STEP 20）

> 状态：draft（2026-08-23）。规则：`?` 必须通过真实验证后才可改为 ●；永远不将 build-only 写成 device-verified。
> 图例：● 已实机/浏览器验证 · ◐ 仅编译/脚本验证 · ? 未验证 · N/A 不适用（桌面无方向/安全区语义）

| Capability | Windows | Linux | macOS | Web | Android | iOS |
|---|---:|---:|---:|---:|---:|---:|
| Boot | ● | ● | ◐ CI 编译 | ● RC-READY(W8) | ? | ? |
| Text | ● | ● | ◐ | ● | ? | ? |
| CJK | ● | ● | ◐ | ● | ? | ? |
| Image | ● | ● | ◐ | ● | ? | ? |
| Input | ● | ● | ◐ | ● | ? | ? |
| Audio | ● | ● | ◐ | ● | ? | ? |
| Save/Load | ● | ● | ◐ | ● | ? | ? |
| Lifecycle | ● | ● | ◐ | ●(W5) | ? | ? |
| Orientation | N/A | N/A | N/A | ?(smoke 未测旋转) | ? | ? |
| Safe Area | N/A | N/A | N/A | N/A(桌面浏览器) | ? | ? |
| Stress | ● | ◐ | ? | ●(W4) | ? | ? |
| Build/link (CI) | ● | ● | ● | ● | ✅ android-compile 探针全绿 (2026-08-24, NDK 27 arm64-v8a + SDL3 3.2.4 + OpenSSL 3.3.2 切片；模块库+可执行+测试全链接) | ✅ ios-compile 探针全绿 (2026-08-24) |

## 证据锚（全部本会话实测/门禁）

- **Windows**：C++ 1015/1015（315871 断言）+ ctest；桌面 Demo/首 VN 冒烟；Live2D/Steam 关（配置矩阵）；Release CPack 路径（历史）。
- **Linux**：CI 三平台绿 + WSL 实机 ctest 11/11（历史 025）；本机 M 无实机 Linux 显卡。
- **macOS**：CI Clang 严格编译通过（历史修复 3 项 clang-strict 后 2m12s）；**无 Mac 真机** → Boot/Text 等仅 ◐（诚实：不得升 ●）。
- **Web**：**RC-READY**（Track W 全部闭环，见 docs/release/web-release-checklist.md 14/14；Chrome/Edge 全模式实测；子路径/离线/压力/挂起恢复均验）。
- **Android**：script-contract ✅ + **CI build-verified（A1）**：`.github/workflows/ci.yml` 的 `android-compile` 探针（GH ubuntu NDK 27.3 + libc++/lld）构建 SDL3 3.2.4 与 OpenSSL 3.3.2 android-arm64 切片后完成引擎 configure + 全量 module graph、CaesuraAmeKAG 可执行与 CaesuraTests 链接（5m35s，红绿循环推进 2 个红点：SystemDeps X11→ANDROID 分支、executeDebug NDK 显式实例化）。**仍 ⏳ 真机验证**（docs/platform/android-device-validation.md 待设备）。
- **iOS**：CI 编译同理 macOS；真机/构建链审计中（Track I I0）。

## 更新规则

1. 任何升 ● 必须在相应 docs/platform/*-validation.md（或 web-release-checklist）出现对应记录。
2. 仅 CI 编译通过 → 只记 ◐（build-verified 语义），不得写 ●。
3. 每轮更新附「哪个 commit/日期/谁验证」。

> 下一步：Android 按 A1(完成)→A2/A3/A4 推进（NDK 仅 CI 有，本机无）；iOS 按 I1(完成)→I3→I2→I4（Mac 硬件门禁）。