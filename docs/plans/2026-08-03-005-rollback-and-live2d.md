# 2026-08-03-005 — rollback 回滚 + Live2D SDK 本地化 + 文档校准

## 背景

市场对比（docs/design/engine-market-comparison.md）指出与 Ren'Py 的最大差异化缺口是 rollback 回滚；
Live2D SDK 专有许可不可入库，需用户自行安装；文档漂移 6 处（README 28/480、矩阵 42/555、接口 28/20 等）。

## 变更

### rollback 回滚（新增）
- `scripts/kag/snapshot.lua`（新）：token 级快照 capture/restore（f/sf/tf/mp/variables 深拷贝整体替换、
  call_stack/seen_scenes 深拷贝、tokens/macros/characters/backlog 引用、text_state 深拷贝、
  layers.capture_snapshot/restore_snapshot、backlog 截断、voice 停止）
- `scripts/kag_runner.lua`：start 初始化 `_undoStack`（上限 64）；on_click 前进前压栈（reveal 动画中跳过）；
  update 死协程分支加 `_pendingRollback` 重生成；场景变更清栈；新增 `rollback()`（choice 打开时拒绝）
- `scripts/kag/commands/system.lua`：`[rollback]` 命令
- 约束：不跨选择支/宏调用点/场景；音频只停 voice 不卷 BGM；restore 后整行显示（不打字机）
- `tests/scripts/test_rollback.lua`（新，5 用例）：capture/restore 往返、深拷贝隔离、backlog 截断、
  reveal 补全、场景/索引恢复

### Live2D SDK 本地化（用户自行安装）
- `CMakeLists.txt`：CUBISM_SDK_ROOT 探测顺序 = 显式 → `thirdparty/CubismSdkForNative-5-r.5` → 项目根目录
- `docs/guides/live2d-setup.md`：推荐 thirdparty/ 放置、不入库说明、MSVC v143 前提、探测顺序
- SDK 已本地解压 thirdparty/（.gitignore 排除，git 不入库）；`CAESURA_LIVE2D=ON` 构建验证 569/569

### 文档校准（6 处漂移）
- README：28→30 接口、480→569 测试、29→30、9→18 端点、51→52 文件、42→43 能力
- engine-capability-matrix：42→43、6→7 capabilities、555→569 用例
- cpp-interfaces：28/20→30/21、EditorServer 已接入、端点表补全
- editor-api-reference：CORS localhost 白名单、500→200 日志、run/eval 已实现、HTTP 调试已开放、28→30
- kag-commands：System Commands 4→6（补 unlock/rollback）、8→10 类别、Flow 11→17、Resource 6→5

## 验证

- Lua 套件：tokenizer 10 + kag_commands 9 + scheduler 10 + rollback 5 + sandbox 5 全过
- CaesuraTests：569/569 ×2（build-repro-verify + build-live2d）
- 耦合度 PASS
- Live2D ON 构建：SDK 自动探测成功、D3D11 路径编译+测试全绿
