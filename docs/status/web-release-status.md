# Web Release Status（Caesura-AmeKAG Agent Execution Plan §W0）

> 生成：2026-08-23（Track W 审计轮，round 1）
> 目的：让另一名 agent 在**无额外上下文**下复现 Web build + package + browser smoke；并列出已验证/未验证能力与剩余阻塞项。
> 相关文档：`docs/guides/release-qa-matrix.md`（QA 矩阵）、`docs/plans/audit/ROADMAP-200.md`、`docs/plans/2026-08-23-026-delivery-handoff.md`

## 1. 当前 Build Command（Windows git bash，仓库根）

```bash
# web player 构建（vite）——输出 web/dist/（index.html + web-assets/*.js + scripts/ + demo/ + assets/ + cache/story/）
cd web && npm install        # 首次
cd web && npx vite build     # 或 npm run build；0 依赖修复后 web/dist/scripts/index.json 自动生成
```

- bundle 目标：`web/dist/web-assets/index-*.js`（159KB，gzip 46KB），入口 `<script type="module" crossorigin src="/web-assets/index-*.js">`。
- **模块索引**：`web/bridge.js` 在启动时硬依赖 `scriptsBase + 'index.json'`（无容错，404 即 boot 失败）。
  - CI 工件：`web/scripts-index.json`（已提交，`node web/gen-index.mjs --check` 保鲜）。
  - vite build：`closeBundle` 现在自动为 `web/dist/scripts/index.json` 生成（vite.config.js `copyRuntimeDirs`）。
  - dev server：`vite.config.js` 的 `devScriptsIndex` 中间件按需生成（此前 dev 模式会 404 挂机）。

## 2. 当前 Dev Command

```bash
cd web && npm run dev:web   # vite @ http://127.0.0.1:5174（host 127.0.0.1；publicDir='..' 服务仓库根）
```

## 3. 当前 Package Command

```bash
# 一键打包：ks_check 契约门禁 → ks_bake --web 烘焙 → vite build → 组装 → gen-index → MANIFEST
bash scripts/package_game.sh                      # 默认 demo/example_game -> dist/example_game
bash scripts/package_game.sh tests/projects/first_vn    # -> dist/first_vn（first VN）
bash scripts/package_game.sh --out dist/foo demo/example_game   # 自定义输出
# 产物自包含：index.html + web-assets/ + scripts/（含 index.json）+ assets/ + demo/<game>/*.ks + cache/story/story.lua + MANIFEST.txt
# 本地预览：cd dist/first_vn && python -m http.server 8080
```

## 4. 当前 Browser Test Command（真实浏览器，非 mock）

```bash
# 零依赖：内置静态服务器 + Chrome/Edge headless-new + CDP（Runtime.evaluate / Page.reload / Input.dispatchMouseEvent）
node scripts/web_browser_smoke.mjs --root dist/first_vn                # Chrome，默认 autoplay 旗标
node scripts/web_browser_smoke.mjs --root dist/first_vn --browser edge
node scripts/web_browser_smoke.mjs --root dist/first_vn --unlock       # 默认 autoplay 策略：验证手势解锁
node scripts/web_browser_smoke.mjs --root dist/first_vn --scene story.ks
# 断言：boot(parked)/text(含 CJK)/image(img naturalWidth>0)/audio(audio-status + WebAudio source)/save(跨 Page.reload 持久)/unlock(state suspended→running)
# 产物：build/web-smoke/smoke-<browser>.png
```

配套自动化测试（jsdom/unit，`cd web && npm test`）：**21 个文件 / 311 用例全绿**（2026-08-23，含本轮新增 W1 unlock 9 单测 + main.mjs 级 4 e2e）。

## 5. 已验证能力（2026-08-23 真实浏览器）

| 能力 | 状态 | 证据 |
|---|---|---|
| boot（引擎加载 + 自动运行 park） | ✅ | Chrome / Edge headless-new，`status = parked`，≤30s |
| text（含 CJK 中/日/英混排） | ✅ | `[Narrator]Your first VN... / 你的...`；截图 multimodal 复核 |
| image（背景/立绘解码） | ✅ | `img.naturalWidth>0`（修复前为 /assets/assets/ 双前缀 404 == 真实阻断） |
| input（advance 推进） | ✅ | CDP 可信点击 `#advance` 后场景前进（--unlock 轮） |
| audio BGM/SE/Voice | ✅ | audio-status 显示 BGM: daily.wav；**WebAudio `_sources:["bgm"]` 存在**（修复前 fetch 404 只静音、状态机仍显示播放） |
| audio autoplay 解锁 | ✅ | Chrome 默认策略：初始 state=suspended → 用户手势后 running（W1 核心）；重复手势幂等 |
| 页面隐藏恢复 | ✅ | unit + jsdom e2e：visibilitychange→visible 再次 resume（真实 Chrome 后台行为待 W5 实测） |
| save/load 持久化 | ✅ | Save Current → slot 列出 → `Page.reload` 后仍在（localStorage `caesura.save.<slot>`） |
| packaged run | ✅ | dist/first_vn 静态服务器直跑，无需改源码 |
| 场景选择 | ✅ | ?scene= 参数 + 下拉；bundle 首场景自动启动 |

