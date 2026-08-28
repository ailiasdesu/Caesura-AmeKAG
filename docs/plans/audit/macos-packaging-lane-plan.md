# macOS 打包通道实施方案（Sprint6-L2）

> 状态：设计文档（零代码改动）。基于 2026-08-29 工作树 HEAD=5d44b0a5 实际读取的文件证据。
> 范围：Windows ZIP 通道已验证（verify_release_package.sh 29/29）；Linux TGZ 已提交待 CI 首验；本文档设计 macOS 通道。

## 0. 证据地图（所有结论的来源，均可复读验证）

| 编号 | 事实 | 证据位置 |
|---|---|---|
| E1 | CPack 已按平台分支：WIN32->ZIP，其他->TGZ（注释：tar preserves the executable bit ... covers the future macOS lane with zero further CMake churn） | CMakeLists.txt:548-556 |
| E2 | 包名公式统一：CaesuraAmeKAG-<v>-<CMAKE_SYSTEM_NAME>-<CMAKE_SYSTEM_PROCESSOR>（macOS 预期 = CaesuraAmeKAG-1.0.1-Darwin-arm64.tar.gz） | CMakeLists.txt:560-564 |
| E3 | 引擎只有 iOS 强制 MACOSX_BUNDLE（CMAKE_SYSTEM_NAME STREQUAL \"iOS\" 分支）；macOS 桌面构建是裸 Mach-O 可执行 | CMakeLists.txt:208-217（iOS 专属）；无 APPLE 桌面 BUNDLE 设置 |
| E4 | install(TARGETS) 带 BUNDLE DESTINATION（为 iOS probe 预留）；APPLE 注释确认（CPack needs a BUNDLE destination） | CMakeLists.txt:507-511 |
| E5 | Linux 通道（已落地）：UNIX/NOT APPLE -&gt; 安装 SDL3 SONAME 实文件 + INSTALL_RPATH \"$ORIGIN\"（$ORIGIN = Linux 机制；macOS 等价物是 @loader_path） | CMakeLists.txt:585-597（if(UNIX AND NOT APPLE AND TARGET SDL3::SDL3)） |
| E6 | CI macos 构建用 brew：brew install cmake sdl3 freetype zstd openssl@3 pkg-config lua；无源码构建、无 rpath 处理——SDL3 dylib 的 install_name 是 brew 绝对路径（/opt/homebrew/opt/sdl3/lib/libSDL3.dylib），不可重定位 | .github/workflows/ci.yml:181（brew 行）；对比 release-linux 的源码构建 387-397 |
| E7 | Linux 通道 CI 先源码构建 SDL3 -&gt; /usr/local（可重定位路径），再 CPack TGZ + 29 断言 verify；macOS 没有等价 job | ci.yml:387-397（SDL3 源码构建）、413-424（TGZ verify + upload） |
| E8 | SoLoud 后端选择：WINMM+WASAPI（WIN32）；ALSA（UNIX 非 APPLE 非 ANDROID）；OPENSLES（ANDROID）；COREAUDIO（**iOS only**）；macOS 桌面当前**无任何后端**（注释：macOS desktop keeps its current (backend-less, CMake-tested) behavior because CoreAudio init on CI headless runners breaks the http smoke） | CMakeLists.txt:127-145 |
| E9 | SoLoud CoreAudio 后端源码存在且在 Apple 平台可用：soloud_coreaudio.cpp（头注释 Core Audio backend for Mac OS X）；source 选择器在 src.cmake:184-199：NOT APPLE->FATAL_ERROR、需 AudioToolbox framework、add_definitions WITH_COREAUDIO | external/soloud/src/backend/coreaudio/soloud_coreaudio.cpp:1；external/soloud/contrib/src.cmake:182-199 |
| E10 | Configure.cmake 选项默认 OFF：SOLOUD_BACKEND_COREAUDIO（需显式 ON） | external/soloud/contrib/Configure.cmake:26 |
| E11 | verify 脚本已支持双容器（.zip + .tar.gz 自动挑包/解压分支）、无 .exe 依赖（CaesuraAmeKAG.exe 与 CaesarAmeKAG 双名探测）、进程清理 POSIX 兜底 | scripts/verify_release_package.sh:87-97（glob 双容器）、209-211（解压分支）、220-240（双名探测）、136-163（powershell/ps 双分支） |
| E12 | CI release 选择包：Windows job Get-ChildItem -Filter \"*.zip\"...Select-Object -First 1；Linux job ls build/CaesuraAmeKAG-*-Linux-*.tar.gz | head -1——均依赖现有命名公式，若包名加 config 后缀会打破（A7） | ci.yml:350-351、413 |

