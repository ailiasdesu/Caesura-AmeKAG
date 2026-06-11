# 待完成事项审查报告

> 审查日期: 2026-06-08 | 引擎 @ Alpha 0.3.0 (`410ec26`)

---

## 一、🔴 未实现 (1 项)

| # | 项目 | 审查结果 |
|---|------|:---:|
| 1 | 移动端 MobileAdapter | ⚠️ **部分属实** — 实际 2 处 TODO（文档说 6 处），已清理 TD-14 标签，存根仍存在 |

---

## 二、🟡 部分实现 (9 项)

| # | 项目 | 审查结果 |
|---|------|:---:|
| 1 | CRC32 反调试 | ❌ **不属实** — 0 行代码，纯文档规划，不构成"部分实现" |
| 2 | HMAC-SHA256 存档签名 | ❌ **不属实** — 0 行代码，纯文档规划 |
| 3 | bsdiff 差分更新 | ❌ **不属实** — 0 行代码，纯文档规划 |
| 4 | 3D 空间音频 | ✅ **属实** — IAudioBackend 接口定义，SoLoud 未接入 |
| 5 | macOS Metal | ✅ **已修复** — CI 已阻塞化测试，shader 完整 |
| 6 | Linux Vulkan/OpenGL | ✅ **已修复** — CI 已阻塞化测试，shader 完整 |
| 7 | 跨平台加密 | ⚠️ **部分属实** — OpenSSL EVP 路径存在但 macOS/Linux CI 未专门测试加密 |
| 8 | Web 编辑器 | ✅ **属实** — 骨架存在，功能未完成 |
| 9 | FFmpeg 后端 | ✅ **属实** — 接口完整，默认 OFF |

---

## 三、技术债务 (18 项)

| ID | 项目 | 审查结果 |
|----|------|:---:|
| TD-01 | render() 两次 pcall | ❌ **不属实** — Engine::render() 仅 1 次 pcall，其他在 update/GC |
| TD-02 | TextRenderer↔FontRenderer 重叠 | ❌ **不属实** — FontRenderer.cpp 不存在，仅 FreeTypeContext + TextRenderer |
| TD-03 | BackendRegistry 三重所有权 | ✅ **已修复** — Engine 持有 unique_ptr，Registry 仅 non-owning raw ptr |
| TD-04 | DebugManager recursive_mutex | ✅ **属实** — `DebugManager.h` 中使用 recursive_mutex，调用路径在主线程 |
| TD-05 | LuaManager 内联 Lua 字符串 | ❌ **不属实** — 0 处 luaL_dostring/luaL_loadstring |
| TD-06 | ParticleSystem BgfxRenderDevice* | ❌ **不属实** — 未引用 BgfxRenderDevice 或裸渲染指针 |
| TD-07 | CryptoEngine 跨平台 | ✅ **属实** — 与 2.7 重复，Windows BCrypt + 跨平台 OpenSSL |
| TD-08 | SaveManager 手写 JSON | ✅ **属实** — 注释写明 "raw string building for JSON" |
| TD-09 | HotReload sleep(50ms) | ✅ **属实** — 第 168 行：`std::this_thread::sleep_for(50ms)` |
| TD-10 | RTTManager @Beta 注释 | ❌ **不属实** — 无 @Beta 注释 |
| TD-11 | FontRenderer 2048x2048 | ❌ **不属实** — FontRenderer.cpp 不存在 |
| TD-12 | IVideoDecoder FFmpeg | ⚠️ **与 2.9 重复** |
| TD-13 | BgfxRenderDevice LUT 图集 | ❌ **不属实** — 无 LUT/colorGrading 代码 |
| TD-14 | MobileAdapter 存根 | ✅ **已修复** — 与 1.1 重复，TD-14 标签已清理 |
| TD-15 | UnifiedBinding↔KAGBinding | ✅ **已修复** — UnifiedBinding 已废弃，BackendFactory 接管 |
| TD-16 | CAESURA_DEBUG 始终为 1 | ✅ **属实** — `#define CAESURA_DEBUG 1`，无 Release 剥离 |
| TD-17 | MobileAdapter namespace 不一致 | ❌ **不属实** — namespace 为 `Caesura`，头尾一致 |
| TD-18 | CRLManager fetchOnline(url) | ✅ **属实** — url 参数冗余 |