## 6. 未验证能力（诚实标注）

- **tab 后台挂起/恢复（W5）**：rAF 主循环 + setTimeout auto-advance 在后台节流 → 待专项测量（web_stress / 长跑页签）。
- **大资源/内存压力（W4）**：暂未建 web_stress_vn；已知资产基线：12MB wav + 16MB CJK otf 单页可跑。
- **Edge autoplay 解锁**：headless Edge 默认策略即 running（未复现 suspended 场景），解锁证明以 Chrome 为准；Edge packaged demo + 存档持久已实测。
- **localStorage quota 超限 UX**：bridge 静默返回 false，UI 仅日志（W2 跟进）；缩略图 capture_thumbnail 恒 null；无 IndexedDB。
- **部署子路径**：打包产物用绝对路径（/assets/、/web-assets/），GitHub Pages 子路径托管会 404（W7 待办：vite base './'）；域名根目录托管可用。
- **离线 wasm**：wasmoon 默认从 unpkg CDN 拉 glue.wasm（`__CAESURA_WASM_FILE__` 未设时）；smoke 脚本固定本地副本才离线可跑（W7：建议 vendor 进 web-assets + index.html 内联 pin）。
- **Android/iOS**：Track M / Track I，不在本阶段 Web 范围。

## 7. 已知限制（核对为当前状态）

1. 打包产物的 `scripts/index.json` 由 package_game.sh 生成；**旧包（2026-08-22 前的 dist/example_game）缺失该文件 → boot 404 挂**，须重新打包。
2. Web 渲染为 DOM/HTML（img + CSS 层），非 Canvas/WebGL —— 无 GPU 依赖（W4 的 WebGL 错误项按"不适用 DOM 渲染器"记录；WASM（Lua VM）错误仍适用）。
3. `skip-mode`/`fast-forward` 在 web 未验证（仅桌面段）；mods/plugin 类能力不在 Web 当前范围。

## 8. 本阶段剩余阻塞项（P0，按计划顺序）

- [x] **W1 Web Audio 生命周期** —— 完成（unlock 幂等 + 初始 suspended→running + 后台恢复 + 销毁安全；Chrome+Edge packaged 实跑；318 测试绿）
- [x] **W2 Web Storage/Save** —— 复用现有 localStorage KV 桥（caesura.save.<slot> 0..99）：新增槽位校验（save_game/_load_raw/saveCurrent/deleteSlot 整数 0..99，非法诚实拒绝）、storageStats()（槽数+字节）、saveCurrent 以 ctx.tf.save_result=='ok' 判成败（原判定把引擎 SaveCommands.save 隐式 nil 当成功，quota 失败误报）、UI 失败可见（#saves-storage 红字 + quato 提示）、web/save-persistence.test.js（真 localStorage + 双实例模拟 reload 持久 + overwrite/invalid/empty/corrupt/quota），真实浏览器跨 reload 持久已由 smoke 验证；IndexedDB 大载荷仍为已记录限制（缩略图恒 null，暂无需）
- [ ] **W3 Web CJK/Font/Asset smoke 场景** —— 现有 first_vn/example_game 已含中文+日文+英文文本且渲染通过；建独立 smoke 场景 + font fallback/缓存项
- [ ] **W4 Web Stress/Memory** —— web_stress_vn（100+ 图集/多音频/CJK 字体/连续切场景/反复加载）+ 实际测量记录
- [ ] **W5 Tab Suspend/Resume** —— 后台恢复/计时器跳变/输入不丢失/存档不损坏
- [ ] **W7 Packaging/Deployability** —— vendor glue.wasm + 子路径 base + 收敛脚本（build_web.sh/package_web.sh 或复用现有）
- [ ] **W8 Release Gate** —— docs/release/web-release-checklist.md + WEB_RELEASE_STATUS=RC-READY

## 9. 复现验证记录（2026-08-23）

```text
Chrome  (headless=new, --autoplay-policy=no-user-gesture-required)  7/7  PASS
Chrome  (默认 autoplay 策略, --unlock)  10/10 PASS  (audio state: suspended -> running; WebAudio source bgm)
Edge    (headless=new, no-user-gesture-required)                    8/8  PASS
Edge    (默认 autoplay 策略, --unlock)  10/10 PASS  (headless Edge 策略本已 running；存档/图片/音频全通过)
web vitest 全量 311/311（21 文件）
```

> 注：W1·浏览器验收要求"Chrome + Edge 各至少运行一次 packaged Web demo"≈已满足；Edge 的 suspended 起始态仅 Chrome 可复现（Edge headless 默认放行 autoplay）。