## a) 决策建议：TGZ 裸目录布局（镜像 Linux）vs .app bundle

**结论：首版 macOS 通道采用 TGZ 裸目录布局（镜像 Linux），不做 .app bundle。**

理由（三平台对照）：
1. **零新 CMake 成本**：E1 已把 TGZ 定为所有非 Windows 平台的容器；E2 包名公式自动产出 CaesuraAmeKAG-<v>-Darwin-arm64.tar.gz。.app bundle 需要额外 MACOSX_BUNDLE 设置 + Xcode 产物处理 + Info.plist + 签名决策（无签名 .app 在 macOS 上触发 Gatekeeper 更激进的拦截；裸二进制 + 终端启动是内容创作者更熟悉的形态）。
2. **与 Windows/Linux 通道同构**：Windows 是 ZIP 裸目录，Linux 是 TGZ 裸目录——macOS 用同构布局后，verify_release_package.sh 的内容断言（exe in root、web-editor/dist、scripts、assets、demo、tools/project_templates、external/lua）与 create/build/run 探针全部零改动适配（E11 已具备双容器 + 双名探测；t27 已用假 tar.gz 证明解压分支）。
3. **一次通道 vs 一次体验**：.app bundle 的真正收益（双击启动、Dock 图标、GUI 体验）属于 Phase2 打包体验层（产品化任务书 Phase2 Release/Distribution 的 macOS .app/DMG/签名/公证），应在其时点一次性设计（含 codesign/notarization 策略），不要在此通道重复半套。
4. **风险最小化**：.app 的 Contents/MacOS 布局会让 SDL_GetBasePath 锚定逻辑走进 main.cpp:923-928 的 exeDir parent_path 链（i<4 三层上探已为此设计），但 verify 脚本的 ROOT 探测（CaesuraAmeKAG at archive root）不认 .app 结构——选择裸目录即规避该适配面。

**结论一句话：先上 TGZ 裸目录把「陌生用户在 mac 上能解压即跑」（含 --editor）打通；.app/DMG/公证作为 Phase2 独立里程碑。**

## b) CMakeLists.txt 拟改动片段（提案——队长专属，勿由本任务落地）

```cmake
# --- 在现有 if(UNIX AND NOT APPLE AND TARGET SDL3::SDL3) 块后新增：----------
# Sprint6-L2: macOS desktop package must be relocatable. brew's libSDL3.dylib
# carries an absolute install_name (/opt/homebrew/opt/sdl3/lib/libSDL3.dylib
# or /usr/local/...) -- copying it into the TGZ and launching from the copied
# dir fails with dyld: library not found. Two-part fix:
#   (1) ship the SDL3 dylib next to the executable (TARGET_SONAME_FILE;
#       matches the loader's requested name like the Linux branch);
#   (2) rewrite the installed dylib's install_name to @loader_path so the
#       loader resolves it relative to the executable, mirroring $ORIGIN.
if(APPLE AND NOT IOS AND TARGET SDL3::SDL3)
    get_target_property(_caesura_sdl3_type SDL3::SDL3 TYPE)
    if(_caesura_sdl3_type STREQUAL "SHARED_LIBRARY")
        install(FILES "$<TARGET_SONAME_FILE:SDL3::SDL3>" DESTINATION . OPTIONAL)
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "@loader_path")
    # post-install: make the packaged dylib self-referencing (id = @loader_path/libSDL3.dylib)
    install(CODE "
        execute_process(COMMAND install_name_tool -id \"@loader_path/libSDL3.dylib\"
                        \"${CMAKE_INSTALL_PREFIX}/libSDL3.dylib\" RESULT_VARIABLE _r)
        if(NOT _r EQUAL 0)
            message(WARNING \"install_name_tool failed: ${_r} (dylib may not be relocatable)\")
        endif()
    ")
endif()
```

