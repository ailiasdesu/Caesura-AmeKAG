# Caesura (AmeKAG) — Release Gate

> **Release Gate（Rel Gate）** — 任何正式版本必须满足的一组硬性签署清单（checkbox）。
> 它是任务书 docs/plans/audit/Caesura-AmeKAG_产品化推进总任务书.md §21 的落地文档化；
> Runtime/Platform 之外的 Golden Project / Golden Save 要求来自同任务书 §14。
>
> 执行顺序：先用本清单在**本地/CI 全绿**，再走
> [release-process.md](./release-process.md) 的发布流程（构建→门禁→changelog→CPack→验证→gh release）。
> 本清单**不替代** release-process.md —— 它是「为什么能发」的签署，
> release-process.md 是「怎么发」的步骤。

---

## 0. 签署头（每次发布前填写）

| 字段 | 值 |
|------|----|
| Version | (v1.0.x) |
| Date | (YYYY-MM-DD) |
| Gatekeeper | (发布负责人) |
| 来源分支 | master（所有改动先合入 master 且门禁全绿） |
| 结论 | ☐ 通过（可发布） ☐ 阻断（见 §8 失败处理） |

> 清单中每一项都是 **Red-blocker**：任一项失败即不得打 tag、不得 `gh release create`。

---

## 1. Runtime Gate

正式版本运行时层面必须满足，可脚本验证。

- [ ] **全关键 test pass** — C++ 套件全绿，`0 failed, 0 skipped`：
      ```bash
      cd build/tests/Release && ./CaesuraTests.exe   # Release 门禁（CWD 必须在此）
      ```
- [ ] **command contract pass 123/123** — 全部 123 个 KAG Neo-Genesis 契约命令
      均有运行时执行覆盖（README / engine-capability-matrix.md S2v）。
      验证命令（静态契约 0 violations + 专项运行时套件）：
      ```bash
      external/lua/lua.exe scripts/ks_check.lua demo/galgame_demo.ks demo/full_pipeline_demo.ks scripts/demo_story.ks
      external/lua/lua.exe tests/scripts/run_lua_tests.lua   # 主套件（含契约/表达式专项）
      ```
- [ ] **save migration pass** — 旧版本存档能被当前版本读入并迁移。
      运行期 storage 模块 schema migration 用例；Golden Save 跨版本迁移见 §7。
      ```bash
      cd build/tests/Debug && ./CaesuraTests.exe -tc=*Save*
      ```
- [ ] **no known high severity bug** — 当前无未修复的 P0/P1 blocker（P0=影响既有游戏/
      安全/存档损坏；P1=第三方无法完成核心流程）。已知问题列表见 issue 与 ROADMAP。

> 附：Lua 主套件 132 + 孤儿 24、耦合预算、生成物新鲜度（api_stats / gen-index --check）
> 由 release-process.md §2 兜底，发布前一并跑通。

---

## 2. Platform Gate

仅对**声明支持**的平台强制；未声明支持的平台不得声称「跨平台支持完成」。

当前真实覆盖（2026-08）：

| 平台 | 编译（CI 绿） | 真机验证 |
|------|--------------|----------|
| Windows | ✅ Debug+Release MSVC | ✅ **D3D11 + OpenGL 真机构建/冒烟已做** |
| Linux | ✅ GCC（SDL3 自编） | ⏳ 代码就绪，真机待设备 |
| macOS | ✅ Clang | ⏳ 待设备（Apple Silicon/Metal） |
| Web | ✅ 构建/测试绿 | ⏳ 手工走查（见 sample-game-verification.md） |

- [ ] **Windows smoke** — 本地 Release 构建 + `--frames 60` GPU 冒烟退出 0。
- [ ] **Linux smoke**（若声明支持）— 三平台 CI 编译绿为最低门槛；真机待设备。
- [ ] **macOS smoke**（若声明支持）— 同上。
- [ ] **Web smoke**（若声明支持）— `node web/gen-index.mjs --check` + 播放器手工走查：
      ```bash
      bash scripts/package_game.sh            # 产出 dist/<game>/ 静态站
      # 浏览器加载 dist/<game>/ 验证 demo 播放（详见 docs/guides/sample-game-verification.md）
      ```

---

## 3. Packaging Gate

发布归档（CPack ZIP）必须完整、可解压、干净机器可启动。

- [ ] **package build** — Release 构建后 CPack 打 ZIP：
      ```bash
      cmake --build build --config Release --parallel
      cd build && cpack -C Release -G ZIP && cd ..
      # → build/CaesuraAmeKAG-<ver>-Windows-AMD64.zip + .sha256
      ```
- [ ] **clean extraction** — 解压到**全新**目录，核对从归档根起内容完整：
      ```bash
      unzip -l build/CaesuraAmeKAG-*.zip
      mkdir -p /tmp/caesura-gate && cd /tmp/caesura-gate
      unzip "$OLDPWD/build/CaesuraAmeKAG-*.zip"
      # 核对：CaesuraAmeKAG.exe + SDL3.dll(+FFmpeg DLL) + scripts/ demo/ assets/ shaders/ README.md LICENSE
      ```
- [ ] **clean machine startup** — 从未装过引擎的干净环境，直接从归档根启动，无缺失资源错误。
- [ ] **game launch** — 归档内启动 demo 正常播放：
      ```bash
      ./CaesuraAmeKAG.exe --frames 60    # 从解压目录启动，确定性 GPU 冒烟，退出 0
      ```
- [ ] **save/load** — 运行 demo 触发存档/读档路径正常（storage schema 无回归）。

> 引擎按**工作目录**解析 assets/ 等，必须从解压根启动；`--frames 60` 期望日志以退出 0 收尾。

