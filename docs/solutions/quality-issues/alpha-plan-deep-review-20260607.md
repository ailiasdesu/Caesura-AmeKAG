# Caesura Alpha 完成计划 — 深度审查报告

> 审查方式: ce-doc-review (4 并行子代理)
> 文档: docs/plans/Caesura_Alpha完成计划_20260607.md
> 日期: 2026-06-07

---

## 四路审查共识

### ✅ 4/4 共识
- **取消 Phase 1.2 (MobileAdapter)**: 死代码，P2 预留，零调用点
- **推迟 Phase 2.1 (RTT spinlock)**: 单线程 Alpha 无收益，应 Beta 随 Job System 一起实现
- **保留 Phase 1.1 (视频 TODO)**: 用 `luaL_error` 替代静默 `pushboolean(0)`
- **保留 Phase 3+4 (文档+验证)**: 无争议

### ⚠️ 架构审查额外发现

| # | 发现 | 位置 | 严重度 |
|---|------|------|:---:|
| 3 | `m_handleToPoolIndex` 从未填充 (死代码) | RTTManager.h | gated_auto |
| 4a | `destroyCanvas` 立即 flush 违反延迟销毁语义 | RTTManager.cpp:277-281 | manual |
| 4b | 多点触控缺少 fingerId=0 过滤 | MobileAdapter.cpp | gated_auto* |
| 4c | `video_play` 静默返回 false，Lua 无法区分不支持 | RenderBinding.cpp:428 | safe_auto |
| 4d | 关机顺序 `flushAllRTT()` 在 video 之后但 Lua 之前 | Engine.cpp:420-434 | FYI |

*注: 4b 因 MobileAdapter 整体推迟到 Beta 而同样推迟

### ⚠️ 范围审查额外发现

| # | 发现 | 严重度 |
|---|------|:---:|
| 5 | P2 计数不一致: 计划 8/23 vs ALPHA_NOTES 10/16 | manual |
| 6 | 推迟列表缺 [10.2.64] MobileAdapter + [10.2.55] RTT spinlock | manual |

### ⚠️ 产品审查额外发现

| # | 发现 | 严重度 |
|---|------|:---:|
| 7 | 缺少性能回归检查 (GPU <1.0ms, 内存 ~30MB, 二进制 2.47MB) | manual |
| 8 | 未明确重新运行 110 C++ 测试 + 33 Lua 测试 | FYI |
| 9 | FreeType 图集扩容建议加 `// @Beta` 提醒 | safe_auto |
| 10 | IVideoDecoder 接口建议 Alpha 定义 (15 行纯虚头文件) | safe_auto |

### ❌ 可行审查发现 (因 MobileAdapter 取消而 moot)

| # | 发现 | 严重度 |
|---|------|:---:|
| 11 | SDL2/SDL3 命名不匹配: SDL_MOUSEBUTTONDOWN → SDL_EVENT_MOUSE_BUTTON_DOWN | manual |
| 12 | SDL_MouseButtonEvent 12 字段需正确初始化 (which=SDL_TOUCH_MOUSEID) | manual |

---

## 修订后 Alpha 完成计划

| Phase | 内容 | 文件 | 行数 |
|:-----:|------|------|:---:|
| 1.1 | 视频 TODO → `luaL_error` + 注册 `video_play` + 删除 `dynamic_cast` | RenderBinding.cpp | ~10 |
| 1.2 | RTTManager `m_handleToPoolIndex` 死代码清理或填充 | RTTManager.h/cpp | ~20 |
| 1.3 | `destroyCanvas` 移除立即 flush，改为纯入队 | RTTManager.cpp:277 | ~5 |
| 1.4 | IVideoDecoder 纯虚接口定义 | Render/IVideoDecoder.h (新) | ~15 |
| 1.5 | FontRenderer 图集扩容 `// @Beta` 提醒 | FontRenderer.cpp | ~3 |
| 2 | 文档更新: CONTEXT + ALPHA_NOTES + P2 推迟列表枚举 | 2 files | ~15 |
| 3 | 集成验证: Debug+Release 构建 + 110+33 测试 + 性能基线 | — | — |

**修改量**: ~70 行 C++ + ~15 行文档
**推迟**: MobileAdapter, RTT spinlock, 15 项 P2