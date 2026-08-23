# Caesura (AmeKAG) — 交接文档（2026-08-23 第 26 号 / Validation-Release 阶段总结）

> 面向后续 agent 的完整上下文。承接 025（WSL Linux 验证 + 跨平台修复）。
> 本文件记录 **Validation-Release Agent 任务书（D:/system doucument/下载/Caesura-AmeKAG_Validation-Release_Agent任务书.md）执行结果**（round 136-141）。

---

## 1. 任务书执行总结

| 任务书项 | 状态 | 交付物 |
|---|---|---|
| §5 P0-1 SoLoud ALSA race | ✅ 根修 | deinit assert 移 join 后（9a6cf38c）+ 回归测试（01fe36ac） |
| §6 P0-2 First-VN E2E | ✅ | tests/projects/first_vn/ + verify_first_vn.sh **13/13**（0f94ab51+3c2d2601） |
| §7 P0-3 ProjectContext | ✅ | src/rpc/ProjectContext.h 统一 resolver（8e07d8a5） |
| §8 P0-4 macOS | ⏳ pending | 无 Mac 真机——如实 external verification pending |
| §9 P0-5 Web 真实验证 | ✅ | CDP 多模态：boot/text/image/input 全通过；修复 autostart（43b57e88）+ index.json（3f0cc03c）；audio/大资源待真实浏览器会话 |
| §10 Steam 真实发布 | ⏳ pending | 无凭据——VDF 模板就绪（标注 pending） |
| §11 第三方验证 | ⏳ pending | 需用户招募 3-5 人 |
| §14 RPC 服务分层 | ✅ | ProjectService/PackagingService/AssetService（36b754d0/d19e3e5a/cdf4348f） |
| §16 Release QA | ✅ | docs/guides/release-qa-matrix.md（诚实分级） |

## 2. 关键架构变更

1. **CAESURA_SOURCE_DIR**（全局 u8 宏，CaesuraBuildOptions）——out-of-tree 构建的源码根单一来源
2. **ProjectContext**——sourceRoot/projectRoot/templatesRoot/assetRoot/buildRoot/outputRoot/platform；业务代码**禁止裸 fs::current_path()**（§7 铁律）
3. **Core Services 模式**——ServiceResult{status, body} + dump(error_handler_t::replace)（中文路径 GBK 字节必须 replace）+ 异常 catch；handler 仅 parse→service→响应
4. **Web autostart**——播放器自动启动首个 bundle 场景（?scene= URL 优先）
5. **package_game.sh 生成 scripts/index.json**（bridge.js 硬依赖）
6. **SoLoud deinit 竞态根修**——断言移到 backend cleanup join 后（精确非尽力）

## 3. 当前基线（全绿）

C++ **997/997**（315807 断言）· Lua **133/132+24** · editor **615/615** · web **298/298** ·
golden **18/18** · http-smoke **72/72** · first-vn **13/13** · coupling PASS ·
CI 三平台 success（最近 32631126392 验证中）

## 4. 方法论沉淀

- **真实案例多模态验证链**：Chrome headless + CDP（remote-debugging-port + node WebSocket）抓 stage DOM/console/截图——比 --screenshot（load 即刻截图早于异步帧）可靠；黑屏误导经验已记录
- **WSL 环境**：源码复制 /root/caesura（括号中文路径）、ALSA null、CMake 3.28、output 落 /d/ 读取
- **GBK 路径编码**：Windows fs::path::string() 返回 ACP 字节——所有 JSON dump 必须 error_handler_t::replace

## 5. 待办（外部资源）

- macOS 真机验证（§8 全链）
- Steam 发布链执行（§10：VDF 模板+指南就绪）
- 第三方 3-5 人验证（§11：需招募，记录首次点击/报错/困惑）
- Web audio/大资源/内存压力浏览器复核（有音频会话语境）

## 6. 后续建议

外部资源就位后按 release-qa-matrix.md 分层验证即可；代码侧无遗留 blocker。
