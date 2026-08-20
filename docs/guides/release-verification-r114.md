# Release 全链路终验（R114）— v1.0.0 发布清单 + 阶段 G 新特性随包验证

> 执行时间：2026-08-20（git bash，仓库根 `D:/文件存放处/code/Caesura(AmeKAG)`）
> 分支：`master`（HEAD `d574c73a` docs(roadmap): record round 113）
> 依据：`docs/guides/sample-game-release.md` §8（v1.0.0 发布清单）与 `docs/guides/release-process.md`（桌面 Release 全流程）。
> 方法：全部命令真实执行，输出重定向到 `tmp/release-r114/*.log`，耗时用 `date +%s` 前后差计量。
> 基线（round 90 实测）：C++ 849 用例 / Lua 126+19 / ZIP 359 文件；R113 记录对照见 §4。
> 本文件只写入工作树，**不 git 提交**。

---

## 1. 实测结果总表

按 `sample-game-release.md` §8 命令序列逐项执行（顺序执行，避免共享构建产物冲突）。

| # | 命令 | 结果 | 耗时 | 与 round 90 基线对比 |
|---|---|---|---|---|
| 1 | `cmake --build build --config Release --parallel` | ✅ 零错误（`BUILD_EXIT=0`）；增量无重编译（4s 全部 up-to-date） | **4 s** | 基线 5–15 min（全量）；本次为已配置 VS2022 多配置生成器的增量构建，更快 |
| 2 | `build/tests/Release/CaesuraTests.exe` | ✅ 976/976 passed、8858/8858 assertions、**0 failed / 0 skipped**（`TESTS_EXIT=0`） | **24 s** | 基线 C++ **849** → 976（**+127** 用例） |
| 3a | `external/lua/lua.exe tests/scripts/run_lua_tests.lua` | ✅ **132 passed / 0 failed**（`LUA_MAIN_EXIT=0`） | **8 s** | 基线 126 → 132（**+6**） |
| 3b | `external/lua/lua.exe tests/scripts/run_orphan_tests.lua` | ✅ **24 passed / 0 failed**（`LUA_ORPHAN_EXIT=0`） | **4 s** | 基线 19 → 24（**+5**） |
| 4 | `python scripts/count_coupling.py --ci` | ✅ PASS：entry 14/14、di 13/14、script 11/14、其余 ≤4 | **<1 s** | 与基线一致（entry 14、di 13、script 11） |
| 5 | `cpack -C Release -G ZIP`（workdir=build） | ✅ `CaesuraAmeKAG-1.0.0-Windows-AMD64.zip` + `.sha256` 重新生成（`CPACK_EXIT=0`）；ZIP 87,972,314 B（**87.97 MB**）、**403 文件**、sha256 `60d70d82…e1f6d3` | **9 s** | 基线 ZIP **359 文件** → 403（**+44**）；R113 实测 386 → 403（+17） |
| 6 | 解压 ZIP 冒烟 `./CaesuraAmeKAG.exe --frames 120`（归档根运行） | ✅ Direct3D 11 真实设备初始化（1280x720，3 views RTT→MAIN→DEBUG），demo 加载，`Caesura (AmeKAG) shut down cleanly.`，**停止码 `SMOKE_EXIT=0`** | **3 s**（解压+运行） | 与 round 90 冒烟一致（干净启动/干净退出 exit 0） |
| 7 | `bash scripts/package_game.sh --no-web-build demo/example_game` | ✅ EXIT=0；ks_check 两场景过契约；ks_bake 产出 `cache/story/story.lua`（2 scenes/6 assets）；`dist/example_game` = **35,241 KB（≈34.4 MiB）**，MANIFEST 齐全 | **1 s** | 与 R113 实测一致（≈34.4 MiB、2 场景） |

> 注：① 步骤 1 增量构建因 round 113 后无 C++ 源码变更而直接 up-to-date（4s 为 msbuild 扫描+拷贝脚本/运行时）；② 步骤 5 为覆盖式重建（旧 ZIP 同路径，大小 87,954,428 B → 87,972,314 B）；③ 步骤 7 复用 `web/dist`（`--no-web-build`），未触发 vite 重建。

---

## 2. 阶段 G 新特性随包核对（ZIP 内确认）

