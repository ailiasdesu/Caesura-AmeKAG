# Web Release Checklist（Track W8 — Release Gate）

> 状态：**WEB_RELEASE_STATUS = RC-READY**（2026-08-23，本机 Windows + Chrome/Edge headless-new + CDP 实机验证）
> 断言证据全部来自零依赖脚本：`scripts/web_browser_smoke.mjs`（真浏览器，非 mock）；配套单测 `cd web && npm test`。
> 复现（仓库根，git bash）：
> ```bash
> bash scripts/package_game.sh tests/projects/first_vn       # 或其他场景：example_game / cjk_smoke / web_stress_vn
> node scripts/web_browser_smoke.mjs --root dist/first_vn [--browser edge] [--unlock] [--suspend] [--cjk] [--stress] [--subpath games]
> ```

## 检查项（全部通过 → RC-READY）

| # | 检查项 | 状态 | 证据（2026-08-23，commit b418c8c7） |
|---|--------|------|------|
| 1 | clean build | ✅ | `rm -rf web/dist && cd web && npx vite build`（零报错；产物 index.html + web-assets/(js+glue.wasm) + scripts/ + demo/ + assets/ + cache/story/） |
| 2 | package | ✅ | `bash scripts/package_game.sh` ×（first_vn / cjk_smoke / web_stress_vn）；MANIFEST.txt 生成；无绝对路径依赖 |
| 3 | Chrome smoke | ✅ | Chrome headless-new：ROOT 10/10、UNLOCK 12/12、SUSPEND 15/15、CJK 18/18、STRESS 18/18 |
| 4 | Edge smoke | ✅ | Edge headless-new：ROOT 10/10、CJK 18/18、SUSPEND 15/15、STRESS 16→18/18 |
| 5 | text | ✅ | .caesura-message 渲染断言（first_vn / cjk_smoke 全页面收集） |
| 6 | image | ✅ | `img.naturalWidth>0` 解码断言（背景/立绘；截图 smoke-cjk.png/smoke-chrome.png 多模态复核） |
| 7 | CJK | ✅ | 中/日/英/混合标点逐页断言 + @font-face `document.fonts.check('CaesuraNoto')=true` + 打包字体 URL 资源条目（打包产物） |
| 8 | input | ✅ | CDP 可信点击推进（advance/run）在 root/subpath/suspend 均验证；手势解锁 Chrome suspended→running |
| 9 | audio | ✅ | WebAudio `_sources:[bgm]` 真实解码启动；audio-status 报告；Chrome 默认 autoplay 策略下用户手势解锁（suspended→running）|
| 10 | save/load | ✅ | Save Current → 槽位列出 → `Page.reload` 后仍在（localStorage `caesura.save.<slot>`）；坏载荷跳过（jsdom 套件 + 浏览器双验证） |
| 11 | packaged run | ✅ | dist/* 静态服务器直跑（root 与 `--subpath games` 子路径均通过；离线自包含：0 CDN 资源 + 本地 glue.wasm） |
| 12 | tab suspend/resume | ✅ | 真实标签切换（visibilitychange hidden→visible，6.5s）：场景无跳变、WebAudio 存活、返回后输入/循环恢复（Chrome+Edge） |
| 13 | stress case | ✅ | web_stress_vn 3 整轮（36 页+循环）：纹理缓存 12 恒定、DOM 层 1、0 页面/引擎（含 WASM）错误；boot Chrome 1966ms / Edge 2715ms（实测记录） |
| 14 | no known blocker | ✅ | 全量 `cd web && npm test` 318/318（22 文件）；w3 审查 APPROVE；w5/w7 无遗留 P0 |

## 已知限制（不阻塞 RC-READY，记录在案）

- 浏览器缓存命中验证 = 宿主相关（smoke 静态服务器 `no-store`）；静态宿主（GitHub Pages/Netlify/S3）依赖宿主 cache 头。
- Edge headless 默认放行 autoplay，无法复现 suspended 起始态（Chrome 已可作为该契约定性证据）；实机 Edge 桌面策略以实机为准（device-unverified 分层）。
- Web 渲染为 DOM/HTML（无 GPU）；iOS/Android 属 Track M/I 范围，不在此清单。
- 测试/验证覆盖 Windows 本机；macOS/Linux 桌面 Web 服务器（host browser）未列入本 gate（跨平台矩阵见计划 §8，Web 行已由本清单闭环）。

## 决策记录

```text
WEB_RELEASE_STATUS = RC-READY
web vitest: 318/318 (22 files)
real-browser: Chrome ×5 modes + Edge ×4 modes, all PASS (fresh clean rebuild)
```
