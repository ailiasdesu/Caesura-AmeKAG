# Android Device Validation（Track M A4 — 真机验证记录表）

> 状态：**device-verified**（核心项 2026-08-24 真机闭环；未验项如实标注）。
> 分层语义（计划 §9）：**build-verified** / **install-verified** / **device-verified** / **release-verified**。

## 环境记录

| 字段 | 值 |
|---|---|
| Device model | **Xiaomi M2012K11AC (alioth, 小米 11)** [magisk root] |
| Android version | **13 (SDK 33)** |
| ABI | arm64-v8a |
| Build commit | **6e90d7df + 本表提交**（存储系统槽 + FirstVN 设备入口） |
| APK hash | **3072e33a5dc79a4a191042043f9c42468676198b59916d0120b519a6d944c2eb**（android/app/build/outputs/apk/debug/app-debug.apk） |
| NDK version | **r27.3.13750724**（D:\\green\\ndk\\27.3.13750724） |
| SDL3 android pkg | **3.2.4 本地切片（D:\\green\\android-build-src\\sdl3-android）；OpenSSL 3.3.2 同目录 android-arm64** |
| 验证人/日期 | ailias / 2026-08-24（渲染闭环 + FirstVN E2E 设备行走） |

## 验收清单（计划 A4）

| # | 项目 | 结果 | 备注（复现步骤/截图/日志） |
|---|------|------|------|
| 1 | launch（APK 安装、启动、场景可见） | ✅ device-verified | root pm 通道安装；[FirstVN] Loading + [FirstVN] Ready；场景满屏渲染（连续截图一致，无闪烁） |
| 2 | touch tap 推进 | ✅ device-verified | su -c input tap（MIUI 拒绝 adb shell input，须 su 通道）；对话逐页推进（[Click] resumed N tokens 日志 + 截图 diffs） |
| 3 | long press（右键语义） | ✅ device-verified | GestureDetector（本轮）：单指按住 ≥500ms 触发；真机 su -c input swipe 600 500 600 500 900 → logcat [Mobile] Long press -> right click (431, 475) |
| 4 | pinch 缩放（滚轮语义） | ✅ 单元验证 / ⚠️ 真机未注入 | GestureDetector 双指距离比例脉冲（kPinchInitial 0.08 / kPinchStep 0.02）→ MobileAdapter::onPinch→wheel；单测 6/6 覆盖；adb 无多指注入工具，真机 multi-touch 无法自动化——device-unverified（代码路径+单测已闭环） |
| 5 | portrait / landscape | ✅ device-verified | Manifest 锁定 landscape；settings put system user_rotation 0/1 往返切换后进程存活、画面正常 |
| 6 | lifecycle | ✅ device-verified | su -c input keyevent KEYCODE_POWER 熄/亮屏 + KEYCODE_WAKEUP；进程 PID 不变；恢复后继续运行 |
| 7 | IME / CJK 输入 | ✅ CJK 渲染 / ⚠️ IME 无桥 | 双语文本（中/英 CJK 内联）真实渲染（截图复核）；IME 输入桥为已知缺口（当前无输入功能场景） |
| 8 | save / load | ✅ device-verified | [save slot=7] → saves/save_7.json 3,953,246B + [SaveCmd] Saved to slot 7；[load slot=8]（空槽）优雅 miss 路径；**新增**引擎 60s 自动存档 → saves/save_auto.json + [SaveManager] Saved slot -2（本表提交修复系统槽 -1/-2；此前 Slot -2 每 60s 报错且不写文件） |
| 9 | audio | ✅ device-verified | SoLoud 3 总线；[Audio] BGM: assets/bgm/daily.wav + [Audio] SE: …click.wav 日志 |
| 10 | memory pressure 基本路径 | ⚠️ 未触发 | onLowMemory → MobileAdapter 入口存在（Engine appLifecycleWatch），真机未触发低内存事件——记录为未验证 |

## 状态矩阵（如实更新）

| 层级 | 状态 | 依据 |
|---|---|---|
| script-contract | ✅ | scripts/build_android.sh 单入口（本地验证） |
| build-verified | ✅ CI + 本地 | ios/android-compile 探针全绿；本机 NDK r27.3 + SDL3 3.2.4 + OpenSSL 3.3.2 切片 |
| install-verified | ✅ | root pm 通道安装 app-debug.apk（hash 见上） |
| device-verified | ✅ 核心项 / ⚠️ 2 项未验 | 本表 #1-#9；#3 长按 device-verified（本轮派发接线+真机日志）；#4 pinch 单元验证（无多指注入）、#7 IME、#10 低内存未触发 |
| release-verified | ⏳ | A5 签名环境未配置（keystore 由用户持有） |

## Known Issues（真机发现，勿删除历史）

1. **系统槽（quicksave -1 / autosave -2）此前被 SaveManager 0..99 守卫拒绝**：引擎 60s 自动存档每轮报 [STORAGE] [ERROR] Slot -2 out of range 且不写文件（桌面同样触发）。已修复：slotPath 映射 save_quick.json / save_auto.json，守卫 [-2..99]，listSaves 仍只枚举 0..99；真机验证 saves/save_auto.json 生成、错误清零。
2. **选择分支 UI 文本真机未复现**（2026-08-24 FirstVN 行走）：[save] 之后屏幕文字消失（对话/选择按钮均不可见，日志无错误；桌面/headless/web 同款 story 全绿）。疑与 save→text_scene 状态有关，后续排查。记录为 device-open。
3. tmpvar_5 shader 变体编译失败（GLES ESSL 转换非阻塞回退 Normal）——已知非阻塞项，所有 10 程序 READY。
4. （已闭环 2026-08-24）多指/长按派发：新增 platform/GestureDetector（单指长按 ≥500ms + 双指捏合比例脉冲），Engine finger 流接入、每帧 tick 派发到 MobileAdapter::onLongPress/onPinch；长按真机验证，捏合单测覆盖。