| 阶段 G 特性 | 随包证据（`unzip -l` 命中） | 状态 |
|---|---|---|
| 后处理栈（#102） | `shaders/dx11/fs_postfx_{vignette,lut,blur,bloom}.{hlsl,dxbc}` + `glsl/*.sc` + `metal/*.metal`；冒烟日志 `PostFxVignette/LutGrade/SoftBlur/Bloom program READY` | ✅ 三平台 shader 入包，D3D11 下全部编译就绪 |
| `[tween]`（#106） | `scripts/kag/commands/tween.lua`（11,566 B）+ `demo/tutorial/tutorial_16_tween.ks` | ✅ 命令处理器 + 教程场景入包 |
| `[layout]`（#107） | `scripts/kag/commands/layout.lua`（11,207 B）+ `scripts/kag/layout_math.lua` + `text_layout.lua` | ✅ 命令处理器 + 布局数学库入包 |
| Scene Builder（#108） | `scripts/kag/text_scene.lua`（10,127 B，场景构建器运行时） | ✅ 入包 |
| 示例游戏（#105/#110） | `demo/example_game/`：`story.ks`（16,716 B）+ `story_lastletter.ks`（5,215 B）+ `entry.lua` + `DESIGN.md`/`README.md`/`i18n-report.md` | ✅ 双场景三结局完整入包 |
| 打包脚本（#108/#109） | `scripts/package_game.sh`（仓库工具）实测 EXIT=0；`scripts/ks_bake.lua`/`ks_check.lua` 为打包管线步骤 | ✅ 发布管线在本仓库可跑可用 |

---

## 3. 冒烟日志关键行摘录（`tmp/release-r114/smoke/smoke.log`，142 行）

```
[SDL3] Platform backend initialized: 1280x720 "Caesura (AmeKAG)"
[BgfxRenderDevice] nwh=…, w=1280, h=720, backend=Direct3D 11
[BgfxRenderDevice] Initialized 1280x720 with 3 views (order: RTT -> MAIN -> DEBUG)
[BgfxShaderManager] PostFxVignette program READY.   # 后处理栈就绪
[BgfxShaderManager] PostFxLutGrade program READY.
[BgfxShaderManager] PostFxSoftBlur program READY.
[BgfxShaderManager] PostFxBloom program READY.
[Audio] SoLoud initialized: 3 buses (BGM, VOICE, SE) ready.
[SaveManager] Initialized. Save dir: saves/ (schema v5)
[DEBUG] [WARN] HotReload scan failed: …"assets/script/"   # 良性 WARN（dev-only，不进 ZIP，见 release-process §5）
[Lua] Loading script: scripts/../demo/entry.lua            # demo 加载
Caesura (AmeKAG) shut down cleanly.                        # 干净退出
SMOKE_EXIT=0
```

---

## 4. 遗留项与备注

| 项 | 说明 | 类别 |
|---|---|---|
| `gh release create` + tag 推送 | 未执行（§8.2 步骤 4–5 明确标注「仅发布者执行」） | 需用户确认动作（gh 已认证 `ailiasdesu`） |
| itch.io 发布 | butler 未安装 + 需用户 itch.io 账号登录（`docs/guides/sample-game-release.md` §4B） | 需用户账号/装 CLI |
| 后处理**视觉效果**目视验证 | 冒烟机为 Direct3D 11 真实设备，shader 全部编译就绪；但 120 帧冒烟不截图，bloom/lut/vignette/softblur 的实际渲染效果未人工目视确认（无画面比对手段） | 需真机/人工目视 |
| 冒烟日志 3 条 `RENDER [ERROR] ShaderCache compileVariant: unregistered variant blend=16/10/11` | 后处理栈注册 10 种 blend 模式，但启动期有 3 个未注册 blend 变体请求，**降级回退 Normal**（非致命、demo 照常跑）。疑似新后处理/画面效果请求了未登记 blend 变体——建议后续轮核对 ShaderCache 注册清单与效果绑定是否一致（round 90 基线日志未见此 ERROR，属阶段 G 新增现象） | 待核对（非阻塞） |
| `verify_sample_game.sh` 默认路径缺陷 | 已知：默认 `story.ks.new`（已不存在），裸跑假红；需 `SAMPLE_STORY=demo/example_game/story.ks` 显式传入（§2.4，发布前建议修） | 已知缺陷（发布前建议修） |
| `gen_changelog.py` Windows GBK stdout | 需 `PYTHONIOENCODING=utf-8` 前缀（§8 坑提醒；本轮未执行 changelog 步骤，属发布者动作） | 已知坑 |
| 中间日志 | 全部留在 `tmp/release-r114/`（`tmp/` 已在 `.gitignore`，不影响工作树） | — |

---

## 5. 结论

- **桌面 / Web 两条产物生成链路全部通过**：Release 构建零错误、C++ 976/976、Lua 132+24、耦合 PASS、CPack ZIP（403 文件）+ sha256、ZIP 冒烟干净退出 exit 0、Web 站打包 EXIT=0。
- **阶段 G 新特性全部随包可用**：后处理栈 shader（三平台）、`[tween]`/`[layout]` 命令、Scene Builder 运行时、示例游戏双场景，均在 Release ZIP 内确认，且引擎启动日志证实后处理 program 编译就绪。
- **与 round 90 基线对比**：C++ 849→976（+127）、Lua 126+19→132+24（+6/+5）、ZIP 359→403 文件（+44）。
- **卡点不在构建**：发布前置动作（tag/gh release/itch）全部是用户侧操作；引擎侧仅 2 个非阻塞观察项（blend 变体降级提示、verify 脚本默认路径）建议发布前顺带处理。
