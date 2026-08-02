# Caesura (AmeKAG) — 交接文档 008（2026-08-02）

> 面向后续 agent 的完整上下文。先读 `AGENTS.md`（模块边界铁律），再读本文档与
> `docs/design/engine-architecture-topology.md`。**本文件为最新状态**；历史交接见
> `docs/plans/2026-08-01-004-delivery-handoff.md`（004）与各 closeout（005/006/007）。

## 1. 当前状态

- **分支**：`master`。HEAD 为本文档的最新修订（doc 提交，代码基线见下）；当前文档链快照自 `git log --first-parent 7303fef4..HEAD` 生成，后续 agent 请以该命令为权威提交列表
- **CI**：快照时三平台全绿（run 30727177787 = success，五 job 全过）；`gh run list --limit 1` 查最新
- **本地验证基线**：doctest **564/564**（断言 **2770**）；ctest **10/10**；HTTP smoke **21/21**；
  `python scripts/count_coupling.py --ci` **PASS**
- **工作树**：干净（仅 `.reasonix/` 未跟踪，非项目产物，勿提交）

### 最近提交（自上轮交接 7303fef4 起；**权威列表 = `git log --first-parent 7303fef4..HEAD`**，下文为生成时刻快照）
```
fde7df91  docs(plans): handoff 008 — self-healing state lines            ← 本文档修订（快照首行）
67a78e3e  docs(plans): handoff 008 refresh — HEAD 1181538a, commit list
1181538a  docs(plans): handoff 008 + closeout 007 refresh; note watcher re-entrancy audit
19aca04b  test(rpc): extract constantTimeEquals with unit coverage; closeout 007
31afd181  test(rpc): fix inverted skip condition in HTTP smoke (blocking review finding)
1d12fd9b  test(rpc): skip HTTP smoke on GPU-less CI runners (SKIP_RETURN_CODE 77)
e1c2c8b1  fix(rpc): truly constant-time token compare; document CAESURA_EDITOR_TOKEN
882932ea  fix(rpc): token from env not argv; exempt OPTIONS; constant-time compare
f0fc2345  feat(rpc): optional --editor-token bearer auth; CORS smoke coverage
edabade3  fix(rpc): guard Origin length before substr in CORS check
55c0512d  fix(rpc): exact-host CORS allowlist; normalize build paths; JSON-escape build reply
b96ffb6f  fix(rpc): confine /api/build outputs to build/; localhost-only CORS; guard pauseId
eb7d4a45  fix(rpc): wrap breakpoint line int64 read in try/catch (400 on parse error)
c2aa8b4f  test(rpc): regression-guard int-overflow breakpoint line rejection
0979d921  fix(rpc): reject int-overflow breakpoint lines via int64 bound check
b053ebc4  fix(rpc): bound-check breakpoint line; make smoke/test paths cross-platform
8b8436c5  feat(rpc): HTTP debug routes + live2d path-confinement tests   ← closeout 006
059d4558  fix(live2d): fail closed on stat error; reject dot components and junctions
6510bd90  fix(live2d): reject symlink final component in fallback; case-fold Windows only
82e45b83  fix(live2d): cap file size before allocation; keep symlink checks in fallback
e81fdf4c  fix(live2d): confine model file reads to working dir; cap texture/file sizes
9f39aa14  fix(live2d): per-model render targets + model-texture SRV ownership
5ed07a39  feat(live2d): first real Cubism SDK build — D3D11 path verified end-to-end
e8a2bfc6  test(entry): extract Engine::handleAppLifecycle for unit testing  ← watcher 注释相关
08845e5a  docs: closeout 003 refresh + handoff 004   ← 上轮交接基线
```

## 2. 架构要点（本轮变化）

