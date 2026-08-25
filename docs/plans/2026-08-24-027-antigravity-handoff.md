# Caesura (AmeKAG) 交接文档 — Antigravity 后续开发

> 交接时间：2026-08-24 · 交接方：caesura（DSH 会话） · 接收方：Antigravity
> 状态基准：master HEAD = `c6170e39`（推送至 GitHub ailiasdesu/Caesura-AmeKAG）
> 本文件是当前权威现状 + 全部铁律；开发前必读 AGENTS.md（模块边界宪章）与 CLAUDE.md（操作手册）。

---

## 0. 项目定位（一句话）

现代化 KAG Runtime / 跨平台视觉小说引擎（C++20 + bgfx + SDL3 3.2.4 + SoLoud + Lua 5.4），16 个静态模块库 + 31+ 纯虚接口，组合根 `src/main.cpp` + `src/entry/`，BackendRegistry 依赖注入。产品化总任务书见 `docs/plans/audit/Caesura-AmeKAG_产品化推进总任务书.md`（Phase0 稳定 → Phase1 Studio → Phase2 分发 → Phase3 第三方验证）。

---

## 1. 铁律（每条都踩过坑，违反必出事）

### 1.1 开发环境与命令
1. **只能使用 git-bash 作为 shell**（本会话工具显示名是 pwsh，但执行后端实际是 git-bash）：命令一律 bash 语法 + 正斜杠路径。反斜杠路径会被吃掉（`external\lua\lua.exe` 变成 `externallualua.exe`）；PowerShell 语法与 `>` 重定向行为不适用；子进程 argv[-1] 会被解析成带括号/空格的绝对路径，cmd 分词会截断（需 `call "..."` 包裹）。
2. **PowerShell `>` 重定向默认 UTF-16LE**：生成 markdown 会写坏文件（read 报 binary、Lua find 失效）——用 `Set-Content -Encoding utf8` 或 python 写。
3. **tools.write 大文件（约 64KB+）会截断**（Engine.cpp 1500+ 行曾全量 write 两次截断致 C1075）——只允许 tools.edit 增量修改 entry/Engine 等大文件；其他大文件也优先 edit。
4. **git commit -m 的消息若含 `${...}` 会被 shell 参数展开**（bad substitution 整条命令失败）——提交信息用 bash 单引号包 -m 或写 $ 转义。
5. **Lua 语言限制**：表构造器不能直接索引（`{1,2}[1]` 语法错误，需 `({1,2})[1]`）；插值表达式 `${ {a=1}.a }` 求值前须给前导构造器加括号。
6. **【铁律·用户明令】DSH 宿主进程就是 node.exe（npx @deepseek-ai/dsh web 及其子进程）——绝对禁止 `taskkill //F //IM node.exe` 或 `taskkill //IM node.exe` 杀所有 node 进程**，会把 DSH 宿主连同当前会话一起杀掉（表现：工具调用反复 interrupted、run_code 报 abort）。清理残留 node 进程必须：先用 powershell/python 按 CommandLine 精确识别目标（vitest/其他工具的进程），再按 PID 逐个 kill（taskkill //F //PID <pid>），且动手前先确认该 PID 不属于 DSH（命令行含 dsh/npx-cli.js @deepseek-ai/dsh 的绝不能杀）。

