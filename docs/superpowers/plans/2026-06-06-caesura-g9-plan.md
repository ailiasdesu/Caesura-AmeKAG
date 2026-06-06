---
title: "Caesura G9: 集成验证 + Alpha 收尾 (v2 — 审查修正)"
type: plan
status: draft
date: 2026-06-06
origin: docs/superpowers/plans/2026-06-06-caesura-implementation.md
review: ce-doc-review r1 (7 findings, all applied)
execution: code
---

## Summary

G8 完成了异步加载、对象池、CJK 字体回退。G9 聚焦端到端集成验证 — 先审计 P1 决策实现状态，再分批跑通 full_story.ks（46 种 KAG 标签），验证存档/读档/CARC 完整闭环，补全存根功能，验证错误界面和性能目标。

## P1 决策审计 (U0 产出后填充)

> U0 完成后替换此表

| 状态 | P1 决策 | 对应模块 |
|------|---------|----------|
| ✅ | (待填充) | |
| ⚠️ | (待填充) | |
| ❌ | (待填充) | |

---

## G9 Units

### U0: P1 决策现状审计 🔴 (新增)

**目标:** 遍历 30 项 P1 决策，逐项标记实现状态，生成差距表驱动后续单元。

**对照来源:** `CONTEXT_ANCHOR.md` §3 P1 列表 + `src/` + `scripts/`

**Approach:**
1. 逐项检查 30 项 P1 决策在代码中的实现证据
2. 标记 ✅ (已实现) / ⚠️ (部分实现/存根) / ❌ (未实现)
3. 对 ⚠️/❌ 项评估对 Alpha 的阻塞程度
4. 输出差距表，映射到 U5a/U5b/U5c 的精确范围

**Risks:** 部分 P1 决策跨多个模块，需要全局搜索

---

### U1: 集成测试重新验证 + G8 回归修复 🔴

**目标:** 确保现有 26 项集成测试全绿，修复 G8 引入的回归。

**策略:** 先跑 → 建基线 → 按优先级修复 → 每修复一项重跑

**Files:**
- `tests/test_integration.lua` — 26 项集成测试
- `tests/test_full_story_parse.lua` — 全量解析测试
- `scripts/layers.lua` — G8 池化集成（可能引入回归）
- `src/Core/Engine.cpp` — G8 事件处理（可能引入回归）

**Risks:** layers.lua 池化修改、Engine.cpp 事件处理修改可能导致现有测试 FAIL

---

### U2: full_story.ks 分批端到端执行 🟡

**目标:** 分批跑通 full_story.ks 全部 46 种标签。

**46 种标签按优先级分 3 批:**

| 批次 | 标签 | 数量 | 策略 |
|------|------|------|------|
| **高频** | ch, text, l, r, er, p, bg, fg, cl, image, wait, playbgm, stopbgm, playse, stopse, playvoice, fadebgm, pt, font, reset | 20 | 先跑，高影响 |
| **关键路径** | jump, save, load, saveplace, loadplace, if, endif, call, return, link, eval, emb, skip, ruby, layopt | 15 | 次跑，流程依赖 |
| **高级功能** | trans, move, position, fade, quake, vfx, video, stopvideo, preload, history, end | 11 | 末跑，独立功能 |

**Approach:**
1. 按批次顺序执行，每类标签跑通后记录
2. 对每个 FAIL 标签：检查 Lua 命令实现 → 转入 U5 对应子单元
3. 每批通过后，记录通过率和帧时间

**Risks:** 高频批次中 playvoice 依赖音频回调（可能阻塞），eval/emb 需要沙箱（复杂度高）

---

### U3: 存档/读档闭环 + 4 专项验证 🟡

**目标:** save→退出→load 完整循环 + 4 项新增验证点。

**专项验证点:**
1. **协程位置恢复** — load 后执行的下一个 token 与 save 时一致
2. **CancelToken 状态** — save 时活跃的 CancelToken，load 后行为正确（不重复触发）
3. **音频恢复** — load 后 BGM/SE 状态与 save 时一致
4. **纹理句柄有效性** — load 后持有的纹理 handle 通过 `is_valid()` 检查

**文件检查点:** `saves/slot_01.json` 必须包含：ctx.tokens, ctx.co_state, ctx.f/sf/tf, ctx.layers, ctx.backlog, ctx.active_cancel_tokens

**Files:**
- `src/System/SaveManager.cpp`
- `src/Scripting/SaveBinding.cpp`
- `scripts/kag/commands/save.lua`

**Risks:** GameState 序列化可能遗漏 Backlog/CancelToken，需要检查

---

### U4: CARC 打包→加密→引擎读取 闭环 🟡

**目标:** 验证 CARC 容器完整工具链，**包含密钥生成**。

