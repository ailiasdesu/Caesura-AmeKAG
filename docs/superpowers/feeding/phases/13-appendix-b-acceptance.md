## 附录 B: 各 Phase 独立验收标准

| Phase | 独立可验证 | 验收命令 |
|-------|:---:|------|
| 0 | ✅ | `./caesura.exe` → 窗口 + 三角形 + Lua print |
| 1 | ✅ | `busted tests/lua/scheduler_spec.lua` → all pass |
| 2 | ✅ | 渲染测试图片到 bg 图层 |
| 3 | ✅ | 播放 test.ogg, 确认回调触发 |
| 4 | ✅ | `busted tests/lua/kag_commands_spec.lua` → all pass |
| 5 | ✅ | 切换混合模式 + LUT, 确认视觉变化 |
| 6 | ✅ | 存档 → 退出 → 读档, 对比状态 |
| 7 | ✅ | `carc_pack data/ test.carc` → 引擎读取 test.carc |
| 8 | ✅ | 修改 .ks → 自动重载 |
| 9 | ✅ | `[eval] os.execute('calc')` → 报错 |
