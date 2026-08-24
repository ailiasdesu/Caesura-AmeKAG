# Android 运行语义：A2 检查项 → 机制映射（Track M）

> 状态：**设计对照表（未真机验证，2026-08-24）**。真机日按本表逐项实测；
> 每项都给出机制归属与验证动作，避免临时翻代码。验收结果最终写入
> docs/platform/android-device-validation.md（device-verified 铁律）。

| A2 检查项 | 机制归属 | 说明 / 真机验证动作 |
|---|---|---|
| SDL Activity 正确启动 | SDL3 glue：SDLActivity(SDLMain)→`nativeRunMain("CaesuraAmeKAG","SDL_main",argv)` | `MainActivity.getLibraries()={SDL3,CaesuraAmeKAG}`；libCaesuraAmeKAG.so 导出 SDL_main（Android 构建**不定义 SDL_MAIN_HANDLED**，cmake/CaesuraModules.cmake） |
| native library load | `System.loadLibrary` 顺序：SDL3 → CaesuraAmeKAG | 验：首次启动 logcat 无 UnsatisfiedLinkError |
| 生命周期回调 | SDL3 事件 → Engine appLifecycleWatch → LifecycleService(STEP11) → onPause/onResume/音频 suspend | 验：切后台/回前台，BGM 停/续，Lua [wait] 合成 dt 无跳变（W5 同语义） |
| 方向 orientation | SDL3 `SDL_EVENT_WINDOW_OCCLUDED`/display orientation + IDisplayService(STEP10, SDL_GetCurrentDisplayOrientation + dpi=96*contentScale) | manifest configChanges 已含 orientation；验：旋转后逻辑尺寸/朝向一致 |
| 全屏/沉浸 | SDLActivity 系统 UI 隐显（mHasFocus/setSystemUiVisibility）；SDL3 window fullscreen flags | 验：无状态栏遮挡；ExitFullscreen 手势后恢复 |
| 返回键/手势语义 | Android back → SDL3 back key 事件；`SDL_HINT_ANDROID_TRAP_BACK_BUTTON`（默认 0=回调给 app） | 验：返回键=app 内取消/回退，非直接退出；从应用内退出回桌面 |
| 输入/触摸 | SDL3 touch→SDL_EVENT_FINGER_*；引擎 IInputRouter/IMobileAdapter(STEP12)：tap/long-press/pinch/多触点 | 验：长按=菜单，双指缩放 demo 场景（minigame/地图） |
| IME/CJK | SDL3 `SDL_EVENT_TEXT_INPUT`(软键盘)；字体走 assets 字体栈 | 验：中文/日语输入到文本框；CJK 渲染无豆腐（W3 同断言，字体已入 assets/fonts） |
| 存档/读档 | SaveManager(STEP13) 默认 LocalFileSaveProvider → getFilesDir()/caesura_root 同目录（资源根旁） | 验：存→毁进程→读；槽 0..99；配额失败诚实拒绝 |
| 音频 | SoLoud OpenSLES（SOLOUD_BACKEND_OPENSLES，Android 强制）；SDL 音频焦点 → IAudioFocusService(STEP14) | 验：三总线出声；来电/其他 app 聚焦暂停-恢复 |
| 内存压力基本路径 | SDL_EVENT_LOW_MEMORY → LifecycleService onLowMemory(STEP11)；TextureBudget 降档 | 验：开发者选项强制低内存，无崩溃 |

## 资源根（R6，已桌面实证）

APK `assets/game/**`（scripts+assets+demo/<proj>）→ `MainActivity` 首启提取到
`filesDir/caesura_root`（`.bundle-version` 标记）→ `getArguments()` 传
`--resource-root <root>` → SDL_main argv（引擎 f5dbac5e 根解析）。
已验证：Windows 本地 + Linux CI xvfb 双端真实启动（scripts/verify_bundle_boot.sh，
first_vn + demo 两包）。**APK 提取→SDL_Init 联动链真机未验**。

## 真机日命令速查

```bash
# 从 CI 拉 debug APK（跑一次 android-compile 探针后用 run id 取件）
#   gh run download <run-id> -n android-apk?（探针未单独上传件→改用探针日志路径）
# 更稳：本地/CI 置 CAESURA_ANDROID_KEYSTORE 均不需要——直接 install debug 包：
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb logcat -s SDL:V CaesuraAmeKAG:V | grep -iE "SDL|error|fatal"
```

## 诚实边界

- 本表是**设计映射**，不是验证记录；任何一项未经真机不得写入 「verified」；
- manifest/沉浸/返回键依赖 SDL3 宿主行为，升级 SDL3 版本需回归本表。