**Steps:**
1. **Step 0:** 用 CryptoEngine 生成 Ed25519 测试密钥对 (test_private.key + test_public.key)
2. **Step 1:** 用 carc_pack.exe + test_private.key 打包 tests/fixtures/ → test.carc
3. **Step 2:** 验证 test.carc header 签名和 Index 区块 (AES-256-GCM)
4. **Step 3:** 引擎通过 CarcAssetProvider 读取 test.carc 中的资源
5. **Step 4:** 验证 Provider Chain 优先级 (CARC 10 > dir 5)
6. **边界测试:** 损坏 CARC、签名不匹配、旧版本号

**Files:**
- `src/carc_pack/main.cpp`
- `src/Carc/CARCWriter.cpp` / `CARCReader.cpp`
- `src/Carc/CryptoEngine.cpp`
- `src/Carc/CarcAssetProvider.cpp`

**Risks:** 密钥生成路径可能缺失，CryptoEngine 可能缺少独立的 keygen 入口

---

### U5a: 高频标签补全 🟡

**范围:** ch, text, l, r, er, p, bg, fg, cl, image, wait, playbgm, stopbgm, playse, stopse, playvoice, fadebgm, pt, font, reset

**目标:** 端到端执行通过（不是仅解析通过）

**验证:** 每个标签跑对应 full_story.ks 片段

---

### U5b: 关键路径补全 🟡

**范围:** jump, save, load, saveplace, loadplace, if/endif, call, return, link, eval, emb, skip, ruby, layopt

**特殊项:**
- [eval]/[emb] — 需实现 _ENV 白名单沙箱
- [if]/[endif] — 需实现条件求值
- [call]/[return] — 需实现调用栈
- [link] — 需 CancelToken 清理 + 跨脚本跳转

---

### U5c: 高级功能补全 🟢

**范围:** trans, move, position, fade, quake, vfx, video, stopvideo, preload, history, end

**降级策略:** 若 video/vfx 依赖未满足，标记为 SKIP 非 FAIL，不阻塞 Alpha

---

### U6a: ErrorUI Level 1 + Level 3 自动化验证 🟢

**Level 1 (Lua 错误触发):**
1. 注入语法错误的 .ks 脚本 → 验证 ErrorUI 弹出
2. 点击 [Retry] → 验证重新加载
3. 点击 [Title] → 验证返回标题

**Level 3 (全局计数器保护):**
1. 连续触发 3 次 Title 崩溃 → 验证全局计数器 → 退出
2. 验证退出后无残留进程

**Level 2 (bgfx 故障):** 手动验证，不在自动化范围

**Files:**
- `src/Core/ErrorUI.cpp` / `ErrorUI.h`

---

### U7: Lua 内存 OOM 验证 🟢

**目标:** 验证 G8-U1 的 256MB 硬上限 + 三级响应。

**测试用例:**
1. 创建大量临时表逼近 204MB (80%) → 验证 GC step 触发，不崩溃
2. 继续分配至 243MB (95%) → 验证全量 GC + 音频缓存清理
3. 继续分配至 260MB (>256MB) → 验证 lua_Alloc 返回 NULL → ErrorUI 弹出

**Files:**
- `src/Core/Engine.cpp` — lua_Alloc 钩子 + 帧末检查

---

### U8: 性能基准烟雾测试 🟢

**目标:** 验证 HD 620 / 1280x720 / 60 FPS 目标可达。

**方法:**
1. 运行 full_story.ks 高频部分 (ch/bg/fg)
2. GpuMonitor 采样 100 帧
3. 验证 P95 帧时间 < 16.67ms
4. 不强制 PASS（开发机可能高于 HD 620），但建立参考基线

---

## Execution Order

```
U0 (P1审计) → U1 (测试基线) → U2 (分批执行) ──┬── U3 (存档闭环)
                                              ├── U4 (CARC闭环)
                                              └── U5a/b/c (功能补全，与U2交织)
                                                         ↓
                                              U6a (ErrorUI) → U7 (OOM) → U8 (性能)
```

U0 先做（决定 U5 精确范围），U1 紧随（稳固基础），U2/U3/U4/U5 交织进行（U2 发现 FAIL → U5 修复 → U3/U4 独立验证），U6-U8 收尾。

## Success Criteria

- [ ] P1 差距表完整，所有 ❌ 项有明确负责单元
- [ ] 26 项集成测试全绿
- [ ] full_story.ks 高频 20 标签 + 关键路径 15 标签端到端通过
- [ ] 存档→退出→读档 4 专项验证全通过
- [ ] CARC 密钥生成→签名→打包→验签→读取 完整链通过
- [ ] ErrorUI L1 弹出/L3 计数器保护 验证通过
- [ ] Lua OOM 三级响应正确触发
- [ ] 性能基线已记录（P95 < 16.67ms 参考值）
- [ ] cmake --build build 无错误

## Verification

```
lua tests/test_integration.lua
lua tests/test_full_story_parse.lua
./build/Debug/CaesuraAmeKAG.exe --script tests/scripts/full_story.ks
./bin/Debug/carc_pack.exe --key test_private.key tests/fixtures test.carc
```
