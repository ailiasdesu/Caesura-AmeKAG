# Release QA Matrix（任务书 §16 输出物）

> 每次 Release 必须覆盖的验证矩阵。状态分级（§16 必须区分）：
> **Implemented** · **Unit tested** · **CI verified** · **Real hardware verified** · **Browser verified** · **External-user verified**

## Runtime

| 维度 | Implemented | Unit | CI | Real HW | 说明 |
|---|---|---|---|---|---|
| C++ tests | ✅ | ✅ 997/997 | ✅ 三平台 | ✅ Windows/WSL | 996+1（deinit race 回归） |
| Lua tests | ✅ | ✅ 133+24 | ✅ | ✅ Windows/WSL | run_lua + orphan |
| Contract tests | ✅ | ✅ 123 | ✅ | ✅ | ks_check |
| Save/migration | ✅ | ✅（v1..v5 goldens） | ✅ | ✅ | SaveManager 全套 |
| Replay | ✅ | ✅（golden 主路径） | ✅ | ✅ | 非阻塞替代（headless 死等规避） |

## Product（Creator journey）

| 步骤 | Implemented | Unit | CI | Real HW | Browser |
|---|---|---|---|---|---|
| Create | ✅ | smoke | ✅ | ✅ | — |
| Open | ✅ | smoke | ✅ | ✅ | — |
| Import | ✅ | smoke | ✅ | ✅ | — |
| Duplicate | ✅ | smoke | ✅ | ✅ | — |
| Edit | ✅（Monaco+LSP） | editor 615 | ✅ | ✅ | — |
| Run | ✅ | smoke/headless | ✅ | ✅ | — |
| Debug | ✅（断点/单步） | smoke | ✅ | ✅ | — |
| Save | ✅ | goldens | ✅ | ✅ | ✅ Web save 按钮 |
| Load | ✅ | goldens | ✅ | ✅ | ✅ Web 槽位 |
| Build | ✅ | smoke（build 端点） | ✅ | ✅ | ✅ Build 面板 |
| **First-VN E2E** | ✅ | **13/13** | ✅ | ✅ | ✅ 自动启动+推进到 DONE |

## Package

| 平台 | Implemented | CI | Real HW | 状态 |
|---|---|---|---|---|
| Windows | ✅（CPack ZIP） | ✅ | ✅ | 发布路径已验证 |
| Linux | ✅ | ✅ | ✅（WSL 实机 ctest 11/11） | 已验证 |
| macOS | ✅（代码） | ✅（CI 编译 - Clang 严格修复后 2m12s） | ⏳ **pending** | **需 Mac 真机**（§8 诚实标注） |
| Web | ✅（package_game.sh） | ✅ | ✅（Chrome+Edge CDP 验证） | boot/text(含CJK)/image(input img 解码)/input/audio(WebAudio source+autoplay 解锁)/save(跨 reload 持久)/自动启动全通过（2026-08-23 scripts/web_browser_smoke.mjs） |

## Real environment

| 环境 | 状态 |
|---|---|
| Windows hardware | ✅（本机 996/996 + smoke 72/72 + firstvn） |
| Linux hardware/WSL | ✅（WSL ctest 11/11 + 全套件绿；Linux 真卡待硬） |
| macOS hardware | ⏳ **pending**（无设备） |
| Chrome / Edge | ✅（Chrome + Edge headless CDP 多模态验证；Chrome 复现 autoplay suspended→running 解锁；Edge 默认放行） |
| Steam test | ⏳ **pending**（无凭据——§10 诚实标注） |

## Distribution

| 项 | 状态 |
|---|---|
| standalone package | ✅（CPack ZIP + 手动冒烟） |
| Steam test | ⏳ **pending**（VDF 模板就绪，无账号） |

## 本矩阵更新记录

- 2026-08-23：First-VN E2E 13/13；Web CDP 验证 boot/text/image/input；§14 分层（Project/Packaging/Asset Service）；SoLoud deinit race 根修。另：Web Track W0/W1 —— 资产双前缀 404 修复（/assets/assets/→/assets/，图片与音频真实加载）、dev/dist scripts-index 生成、W1 autoplay 解锁（Chrome suspended→running）+ web_browser_smoke.mjs 入库（Chrome/Edge 全部通过，详见 docs/status/web-release-status.md）。