### 1.2 代码规范（AGENTS.md 摘录，务必遵守全文）
1. **模块边界铁律**：每个模块只能通过 `api/` 子目录对外暴露符号（`I*.h` 纯虚接口，无数据成员）；禁止模块间直接 include 具体实现头；唯一例外 = `src/entry/` + `src/main.cpp`（组合根）。
2. **BackendRegistry 唯一访问点**：所有后端访问 `BackendRegistry::instance().getXxx()`；子系统用 `set*()` 注册；禁止绕过（`TextureManager::instance()`、`LuaManager::instance()` 均禁止）。
3. **组合根禁止扩散**：禁止在非组合根位置 `new`/`make_unique` 具体后端类型。
4. **新增 kag 模块必须登记进 kag/init.lua 预加载清单**（sandbox require 只认 package.loaded：`scripts/sandbox.lua` 清除 package.searchers 并把 require 换成 package.loaded-only 包装；帧/点击回调里运行时 require 的 kag 子模块——如 kag.snapshot、kag.text_scene——必须预载，否则报 `Sandbox: module "xxx" not preloaded` 且功能静默失效）。
5. **引擎沙箱 io.open 白名单**（scripts/assets/tests/demo 前缀）决定 RPC Lua 侧文件读取。
6. **修改接口后必须重跑 `python scripts/api_stats.py`** 并提交 docs/api/api-stats.md（CI 用 git diff --exit-code 校验）；生成器唯一权威。
7. **全量门禁（每个开发轮次）**：桌面构建零错误 → C++ doctest 全绿（当前 1027 用例）→ Lua 主套件（132 文件，external/lua/lua.exe tests/scripts/run_lua_tests.lua）→ 孤儿套件（24 文件，run_orphan_tests.lua）→ 耦合预算（python scripts/count_coupling.py：entry/di/script ≤14，其余 ≤4）→ 变更涉及接口时 api-stats。
8. **命名**：模块目录全小写 + 与 git 索引大小写一致；接口 I 前缀 PascalCase；实现 PascalCase；命名空间 Caesura::。
9. **工作流程（用户约定）**：契约由主 agent 定（api/I*.h、RPC 结构、共享耦合点 main.cpp/BackendRegistry/kag init/tests CMakeLists）；独立任务拆成互不重叠文件集，一次扇出 4-8 个子 agent 后台并行（不跑全量门禁、不提交、不碰主 agent 独占文件）；主 agent 统一收敛门禁 + 分语义提交。等待 CI 期间必须并行做其他工作，不干等；多轮迭代本地累积语义提交（每轮仍跑本地全量门禁），到目标节点统一 push + 一次 CI。