**注意**：
- macOS 上 SDL3::SDL3 的 SONAME 是 libSDL3.dylib？——需在 macos-latest 实测确认（brew 的 .dylib 是否为 symlink+实文件组合；TARGET_SONAME_FILE 语义与 Linux 相同，但 install_name 必须改写）。若 SONAME 文件不存在（brew 只有绝对 install_name 的实文件），改为 install(FILES "$<TARGET_FILE:SDL3::SDL3>") 并同样改写 -id。
- **更稳的替代（推荐路径）**：镜像 Linux 源码构建——release-macos job 里 brew sdl3 换成从源码构建 SDL3 3.2.0（cmake -DCMAKE_INSTALL_PREFIX=$RUNNER_TEMP/sdl3）并传 SDL3_DIR；源码构建的 dylib install_name 通常是 @rpath/libSDL3.3.dylib（SDL 的 CMake 默认），配 INSTALL_RPATH @loader_path 即可，无需 install_name_tool。ci.yml 提案按此主线写，CMake 提议保留 install_name_tool 方案作 fallback。

## c) ci.yml release-macos job 完整 YAML 提案（镜像 release-linux 结构）

```yaml
  # ============================================================
  # Release Package (macOS TGZ) -- Sprint6-L2. Mirrors release-linux:
  # master/tags only, needs build-macos green. macOS has NO Xvfb: the
  # --editor HTTP smoke on macos-latest HEADLESS is UNKNOWN (bgfx/Metal
  # window creation may fail without a GUI session) -- the job runs it
  # under timeout with continue-on-error so a video-backend limitation
  # does NOT silently red the whole release before the stranger path
  # (create/build/run, which needs no display) is proven.
  # ============================================================
  release-macos:
    name: "macOS · Package"
    if: github.ref == 'refs/heads/master' || github.ref == 'refs/heads/main' || startsWith(github.ref, 'refs/tags/')
    runs-on: macos-latest
    timeout-minutes: 40
    needs: build-macos
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      # DIFF vs release-linux: no apt, no ALSA null device; brew install works.
      - name: Build SDL3 from source (relocatable install_name; see E6 note)
        run: |
          wget -q https://github.com/libsdl-org/SDL/releases/download/release-3.2.0/SDL3-3.2.0.tar.gz
          tar xzf SDL3-3.2.0.tar.gz
          cd SDL3-3.2.0
          cmake -B build -DCMAKE_BUILD_TYPE=Release -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
                -DCMAKE_INSTALL_PREFIX="$RUNNER_TEMP/sdl3-mac"
          cmake --build build --parallel $(sysctl -n hw.logicalcpu)
          cmake --install build
          cd ..
          rm -rf SDL3-3.2.0 SDL3-3.2.0.tar.gz

      - name: Configure Release
        env:
          SDL3_DIR: ${{ runner.temp }}/sdl3-mac/lib/cmake/SDL3
        run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCAESURA_ENABLE_FFMPEG=OFF -DCMAKE_PREFIX_PATH="$(brew --prefix openssl@3)"

      - name: Build Release
        run: |
          set -o pipefail
          cmake --build build --config Release --parallel $(sysctl -n hw.logicalcpu) > build.log 2>&1 || { echo '=== build errors ==='; grep -iE 'error:|fatal error|undefined' build.log | head -30; tail -40 build.log; exit 1; }
          tail -3 build.log

      - name: Package (CPack TGZ)
        run: cd build && cpack -G TGZ

      # DIFF vs release-linux: no Xvfb; --editor probe degrade-tolerant.
      - name: Verify release package (29 assertions; editor probe degrade-tolerant)
        continue-on-error: true
        run: |
          TGZ=$(ls build/CaesuraAmeKAG-*-Darwin-*.tar.gz | head -1)
          echo "verifying: $TGZ"
          # macOS headless: --editor needs a window; if bgfx/Metal init fails
          # the HTTP serve checks will honestly FAIL but the stranger
          # CREATE/BUILD/RUN path proof is still decisive. Log everything.
          timeout 900 bash scripts/verify_release_package.sh "$TGZ" || { echo "verify exit $? (see log; editor-serve may fail headless)"; }

      - name: Upload Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: caesura-amekag-macos-arm64
          path: |
            build/CaesuraAmeKAG-*-Darwin-*.tar.gz
            build/CaesuraAmeKAG-*-Darwin-*.tar.gz.sha256
```

**mac 差异步骤标注**：①SDL3 源码构建（brew→源码，install_name 可重定位）；②Configure 传 SDL3_DIR=$RUNNER_TEMP；③无 Xvfb → --editor smoke 用 timeout+continue-on-error 探测（未知行为）；④包名 glob *-Darwin-*（E2 命名公式）。
## d) scripts/verify_release_package.sh 第三分支（macOS 包）diff 级提案

**本任务禁止实际改动该脚本**（Linux TGZ 首验未回，不得弄脏刚验证的 29/29 版本）。以下为提案，待 Linux TGZ 验证绿灯后由 captain 决定是否合并或另开任务：

