# Caesura (AmeKAG) 开发上下文锚点

> **投喂规则**: 每次开始新的开发会话时，首先投喂本文件 + 对应 Phase 的计划章节。
> 本文件包含 Codex 必须始终记住的关键约束和架构决策。

---

## 1. 核心架构不变式

### 1.1 Lua↔C++ 唯一通道
- Lua **永远不**直接调用 C 函数或 bgfx/SoLoud/SDL C API
- 所有引擎能力通过单一 `backend` 表暴露
- 资源创建走工厂方法: `backend.load_image()`, `backend.create_viewport()` 等
- 所有句柄统一 `{type, id, generation}` 三元组
- 校验走 `backend.is_valid(handle)` — id+generation 双重匹配

### 1.2 线程模型
- **主线程独占**: bgfx/SoLoud API 仅主线程调用
- 后台线程通过 `SDL_PushEvent` 派发回调到主线程
- **永久无锁** — 不引入自旋锁或互斥锁
- 每处关键代码加 `assert(is_main_thread())`

### 1.3 协程模型
- 全局共享一个 `lua_State`，通过 `lua_newthread` 创建协程
- KAG 脚本作为 Lua 协程运行
- 阻塞操作使用 `coroutine.yield()` (不是忙等待)
- CancelToken 两阶段: 标记取消 → 执行回调 → coroutine.close()
- 回调**禁止** yield 或启动新协程，仅允许释放主线程资源

### 1.4 性能目标
- 目标硬件: Intel HD 620, 1280x720, 60 FPS (<16.67ms/帧)
- 典型场景 <8ms, 转场 +2ms, 视频 +3ms
- 连续 3 帧 >16.67ms 触发自适应降级

---

## 2. 绝对不做的设计决策

| # | 决策 | 原因 |
|---|------|------|
| 1 | 不使用 XP3 容器 | 自研 CARC (AES-256-GCM + Ed25519 + zstd) |
| 2 | 不轮询音频状态 | 使用 SoLoud 回调 + SDL_PushEvent 唤醒协程 |
| 3 | 不直接调用 KAG 解释器 | 使用 LPeg 解析 .ks → Lua 协程执行 |
| 4 | 不使用批量音频提交 | 全局 Direct 模式，所有操作立即执行 |
| 5 | 不每帧创建 LUT 纹理 | 单纹理 + progress uniform + 缓动函数 |
| 6 | 不逐字符提交文本 | MessageLayerCache 顶点缓冲批处理 |
| 7 | 不直接暴露 C++ 指针给 Lua | 所有资源走统一句柄 + generation 校验 |
| 8 | 不使用 load() 反序列化存档 | JSON + schema_version + 迁移脚本 |
| 9 | 不在 [eval]/[emb] 中开放全量 Lua | _ENV 白名单沙箱，Dev/Release 双模式 |
| 10 | 不每帧重建字体纹理 | TextureArray 8 层 2048² 预分配 |

---

## 3. 64 项决策快速索引

### P0 (13 项) — 编码前已解决
1. [10.2.31] GameState 单例 — ctx 全局状态容器
2. [10.2.33] CancelToken — 两阶段取消模型
3. [10.2.5] JSON 存档安全 — 替代 load() 反序列化
4. [10.2.47] [eval]/[emb] 沙箱 — _ENV 白名单
5. [10.2.51] 沙箱开关 — Dev/Release 双模式
6. [10.2.1] Provider Chain — CARC(10) > patch(8) > dir(5)
7. [10.2.28] CARC 公钥外置 — 附加文件末尾 32B
8. [10.2.63] 多 CARC 链式信任 — 证书 + CRL
9. [10.2.10] 签名版本防降级 — SHA-256 含版本号
10. [10.2.7] 文本布局引擎 — 换行/对齐/ruby/CJK
11. [10.2.11] 音频全局 Direct — 所有操作立即执行
12. [10.2.2] 视频 PTS 同步 — 回调机制
13. [10.2.3] 3D 着色器安全 — 64KB 上限 + 文件路径