### 1.3 Web/发布链路铁律（Track W 事实）
1. engine 资源路径带 assets/ 前缀，web 须归一化为 /assets/<path>（曾 /assets/assets/ 双前缀 404）。
2. bridge.js 启动硬依赖 scriptsBase+index.json（404 即 boot 挂）：dev=vite 中间件、vite build=closeBundle、打包=package_game.sh+gen-index；web/scripts-index.json 为提交工件（新增 scripts/*.lua 后必须 `node web/gen-index.mjs` 并提交）。
3. wasmoon 默认从 unpkg 拉 glue.wasm（离线首启挂）——打包产物用本地 web-assets/glue.wasm + index.html 内联 pin `__CAESURA_WASM_FILE__`。
4. 浏览器冒烟：`bash scripts/package_game.sh tests/projects/first_vn && node scripts/web_browser_smoke.mjs --root dist/first_vn [--browser edge] [--unlock] [--cjk|--stress|--suspend|--subpath games]`；Chrome headless 默认 autoplay=suspended（需手势解锁），Edge 默认放行；端口默认自动挑选（别固定 9333/8765，残留浏览器占端口时新实例静默消失）。清残留浏览器进程：powershell -File build/web-smoke/kill-leftovers.ps1（按 cmdline 匹配 web-smoke，勿动 node）。
5. 移动端/后台：web scheduler 用合成 dt（不读墙钟），suspend 验证用 --suspend；WebAudio 后台保持播放。

---

## 2. 当前已完成（2026-08-24 快照）

### 2.1 渲染（安卓真机闭环）
- **分辨率体系**：引擎默认 1920×1080（全平台，含 --resolution WxH 可调）；`config.lua` window_width/height 默认 1920×1080。
- **viewport 布局**：`scripts/viewport.lua`（逻辑分辨率助手，1920×1080 缺省弃用 1280×720 硬编码）：bg/fg 层、消息框（y=vh-200, x=0, w=vw）、对话坐标（msgY=vh-140、nameX=vw/2-100、right 对齐 vw-248/496、NVL 宽 vw-96）、全部 UI 菜单（settings/gallery/music_room/saveload_menu/chapter_select/title_menu/history_ui/system/demo 入口）已 viewport 化。
- **每帧提交**：`scripts/layers.lua` renderNode 对可见节点每帧提交 quad（原只提交 dirty→静态层首帧后 batch 空→黑屏/闪烁；dirty 只管 RTT 懒分配）。
- **present-size**：`IRenderDevice::setPresentSize` + BgfxDeviceCore 用 backbuffer 尺寸设置 VIEW_MAIN/DEBUG rect、投影保持逻辑坐标（Android surface 2320×956 ≠ 逻辑 1920×1080 → 拉伸填满屏）；Engine::init 经 SDL_GetWindowSizeInPixels 注入。
- **横屏锁定（最新 c6170e39）**：Android 分支窗口创建前 `SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft,LandscapeRight")`——manifest screenOrientation=landscape 不够，SDL3 会在未设 hint 时跟随系统自动旋转；真机验证（accelerometer_rotation=1 + user_rotation 1 + `wm user-rotation lock 1` 强制旋转下仍固定横屏）。

### 2.2 安卓真机链路（A4 已验证项）
- 设备：Redmi K40（M2012K11AC，haydn，骁龙 870 / Adreno 650）/ Android 13 / arm64-v8a / Magisk root；本地工具链：NDK r27.3（/d/green/ndk/27.3.13750724）、SDL3 3.2.4 安卓切片（/d/green/android-build-src/sdl3-android）、OpenSSL 3.3.2 同目录、JDK17 /d/green/android-tools/jdk-17.0.20+8、Gradle 8.9 + JDK、Ninja、SDK root /d/green（platform-35、build-tools 35.0.0、aapt2）。
- **本地安卓循环（权威）**：① `cd build-android-arm64-v8a && /d/green/android-tools/ninja.exe`；② cp libCaesuraAmeKAG.so → android/app/src/main/jniLibs/arm64-v8a/；③ `cd android/app && gradle assembleDebug`（**必须 --rerun-tasks**，否则偶发 200MB 陈旧 APK）；④ adb push app-debug.apk /data/local/tmp + `su -c pm install -r`；⑤ 重启 `su -c am force-stop com.caesura.app` + `am start -n com.caesura.app/.MainActivity`。
- **注意**：ninja 修改后可能 stale（no work to do）——确认 .o 已重编或删对应 CMakeFiles/*.dir/xxx.o；gradle 需 ANDROID_HOME=/d/green、JAVA_HOME、PATH 加 gradle/JDK bin、`android.overridePathCheck=true`（仓库路径含中文+括号）。
- **真机部署铁律**：adb push 用 Windows 路径（如 D:/green/device-tmp/x，不能用 /d/...）；su 通道推文件后**必须 chown u0_a242:u0_a242**（root:root 600 会让 app 读不到→Lua module not found/静默回退）；MIUI 拒绝 `adb shell input`（uid2000 无 INJECT_EVENTS）——**必须 `su -c input tap/swipe/keyevent`**；screencap 用 `adb exec-out screencap -p`；日志 `adb logcat -d | grep engine-stderr`。
- 设备 bundle：/data/user/0/com.caesura.app/files/caesura_root（CWD=这里，scripts/demo/assets/first_vn/saves/logs）；改脚本直接 su cp + chown（无需重装 APK）；entry 由 scripts/config.lua `entry_script` 决定（当前 `../demo/first_vn_entry.lua`，上报制留 `demo/entry.lua` galgame 演示）。

### 2.3 存档 / 生命周期 / 手势 / Web
- **系统槽**：quicksave -1 → save_quick.json、autosave -2 → save_auto.json（devices 当前 62 个文件；修复后 60s 自动存档落盘 + 错误清零）；普通槽 0..99；listSaves 只枚举 0..99。
- **截图两阶段**：captureThumbnailPNG 不再帧内 pump bgfx::frame()（双 present 隐患；存档 3.95MB→2.4KB），requestScreenShot 挂下一帧、下一次保存读取上一帧产物。
- **手势派发（platform/GestureDetector）**：单指长按≥500ms（once）+ 双指捏合比例脉冲（0.08/0.02 阈值）→ Engine 每帧 tick → MobileAdapter::onLongPress（右击）/onPinch（滚轮）；长按真机验证，捏合单测覆盖。
- **A4 验证记录**：docs/platform/android-device-validation.md（环境/清单/状态矩阵/Known Issues，含长按 device-verified、pinch unit-verified、IME/低内存未验证）。
- **Web**：FirstVN 浏览器冒烟 12/12、--cjk 20/20、web vitest 318/318、save/choice 回归测试（web/save-choice-regression.integration.test.js）通过；web 布局守卫（layer rects ⊆ #stage）已入冒烟。

---

## 3. 未完成 / 下一步（按优先级）

1. **【P0·已定位】文本可见性**：设备默认 8×16 位图字体（loadTTF 从未接线、loadCjkAtlas 纯死代码）→ CJK 字符是空槽（不可见）。实验：TTF 图集是 **R8**，GLES 文本着色器（fs_texture.sc = texture2D 直通）采样不兼容 → 连 ASCII 也全不可见（已回退）。**修复路线**：TTF 图集改 RGBA8（r=g=b=255, a=coverage，与位图图集一致）+ 引擎启动接线默认 Noto（assets/fonts/NotoSansCJKsc-Regular.otf，loadTTF(path, 22)）+ 真机验证 ASCII/CJK 全可见 → 再复核选择分支文本（届时应自然可见）。
2. 【P1】A5：release APK / AAB / signing 工作流文档（keystore 用户持有）。
3. 【P1】Track I iOS：I0-I3 按 CI 红绿（iOS 探针方案见 memory/上文事实；模拟器项需 Mac）。
4. 【P2/缺口】IME 输入桥（当前无输入功能场景）；内存压力（onLowMemory）真机未触发；pinch 真机多指注入（adb 无工具，可用 sendevent 或物理手测）。
5. 【文档】渲染/真机闭环的 docs/plans 交接（本文件）+ 后续里程碑文档。

---

## 4. 关键环境事实速查

- Android：`后台进程日志=adb logcat -d | grep engine-stderr`；游戏分辨率链：config.window_width(1920) → BgfxDeviceCore m_width(逻辑) ；真机 surface 2320×956（SDL_GetWindowSizeInPixels 注入 present-size）；bgfx 外部 EGL 上下文（SDL_GL_CreateContext → platformData.context），VIEW rect=backbuffer。
- 构建输出：桌面 build/Debug/CaesuraAmeKAG.exe + build/tests/Debug/CaesuraTests.exe（CWD=build/tests/Debug 跑测试）；安卓 APK android/app/build/outputs/apk/debug/app-debug.apk。
- 测试盲区：headless/桌面 D3D 无法复现 GLES 文本问题；web 玩家是 DOM 渲染（≠桌面 GPU 路径）。
- CI：三平台 CI（.github/workflows/ci.yml）；GH 已登录（ailiasdesu），`gh run view --job <jobId> --log-failed` 取失败日志；本仓库 Actions 并发=串行（~17m 排队）；ios/android-compile 探针 continue-on-error。

---

## 5. 建议接手第一步

跑一遍 `docs/platform/android-device-validation.md` 的复现清单确认基线，然后按 §3.1 修文本图集（RGBA8 + 接线 TTF）→ 真机全流程（对话/选择/存档/自动存档）截图留档 → 更新 A4 文档 → 提交推送。

> 更多历史记忆（约 30 条项目事实/踩坑，含 iOS/Web/宏系统/存储等）在 DSH memory（key 轨），交接文档已摘录与本项目推进强相关部分；需要检索原库请查看 ~/.dsh 或本项目 docs/（solutions/、plans/）。