```diff
--- a/scripts/verify_release_package.sh
+++ b/scripts/verify_release_package.sh
@@ auto-pick glob (当前已双容器) @@
# (现有) CANDS=... CaesuraAmeKAG-*.zip → *.tar.gz 双容器 + 排除规则
# macOS 通道无需改 glob：CaesuraAmeKAG-<v>-Darwin-arm64.tar.gz 已被 *.tar.gz 命中
# （建议：确认注释更新为三平台：Zip=Windows、tar.gz=Linux/macOS）

@@ extract 分支（当前已按容器） @@
# (现有) case *.tar.gz|*.tgz → tar -xzf；* → unzip
# 无需改：mac TGZ 走 tar 分支

@@ exe 探测：无改动 @@
# (现有) 双名探测 CaesuraAmeKAG.exe / CaesuraAmeKAG
# macOS 裸 Mach-O 可执行 = CaesuraAmeKAG（无扩展名）——已被第二个候选命中

@@ 唯一需要新增的断言（提案，otool 平台工具） @@
+# macOS 特有：SDL3 dylib 可重定位核查（若 b) CMake 走 brew 拷贝路线）。
+#   若 b) 采用源码构建路线（推荐），此断言可省略（install_name 是 @rpath，天然可重定位）。
+#   建议：verify 第 2 节 content 组增加
+if [ -f "$ROOT/libSDL3.dylib" ]; then
+    # 检查 dylib 的 LC_ID_DYLIB 是 @loader_path/@rpath 而非绝对路径
+    ID="$(otool -D "$ROOT/libSDL3.dylib" 2>/dev/null | tail -1 | tr -d ' ')"
+    case "$ID" in
+        @loader_path*|@rpath*|@executable_path*)
+            ok "SDL3 dylib install_name relocatable ($ID)" ;;
+        *)
+            bad "SDL3 dylib install_name relocatable" "LC_ID_DYLIB=$ID (absolute path breaks relocation)" ;;
+    esac
+else
+    ok "SDL3 dylib present or statically linked (no libSDL3.dylib in package)"
+fi
```

（该断言用 otool（macOS 平台工具）——在非 mac 平台不会执行到（包无 dylib 时走 else 分支），保持三平台兼容。若 Linux/mac 均采用“源码构建 + @rpath”路线，则统一走 else 分支零成本。）

## e) SoLoud 音频决策分析

**现状**：macOS 桌面 SoLoud **无后端**（CMakeLists.txt:127-145；E8 注释明确 macOS desktop keeps its current (backend-less, CMake-tested) behavior because CoreAudio init on CI headless runners breaks the http smoke）。

**源码证据（exact paths/lines）**：
- external/soloud/src/backend/coreaudio/soloud_coreaudio.cpp 第 1 行头注释：“Core Audio backend for Mac OS X” —— **CoreAudio 对 macOS 可用**（并非仅 iOS）。
- external/soloud/contrib/Configure.cmake:26：option (SOLOUD_BACKEND_COREAUDIO “...” OFF) —— 默认关闭，需显式 ON。
- external/soloud/contrib/src.cmake:184-199：if (SOLOUD_BACKEND_COREAUDIO) if (NOT APPLE) FATAL_ERROR ... add_definitions (-DWITH_COREAUDIO) ... find_library (AUDIOTOOLBOX_FRAMEWORK AudioToolbox) ... set (LINK_LIBRARIES ...) —— 启用成本 = 设 ON + 链接 AudioToolbox framework（macOS 系统自带）。
- external/soloud/src/backend/miniaudio/ 存在（miniaudio.h + soloud_miniaudio.cpp），但 Configure.cmake 无对应 option（源码树有，构建开关缺）——启用需先补 option，成本高于 CoreAudio。

**决策分析：先发无声版 vs 启用 CoreAudio**

| 维度 | 无声版（现状保留） | 启用 CoreAudio |
|---|---|---|
| 成本 | 0（零改动） | 小：CMakeLists 加 elseif(APPLE) set(SOLOUD_BACKEND_COREAUDIO ON) + 链接 AudioToolbox（src.cmake 已处理）+ CI headless 需处理 init failure（E8 注释） |
| 用户价值 | 有 BGM/SE demo 但无声（体验减半，但对通道验证目标不致命） | 立即全音频体验 |
| 风险 | 无（已被 CMake-tested 覆盖） | 中（CI headless 音频 init 不确定性——需与 Linux ALSA null device 同类的方案或 CoreAudio 空设备 mock） |
| 与 Phase2 关系 | 作为通道打通阶段正确 | 与 .app/DMG/公证同属体验层，可延后 |

