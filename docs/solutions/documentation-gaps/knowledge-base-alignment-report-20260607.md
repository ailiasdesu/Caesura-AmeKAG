# Caesura 全局知识库对齐报告

> 基准: `docs/Caesura_功能实现规格说明书_整合版.md` (796行, 74项决策)
> 验证: 2026-06-07 代码库实际状态
> 
> ## 概述

对比了 5 个知识库文件与规格说明书 + 代码实物，发现 **2 个文件需修改、3 个文件已对齐**。

---

## 修改清单

### 🔴 CONTEXT.md — 7 项修正

| # | 位置 | 旧值 | 新值 | 原因 |
|---|------|------|------|------|
| 1 | 状态表 P0 | `13/13` | `16/16` | 新增 [10.2.66] 密码学RNG + [10.2.67] RTT flush + [10.2.70] ShaderCache |
| 2 | 状态表 P1 | `30/30` | `32/32` | 新增 [10.2.68] FreeType共享 + [10.2.69] LRU逐出 |
| 3 | 状态表 P2 | _(缺失)_ | `10/16 (62%)` | 新增一行 |
| 4 | 状态表 测试 | `40/40` | `110 C++ + 33 Lua` | 本轮全量补齐 |
| 5 | 已知问题 #2 | Tokenizer 82/167 | **已删除** | Tokenizer 已 154/154 |
| 6 | 已知问题 #4 | ShaderCache 未预注册 | **已删除** | compileVariant fallback + precompileCommon 已连线 |
| 7 | 关键文件索引 | `478行, 68项决策` | `796行, 74项` | 说明书已扩充 |

### 🟡 CONCEPTS.md — 确认无冲突 (7/7 项校验通过)

| 检查项 | 状态 |
|--------|:--:|
| ShaderCache = 排他所有者 (bgfx::destroy 在 shutdown) | ✅ |
| Font/Text 共享 FreeTypeContext | ✅ |
| CARC = AES-256-GCM + Ed25519 + CRL | ✅ |
| 三总线 BGM/Voice/SE×8 | ✅ |
| LRU WaveCache = list + unordered_map | ✅ |
| CryptoEngine BCryptGenRandom 禁止 mt19937 | ✅ |
| Sandbox Dev 只读子集 + Release 移除 | ✅ |

### 🟢 ALPHA_NOTES.md — Phase 5 已更新 (无需修改)

| 字段 | 值 |
|------|-----|
| P0 | 16/16 ✓ |
| P1 | 32/32 ✓ |
| P2 | 10/16 ✓ |
| C++ 测试 | 110 ✓ |
| KAG 标签 | 154/154 ✓ |
| Lua 测试 | 33 ✓ |

### 🟢 架构参考 (docs/solutions/architecture-patterns/) — 完全对齐

- 9 个 src/ 模块全部存在 ✅
- Audio→Render / Render→Scripting / Carc→Core 禁止依赖全部 CLEAN ✅

### 🟡 规格说明书 (docs/Caesura_功能实现规格说明书_整合版.md) — 1 项注记

- 约 20% 内容存在 **GBK→UTF-8 转换损毁** (Part 0-4 标题、Appendix D 中文描述)。
  英文/数字/代码全部完整可读，决策项索引无损失。
  建议: 不影响使用，下次大规模编辑时从原始备份重建。

---

## 规则变更摘要

| 规则 | 操作 |
|------|:--:|
| `单线程独占 bgfx/SoLoud` | ✅ 保留 |
| `CAESURA_ASSERT_MAIN_THREAD() 守护` | ✅ 保留 |
| `无锁设计 (Alpha)` | ✅ 保留 |
| `Audio→Render 禁止` | ✅ 保留 |
| `Render→Scripting 禁止` | ✅ 保留 |
| `Carc→Core 禁止` | ✅ 保留 |
| `ShaderCache 为 ProgramHandle 排他所有者` | ✅ 保留 |
| `FreeTypeContext 全局共享` | ✅ 保留 |
| `Tokenizer 154/154 完成` | 🆕 新增 |
| `CARC 启动自验证已连线` | 🆕 新增 |
| `测试数: 110 C++ + 33 Lua` | 🔄 更新 |
| `P0: 13→16, P1: 30→32` | 🔄 更新 |
| `已知问题 #2 Tokenizer 未完成` | ❌ 删除 |
| `已知问题 #4 ShaderCache 未预注册` | ❌ 删除 |
| `已知问题 #3 AsyncLoader 未绑定 Lua` | ⚠️ 保留 (仍为实际缺口) |