---

## 4. Editor Gate

编辑器能力（Caesura Studio 路线）发布时应可完成核心闭环。当前 Editor RPC 由
tests/headless_rpc_smoke.py 与 tests/headless_http_smoke.py 覆盖（21/21）。

- [ ] **New Project** — 从模板创建新项目成功。
- [ ] **Open Project** — 打开既有项目并装载场景/脚本。
- [ ] **Run** — 一键运行项目（含 Scene Preview / RPC debug 会话）。
- [ ] **Debug** — 断点/单步/变量面板可用（editor DebugView + inspect）。
- [ ] **Build** — 从编辑器触发构建/打包成功。

---

## 5. Sample Gate

官方示例游戏（demo/example_game/story.ks）必须完整运行、全结局可达、存档与本地化可用。
由 scripts/verify_sample_game.sh 端到端验证（5/5 检查），headless 驱动为
tests/scripts/sample_game_headless.lua。

- [ ] **official sample game complete run** — 全剧本跑到 [end]（零错误）：
      ```bash
      bash scripts/verify_sample_game.sh
      # 期望退出 0：ks_check 干净 + headless 全跑 DONE
      ```
- [ ] **all endings reachable** — 三个结局标签均可达：
      ```bash
      SAMPLE_STORY=demo/example_game/story.ks SAMPLE_ENDING=ending_zero      external/lua/lua.exe tests/scripts/sample_game_headless.lua
      SAMPLE_STORY=demo/example_game/story.ks SAMPLE_ENDING=ending_companion external/lua/lua.exe tests/scripts/sample_game_headless.lua
      SAMPLE_STORY=demo/example_game/story.ks SAMPLE_ENDING=ending_promise    external/lua/lua.exe tests/scripts/sample_game_headless.lua
      # 每条 RESULT DONE 即通过
      ```
- [ ] **saves work** — demo 内 [save] 槽位读写正常（storage 回归无）。
- [ ] **localization works** — 双语 {key} 文案与 lang 表切换正常（web i18n×advance 套件已覆盖）。

> 手工走查（含 Web 播放器）见 docs/guides/sample-game-verification.md。
> headless 只断言「跑到 [end] 零错误」，不断言具体剧情文案。

---

## 6. Docs Gate

发布所依赖的文档必须按文档步骤可复现（禁止「只改 README 假装完成」）。

- [ ] **getting started verified** — docs/guides/getting-started.md 从克隆到 Demo 可跑。
- [ ] **tutorial verified** — demo/tutorial/ 16 场教程场景契约检测通过：
      ```bash
      external/lua/lua.exe scripts/ks_check.lua demo/tutorial/*.ks
      ```
- [ ] **packaging guide verified** — docs/guides/carc-packaging.md 与 release-process.md §4–5
      的打包/解包/冒烟步骤按文档可复现。

---

## 7. Golden Project（任务书 §14）

**tests/projects/golden_vn/** 是长期维护的**合成但完整**的回归夹具（非 showcase），
覆盖全引擎 feature 面：dialogue / choices / save+load / rollback / history(backlog) /
NVL / i18n 热切换 / audio(bgm+se+voice) / tween / layout / replay / mod / text markup /
transitions / particles / video 占位 / animated-sprite / layer fades / 表达式条件分支。
资产引用共享资源池（assets/bg|fg|bgm|se|voice|lang），开箱即跑；每行双语 {key}。

- [ ] **每次 release 跑完整项目** — Golden Project 作为回归门禁端到端驱动（入口
      tests/projects/golden_vn/entry.lua；门禁 bash scripts/verify_golden_vn.sh；
      契约检测 lua scripts/ks_check.lua tests/projects/golden_vn/story.ks）。

> 当前状态：golden_vn 的 story.ks + entry.lua 已就位；**verify_golden_vn.sh 门禁脚本待实现**
> （见 story.ks 头部 Gate 行）—— 其实现是 Phase0 稳定性收尾的一项(P1)。

### Golden Save：跨版本迁移

- [ ] 保存不同版本生成的存档若干（Golden Save 库）。
- [ ] 确认新版本能读入并迁移（storage schema migration），旧档不损坏。

---

## 8. 失败处理（Blocker 纪律）

- 任一 **Red-blocker**（Runtime / Platform / Packaging / Editor / Sample / Docs /
  Golden 任何一项）失败 = 发布**阻断**，不得打 tag、不得 gh release create。
- 记录失败项与日志路径，修复后重跑该项及其依赖项，直至整张清单全绿。
- 逐项 Checkbox 由 **Gatekeeper** 签署；签署前须本人跑过/复核过对应命令，不代签。
- 平台项若**未声明支持**则不强制；但**不得声称**「跨平台支持完成」除非真实平台已验证
  （任务书 §21 红线）。

---

## 9. 快速核对（copy-paste 摘要）

```bash
# Runtime
cd build/tests/Release && ./CaesuraTests.exe && cd ../../..
external/lua/lua.exe tests/scripts/run_lua_tests.lua
external/lua/lua.exe scripts/ks_check.lua demo/galgame_demo.ks demo/full_pipeline_demo.ks scripts/demo_story.ks

# Sample
bash scripts/verify_sample_game.sh            # 5/5

# Packaging
cmake --build build --config Release --parallel
cd build && cpack -C Release -G ZIP && cd ..
unzip -l build/CaesuraAmeKAG-*.zip

# Docs / freshness
external/lua/lua.exe scripts/ks_check.lua demo/tutorial/*.ks
node web/gen-index.mjs --check
python scripts/count_coupling.py --ci
```