### P1 (30 项) — Alpha 必须实现
[10.2.12] TextureArray / [10.2.6] Scissor / [10.2.14] 线程安全 /
[10.2.9] Blend+Palette / [10.2.8] 视频图层 / [10.2.13] Resize RTT /
[10.2.4] 热重载 / [10.2.25] LUT 优化 / [10.2.26] 文本批处理 /
[10.2.27] SE per-handle / [10.2.29] 内存预算 / [10.2.30] 启动流程 /
[10.2.32] 异步加载 / [10.2.34] 存档版本 / [10.2.41] 贝塞尔 /
[10.2.42] quake / [10.2.43] 跨版本迁移 / [10.2.46] CARC 自验证 /
[10.2.50] 错误界面 / [10.2.54] 字体降级 / [10.2.57] 异步回退 /
[10.2.58] 句柄验证 / [10.2.60] 内存上限 / [10.2.61] link /
[10.2.62] GPU 预算 / [10.2.59] 缩略图 / [10.2.53] 脏矩形透明 /
[10.2.19] 缩略图配置 / [10.2.21] 协程关闭 / [10.2.22] RTT 并发

### P2 (14 项) — Beta 迭代
[10.2.15] 3D 清除 / [10.2.16] CARC 回滚 / [10.2.24] FFmpeg /
[10.2.23] 动态扩容 / [10.2.35] i18n 深度 / [10.2.36] Lua 5.4 /
[10.2.44] 扩容锁 / [10.2.48] 热重载分析 / [10.2.55] 事件队列 /
[10.2.17] 解码器扩展 / [10.2.38] kag 分期 / [10.2.64] 移动平台 /
[10.2.49] 词汇表

### 已合并条目 (3 项)
[10.2.52] → [10.2.26]+[10.2.7] / [10.2.56] → [10.2.55] /
[10.2.45] → [10.2.11] / [10.2.54] → [10.2.23] / [10.2.59] → [10.2.19] /
[10.2.57] → [10.2.32]

---

## 4. 技术栈

| 层 | 技术 | 版本 |
|----|------|------|
| 语言 | C++17 + Lua 5.4 | - |
| 渲染 | bgfx (cross-platform) | latest |
| 音频 | SoLoud | latest |
| 窗口/输入 | SDL3 | latest |
| 解析 | LPeg | latest |
| 字体 | FreeType | latest |
| 视频(Alpha) | pl_mpeg (MPEG1) | - |
| 视频(Beta) | FFmpeg (H.264/VP9/WebM) | - |
| 压缩 | zstd | latest |
| 加密 | OpenSSL (AES-256-GCM + Ed25519) | latest |
| 测试 | Catch2 (C++) / busted (Lua) | latest |

---

## 5. 每条 KAG 命令的执行路径

```
.ks 文件
  → tokenizer.lua (LPeg 解析为 Token 序列)
  → scheduler.lua (遍历 tokens, 分发 kag[cmd](ctx, params))
  → commands/*.lua (调用 backend.xxx() — 工厂方法)
  → backend.lua (Lua 门面)
  → BackendAPI (C++ 注册器)
  → TextureManager / AudioBackend / LayerManager / ... (子系统)
  → bgfx / SoLoud / SDL3 / FreeType (硬件层)
```

**禁止任何绕过后端层的调用路径。**

---

## 6. 文件路径速查

| 文件 | 用途 |
|------|------|
| `docs/Caesura_功能实现规格说明书_整合版.md` | 完整规格 (64 项决策, Part 0-11) |
| `docs/superpowers/plans/2026-06-06-caesura-implementation.md` | 实现计划 (Phase 0-9 + Phase 0.5) |
| `src/script/BackendAPI.h/.cpp` | C++ 统一注册器 (Phase 0.5) |
| `scripts/backend.lua` | Lua 统一门面 (Phase 0.5) |
| `scripts/kag/commands/` | KAG 标签实现 (Phase 4) |

---

> **最后更新**: 2026-06-06 | **决策总数**: 64 (P0:13, P1:30, P2:14, 已合并:7)