---

## 四、Huygens 建议 (13 项)

| # | 建议 | 审查结果 |
|---|------|:---:|
| 1 | STRATEGY.md Track 1 稳定化 | ✅ **已完成** |
| 2 | 添加 CONTRIBUTING.md | ✅ **属实** — 不存在 |
| 3 | 修复 macOS/Linux CI | ✅ **已修复** — 本轮 `6a995a6` 完成 |
| 4 | 去重 scene-template.lua | ✅ **属实** — 两份副本：`docs/templates/` 和 `src/Core/docs/templates/` |
| 5 | 添加 .clang-format | ✅ **属实** — 不存在 |
| 6 | CI 代码覆盖率 | ✅ **属实** — 未实施 |
| 7 | README 对齐 | ⚠️ **部分属实** — README 基本对齐但仅提及 Windows 构建，未说明跨平台 |
| 8 | JSON → nlohmann/json | ✅ **属实** — 手写 JSON 解析器 |
| 9 | 单例 → 依赖注入 | ✅ **属实** — 大量单例模式 |
| 10 | UnifiedBinding vs KAGBinding | ✅ **已修复** — 本轮 `44c8604` 完成 |
| 11 | ENGINE_ANALYSIS.md 维护 | ✅ **属实** — 文档已过时（含多个不实 TD） |
| 12 | 两份 AI 上下文合并 | ✅ **属实** — 实际有 **三份**：`docs/ai/CONTEXT.md`、`docs/strategy/AI-CONTEXT.md`、`src/Core/docs/ai/CONTEXT.md` |
| 13 | 测试 mock 框架 | ✅ **属实** — 无 mock 框架 |

---

## 统计

| 分类 | 数量 | 属实 | 不属实 | 已修复 | 部分属实 |
|------|:---:|:---:|:---:|:---:|:---:|
| 🔴 未实现 | 1 | 0 | 0 | 0 | 1 |
| 🟡 部分实现 | 9 | 4 | 3 | 2 | 0 |
| 高优先级债务 | 8 | 3 | 3 | 1 | 0 |
| 中优先级债务 | 7 | 0 | 4 | 1 | 1 |
| 低优先级债务 | 3 | 2 | 1 | 0 | 0 |
| Huygens 建议 | 13 | 9 | 0 | 3 | 1 |
| **总计** | **41** | **18** | **11** | **7** | **4** |

---

## 仍有效的真实待办

| # | 来源 | 项目 | 优先级 |
|---|------|------|:---:|
| 1 | 1.1/2.14 | MobileAdapter 移动端存根 | P2 |
| 2 | 2.4 | 3D 空间音频 | P2 |
| 3 | 2.7/TD-07 | CryptoEngine 跨平台实测 | P1 |
| 4 | 2.8 | Web 编辑器完善 | P2 |
| 5 | 2.9/TD-12 | FFmpeg 后端启用 | P2 |
| 6 | TD-04 | DebugManager recursive_mutex 冗余 | P2 |
| 7 | TD-08 | SaveManager 手写 JSON → nlohmann | P1 |
| 8 | TD-09 | HotReload sleep(50ms) → 条件变量 | P2 |
| 9 | TD-16 | CAESURA_DEBUG Release 剥离 | P2 |
| 10 | TD-18 | CRLManager fetchOnline(url) 参数冗余 | P3 |
| 11 | Huygens 4 | 去重 scene-template.lua | P1 |
| 12 | Huygens 5 | 添加 .clang-format | P1 |
| 13 | Huygens 6 | CI 代码覆盖率 | P1 |
| 14 | Huygens 7 | README 跨平台说明 | P2 |
| 15 | Huygens 8 | JSON → nlohmann/json | P1 |
| 16 | Huygens 9 | 单例 → 依赖注入 | P3 |
| 17 | Huygens 11 | ENGINE_ANALYSIS.md 更新 | P1 |
| 18 | Huygens 12 | AI 上下文三份合并 | P1 |
| 19 | Huygens 13 | 测试 mock 框架 | P2 |
