# Caesura (AmeKAG) — 交接文档（2026-08-01）

> 面向后续 agent 的完整上下文。先读 `AGENTS.md`（模块边界铁律），再读本文档与
> `docs/design/engine-architecture-topology.md`。**本文件为最新状态**；历史交接见
> `docs/plans/2026-07-31-002-continuation-handoff.md`（原交接）与
> `docs/plans/2026-07-31-003-live2d-mobileadapter-closeout.md`（closeout）。

## 1. 当前状态

- **分支**：`master`，HEAD = **`7303fef4`**（`feat(platform): IMobileAdapter interface + Engine lifecycle wiring`）
- **CI**：三平台全绿（Windows MSVC ×2、macOS Clang、Linux GCC）；HEAD 对应运行
  **run 30687137418 = success**（`gh run list` 可查）
- **本地验证基线**：doctest **557/557**（断言 **2748**）；ctest **9/9**；
  `python scripts/count_coupling.py --ci` **PASS**
- **工作树**：干净（仅 `.reasonix/` 未跟踪，非项目产物，勿提交）

### 最近提交（自 `06a15af7` 交接基线起，按时间序）
```
06a15af7    docs: continuation handoff — CI green, phase 1-4 status, technical context   ← 基线
83d94650..265e5108   fix/docs(platform): MobileAdapter 加固——触摸状态机 + pinch→wheel 映射、
                     Lua 栈平衡、NaN/Inf 拒绝（含 003 closeout 文档）
4daf7771..e3d2b605   feat/fix(archive): DeltaCARC v2——hash 寻址明文 delta 真实增量应用、
                     溢出安全解析、严格指针差边界（+7 测试用例）
8287cbc0    docs: sync suite counts to 555 cases (post-DeltaCARC)
c0eb64b0..205054e2   refactor/test(script): SaveManager Path B 清理——System.save/load/
                     _write_save_file/_load_from_file、_apply_config、get_save_info/clear_saves 移除，
                     受跟踪 fixtures 迁移至 KAG.save_game/load_game
80fbf1f6 / e4ed06d6 / caa1f072   fix(archive): DeltaCARC 空体读写 UB 防护 + readU32/U64 严格边界
7303fef4    feat(platform): IMobileAdapter interface + Engine lifecycle wiring   ← HEAD
```

## 2. 架构要点

### MobileAdapter 已接线（7303fef4）
- **接口**：`src/platform/api/IMobileAdapter.h`，纯虚 **14 方法**——lifecycle `onPause`/`onResume`
  （onResume 携 savedData）；touch 映射 `onFingerDown/Motion/Up`；手势 `onPinch`/`resetPinch`/
  `getLastPinchScale`/`onLongPress`；显示 `getDisplayScale`/`setDisplayScale`；状态 `isPaused`/
  `activeTouchCount`/`isFingerDown`
- **DI**：`BackendRegistry::setMobileAdapter/getMobileAdapter`（`src/di/BackendRegistry.h`，仅依赖
  `IMobileAdapter` 接口，符合模块边界铁律）
- **组合根**：`Engine::init` 创建具体 `MobileAdapter` 并注册（`src/entry/Engine.cpp`），
  `SDL_AddEventWatch(&Engine::appLifecycleWatch, this)`
- **事件路径**：SDL 将 WILL_ENTER_BACKGROUND / DID_ENTER_FOREGROUND **仅分发到 event watch、
  不进 poll 队列** → watcher 内同步调用 onPause/onResume（带当前 Lua state）
- **shutdown**：首行 `SDL_RemoveEventWatch`，后台事件不得触及正在拆除的 Lua state；
  随后 `setMobileAdapter(nullptr)`
- **线程契约**：平台层须在引擎/主线程投递 app 事件（iOS/Android 为真）；若原生层在其他线程派发，
  必须先 marshal 到引擎线程再触碰 Lua（注释已写明）
- `Engine.h` 现 include `<SDL3/SDL.h>`

### SaveManager（Path B 已清除）
- 仅存 **C++ JSON 路径**：`KAG.save_game/load_game/list_saves/delete_save`
  （`src/script/bindings/SaveBinding.cpp`，路由到 `src/storage/SaveManager.cpp`）
- 旧 Lua 序列化路径（`scripts/system.lua` `System.save/load` 系列）已全部移除（c0eb64b0..205054e2），
  源码中仅剩注释提及

## 3. 构建/测试/提交约定

- **铁律**：见 `AGENTS.md`——模块边界（跨模块只 include `src/<module>/api/I*.h`，具体实现头只在
  `src/entry/` + `src/main.cpp`）、禁止绕过 `BackendRegistry`、组合根才创建具体后端、测试只增不减、
  耦合度目标（entry/di/script ≤14，其他 ≤4）
- **配置+构建**（Windows 主开发机）：
  ```powershell
  cmake -B build-repro-verify -DCAESURA_LIVE2D=OFF
  cmake --build build-repro-verify --config Debug --parallel
  ```
- **全量测试**（**CWD 必须在** `build-repro-verify/tests/Debug`）：
  ```powershell
  cd build-repro-verify/tests/Debug && ./CaesuraTests.exe
  ctest -C Debug --test-dir build-repro-verify
  python scripts/count_coupling.py --ci
  ```
- **提交格式**：`type(scope): description`（feat/fix/refactor/test/docs/chore…）；提交前全量构建 + 测试全绿
- **推送注意**：22 端口 SSH 被当前网络环境阻断，`origin` 已切换为
  `ssh://git@ssh.github.com:443/ailiasdesu/Caesura-AmeKAG.git`（SSH over 443，`.git/config` 已配）

## 4. 剩余可选清单（按优先级）

1. **(a) Live2D Cubism SDK 验证**：需手动下载 SDK（`-DCAESURA_LIVE2D=ON -DCUBISM_SDK_ROOT=...`），
   验证路线图见 `docs/guides/live2d-setup.md`；Metal 渲染路径需 macOS 开发者实现
2. **(b) 编辑器 HTTP debug routes**：D1 仍为 Partial——stdio JSON-RPC 已实现并经
   `tests/headless_rpc_smoke.py` 端到端覆盖，HTTP 路由未落地
3. **(c) 本地 `scripts/game_logic.lua` 迁移**：该文件在 `.gitignore`（本地脚本，未入库），
   待迁移到 `KAG.save_game/load_game`（受跟踪 fixtures 已迁移，a519a662）
4. **(d) watcher 回调内 Lua push SDL 事件的理论自死锁**：security_review 判定 **LOW**——
   onPause/onResume 触发的 Lua 回调若再 push SDL 事件，watcher 内同步执行理论上有自死锁风险；
   原生移动层落地时复查
5. **(e) DeltaCARC 输出内容级 SHA**：评审判定**无收益**——索引集合验证已覆盖输出完整性，
   无需再加内容级哈希

## 5. 历史交接引用

- `docs/plans/2026-07-31-002-continuation-handoff.md`：原交接（2026-07-31），阶段 1–4 状态、
  关键 CI 调试经验、编辑器/RPC 上下文
- `docs/plans/2026-07-31-003-live2d-mobileadapter-closeout.md`：closeout（2026-07-31），
  Live2D 路径审计、MobileAdapter 实现细节与测试方法
- **本文件（004）为最新状态**：MobileAdapter 已接线、SaveManager 清理完成、套件 557/557