**结论：首版 mac 通道先发无声版（保持现状，CMakeLists:127-145 零改动）**，与 channel-first 的战略一致（通道验证 → 体验打磨分层）。启用 CoreAudio 作为 follow-up（预计工作量：CMakeLists 单分支 + CI headless 音频守卫，参照 Linux ALSA null device 方案 ci.yml:383-385）；同时建议在验证体系里对“mac 包无声”做诚实标注（docs/status 或已知问题），不做“音频已就绪”的隐含声明——与“禁止只改 README 假装完成”的工作纪律一致。

## f) 风险清单与验证顺序

**风险清单**
1. **brew SDL3 dylib 不可重定位（高，E6 已证实模式）**：绝对 install_name。缓解：SDL3 源码构建（推荐）或 install_name_tool -id @loader_path（fallback，b) 片段）。
2. **macos-latest headless --editor 行为未知（高）**：bgfx/Metal 窗口创建在无 GUI session 的 runner 上可能失败；--editor HTTP smoke 可能因此红。缓解：c) 提案 continue-on-error + timeout 探测（与 iOS probe 同款红绿哲学 ci.yml:425-427 continue-on-error: true）；另可尝试 --backend metal + env 无窗口模式（需实测）。
3. **包名后缀禁令（A7，中）**：任何 config 后缀都会打破 ci.yml:350-351 / 413 的选择逻辑与 verify 脚本 mtime 选包。缓解：本方案所有命名沿用 E2 公式，零改动；明确“禁止加后缀”为接口约束。
4. **SoLoud 无声（低/已知）**：e) 已决策首版保留；文档诚实标注。
5. **verify 脚本第三分支的 otool 断言平台风险（低）**：提案断言只在有 libSDL3.dylib 时执行；无 dylib（源码构建静态链接或 @rpath 路线）走 else。且该文件 Linux TGZ 首验未回——**不得在本任务改动，等首验绿灯**。
6. **arm64 假设（中）**：macos-latest runner 是 arm64；若未来需 x86_64 英特尔 mac，CMAKE_SYSTEM_PROCESSOR 变 x86_64、包名变 -Darwin-x86_64——方式同 Linux 已有 -Linux-x86_64 先例，无需新机制，仅标注。

**验证顺序（推荐，串行；每步独立可回滚）**
1. **观测**：release-macos 首跑——验证“裸 Mach-O TGZ 能产出 + 解压即跑”，暴露 brew dylib 与 headless --editor 两高风险的实况。
2. **CMake 侧（b)）**：captain 落 b) 片段（首推源码构建路线）→ CI 复跑 → 包内 libSDL3.dylib 的 otool -D 输出验证 @rpath → 解压后 ./CaesuraAmeKAG --frames 60 与 --editor 分别实测 → 记录证据。
3. **verify 脚本扩展（d)）**：等 Linux TGZ 首验绿灯后，另开任务落实 d) 的 otool 断言（或按源码构建路线直接走 else 零断言）→ 三包双跑（Win ZIP 29/29 回归、Linux TGZ 29/29、mac TGZ 内容+create/build/run 组）。
4. **音频 follow-up（e)）**：CoreAudio ON 分支 + CI headless 守卫 → 三平台音频测试适配（Linux ALSA null 先例）。
5. **收尾**：docs/status/platform-matrix.yaml 加 macOS packaging 行（verified 后）；docs/guides/getting-started.md 补充 mac 解压启动说明（若文档化“双击 .app”则错误——通道是裸目录 TGZ）。

## g) A7 包名约束（明确写入，防回归）

CPACK_PACKAGE_FILE_NAME = CaesuraAmeKAG-<version>-<CMAKE_SYSTEM_NAME>-<CMAKE_SYSTEM_PROCESSOR>（CMakeLists.txt:560-564）为**跨平台接口不变量**：ci.yml 的选择逻辑（Windows: Get-ChildItem -Filter "*.zip"...First 1 ci.yml:350-351；Linux: ls ...*-Linux-*.tar.gz | head -1 ci.yml:413）与 verify_release_package.sh 的 mtime 选包（scripts/verify_release_package.sh:87-97）都依赖该公式。任何添加 config 后缀的改动必须同步全部三个消费方，否则静默错包/假红——本方案全程零后缀。
