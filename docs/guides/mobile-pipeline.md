# 移动端管线（Mobile Pipeline）

> P0-3（发布前必须）：MobileAdapter 已接入引擎（P7 核心映射），本指南
> 覆盖 Android 交叉构建、输入/IME 对接、生命周期桥接与已知限制。
> **状态**：构建脚本就绪；真机验证（IME、触摸映射、DPI、Activity
> 生命周期）未覆盖——见文末"待真机验证"。

---

## 1. 构建（Android）

前置：Android NDK r23+（`ANDROID_NDK_HOME`）、Android SDK（`ANDROID_HOME`）、
CMake 3.25+。SDL3 需在 `thirdparty/SDL` 且启用 Android backend。

```bash
export ANDROID_NDK_HOME=/path/to/ndk
export ANDROID_HOME=/path/to/sdk

# 交叉编译引擎共享库（默认 arm64-v8a / Debug）
scripts/android_build.sh

# 指定 ABI / Release
scripts/android_build.sh --abi x86_64 --release
```

产物：`build-android-<abi>/src/libCaesuraAmeKAG.so`

> 配置硬开关：`CAESURA_ENABLE_FFMPEG=OFF`（ffmpeg 无 Android sysroot 构建）、
> `CAESURA_LIVE2D=OFF`（Cubism SDK 桌面专用）。引擎按
> `SDL_VIDEO_DRIVER=android` 走 SDL3 的 Android 后端。

## 2. APK 组装

本仓库不含 JNI/Activity 壳——应用模块需：

1. 创建 Android app module，主 Activity 继承 SDL3 的 `SDLActivity`
2. 把 `libCaesuraAmeKAG.so` 放入 `jniLibs/<abi>/`
3. 在 Activity 的 `onCreate` 后加载引擎，把触摸/生命周期事件路由到
   `IMobileAdapter`（见下）
4. `gradle assembleDebug` 产出 APK

## 3. IMobileAdapter 对接（引擎侧已实现）

接口：`src/platform/api/IMobileAdapter.h`，实现 `MobileAdapter`（
`src/platform/MobileAdapter.cpp`）。宿主（Activity/原生层）调用：

| 事件 | 方法 | 引擎行为 |
|---|---|---|
| 生命周期暂停 | `onPause(L)` | 调用 Lua `onPause` 回调、暂停音频/调度 |
| 生命周期恢复 | `onResume(L, savedData)` | 恢复 Lua `onResume`、重建状态 |
| 单指按下 | `onFingerDown(x, y, id)` | 归一化坐标 → SDL 鼠标按下事件 |
| 单指移动 | `onFingerMotion(x, y, id)` | → SDL 鼠标移动 |
| 单指抬起 | `onFingerUp(x, y, id)` | → SDL 鼠标抬起 |
| 双指捏合 | `onPinch(cx, cy, scale)` | 缩放手势（供 UI 层消费） |
| 长按 | `onLongPress(x, y)` | 长按手势 |
| 分辨率缩放 | `getDisplayScale()` | DPI 缩放系数（画布缩放） |

SDL finger 事件桥（`SDL_FINGERDOWN/MOTION/UP` → 像素坐标）与方向变化
（`onOrientationChanged`）已实现并单测覆盖（87 项）。

## 4. IME（输入法）对接

SDL3 Android 的文本输入：

```cpp
// 请求软键盘（引擎侧或宿主侧皆可）
SDL_StartTextInput(window);
// 引擎侧 Lua：backend.* 绑定可触发；宿主侧：native 方法直达
```

- **中文/日文输入**：SDL3 Android 用 `SDL_SendKeyboardText` 把 IME
  合成串逐字符送入——引擎的 KAG 文本插值（`${...}`/`%var%`）对
  任意 Unicode 安全；CJK 字体由 FreeType + NotoSansCJKsc 渲染
- **输入焦点**：`InputRouter`（KAG ↔ Game 焦点切换）在移动端沿用
  同一路由；软键盘弹出时 Activity 需把 `onWindowFocusChanged` 传给
  引擎（`SDL_HINT_ANDROID_...` 见 SDL3 文档）
- **待验证**：真实软键盘弹出/收起与 VN 点击推进的焦点竞争

## 5. 资源与存档

- 资源：APK `assets/` 目录直接挂载（`SDL_AndroidGetInternalStoragePath`），
  或打包为 CARC（推荐：加密+签名，见 `carc-packaging.md`）
- 存档：引擎存档写 `SDL_AndroidGetInternalStoragePath()/saves/`
  （应用私有目录，无需权限）；云存档（HTTP/Steam）同桌面

## 6. 待真机验证（P0-3 剩余）

- [ ] IME 中文/日文软键盘输入端到端
- [ ] 触摸 → KAG 点击推进（含滚动/长按手势）
- [ ] Activity 暂停/恢复循环（来电、切后台）状态一致性
- [ ] 不同 DPI 档位渲染缩放
- [ ] 硬件按键（返回键 → `[history]` 或退出确认）

> 这些项需要实体设备或 Android 模拟器，本仓库 CI 无 Android runner——
> 构建脚本与对接文档已就绪，验证留待有设备环境。