### 编辑器 HTTP 服务（`src/rpc/`）—— D1 已 Full
- **路由**：18 条 HTTP 路由覆盖 `IRpcDispatcher` 全部 14 操作（ping/status/assets/run/stop/reload/
  logs/live2d/*/build/eval/debug.getState/getFrame/setBreakpoint/removeBreakpoint/clearBreakpoints/
  continue/inspect）——与 stdio 方法集对齐
- **安全加固**（security_review 驱动，全部 no blocking）：
  - `/api/build`：outputPath/keyPath 限定 `build/` 前缀（拒绝 `..`/绝对路径/反斜杠归一化）
  - CORS：精确 host 白名单（localhost/127.0.0.1 任意端口），evil 子域/短 Origin 403 不崩溃
  - 鉴权：`CAESURA_EDITOR_TOKEN` 环境变量（非 argv——/proc cmdline 世界可读），
    `Authorization: Bearer <token>` 校验，OPTIONS 预检豁免，`constantTimeEquals`（`src/rpc/ConstantTime.h`）
    常量时间比较，`Access-Control-Allow-Headers` 含 Authorization
- **测试**：`tests/headless_http_smoke.py`（21 断言，ctest `CaesuraHeadlessHttpSmoke`）；
  无 GPU runner 下引擎非零退出时 SKIP（exit 77 + `SKIP_RETURN_CODE`），存活未就绪 FAIL

### Live2D（closeout 005/006 收尾）
- `confineToModelRoot` 提取为 `src/live2d/PathConfinement.h/.cpp`（无 SDK 依赖、始终编译）
- 单测 6 用例（绝对路径/`..`/dot/lookalike/symlink），套件 557→563→564

## 3. 构建 / 测试 / 提交约定

- 构建目录：`build-repro-verify`（无 SDK）/ `build-live2d`（`-DCAESURA_LIVE2D=ON`，SDK 在仓库根
  `CubismSdkForNative-5-r.5/`，`.gitignore` 忽略）
- 测试：`cd build-live2d/tests/Debug && ./CaesuraTests.exe`（CWD 需匹配资源路径）；ctest 同前
- 提交格式：`type(scope): description`（feat/fix/test/docs/review/merge/plan）；直接推 master（CI 自动跑）
- origin：`ssh://git@ssh.github.com:443/ailiasdesu/Caesura-AmeKAG.git`（SSH over 443）

## 4. 剩余可选清单（按优先级）

1. **(d) watcher 回调 Lua push SDL 事件自死锁理论**（004 遗留，LOW，**已复查 2026-08-02**）：
   SDL3 从 push 路径也会调用 event watcher；用户 Lua onPause/onResume 回调若再 push SDL 事件，
   会在事件队列锁内同步重入 watcher → 理论自死锁。**当前不可达**：引擎无 Lua 绑定可 push SDL
   事件（MobileAdapter 的 SDL_PushEvent 仅平台层触摸映射直调，不经 Lua 回调）。已在
   `Engine.cpp handleAppLifecycle` 加防御性注释；原生移动输入/事件 marshalling 层落地时复查
2. **Live2D OpenGL 路径验证**：OpenGLShared/OpenGLReadback 仅 Apple/Linux 编译，无平台环境未验证
3. **Metal stub**：`MetalNativeRenderPath` 恒失败回退，需 macOS 开发者实现
4. **编辑器远程暴露设计**：当前 loopback 信任边界 + env token；如需局域网/远程访问需显式设计
5. **Live2D 模型渲染像素级视觉确认**：HTTP 加载 Haru 无崩溃/无设备丢失已验证，画面正确性未人工确认

## 5. 历史交接引用

- `docs/plans/2026-08-01-004-delivery-handoff.md`：004 原始交接（阶段 1-4 状态、CI 调试经验）
- `docs/plans/2026-07-31-003-live2d-mobileadapter-closeout.md`：003（Live2D 路径审计、MobileAdapter）
- `docs/plans/2026-08-01-005-live2d-verified-game-logic-migrated.md`：005（Live2D D3D11 验证+安全修复）
- `docs/plans/2026-08-01-006-editor-http-routes-live2d-tests.md`：006（HTTP debug routes + PathConfinement）
- `docs/plans/2026-08-02-001-ci-editor-hardening-closeout.md`：007（CI 全平台加固 + 编辑器安全闭环）
- **本文件（008）为最新状态**：编辑器 HTTP 服务 D1 Full + 安全闭环、CI 全绿、套件 564/564
