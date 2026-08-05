# Caesura (AmeKAG) 引擎架构、能力与市场对比（2026-08-03）


> **Historical snapshot (2026-08-03).** Capability counts cited here (68 commands /
> 43 capabilities) predate the Neo-Genesis contract system. Current authoritative
> numbers: 69 contract commands (`docs/api/command-contracts.md`) and 48 capabilities
> (`docs/design/engine-capability-matrix.md`).

> 调研记录：基于 AGENTS.md/CLAUDE.md、docs/design/engine-capability-matrix.md、
> docs/api/kag-commands.md、docs/api/lua-modules.md、docs/api/cpp-interfaces.md、
> docs/design/engine-architecture-topology.md、scripts/ 与 src/ 现状；市场部分
> 以官网/维基/GitHub 在线核实为主。

## 一、架构概览

### 模块拓扑（16 模块 · 15 API 接口目标 · 单可执行文件）
- archive（CARC/AES-256-GCM/Ed25519/DeltaCARC）、audio（SoLoud/Null）、debug（日志/DebugProtocol/HotReload）、
  di（BackendRegistry/SandboxQuota/TextureBudget）、entry（组合根）、input、job（JobSystem/Null）、
  live2d（Cubism/Null）、minigame（Bgfx/Null）、platform（SDL3/Null/MobileAdapter）、
  render（bgfx/Null，6 接口）、resource（AsyncLoader/ProviderChain/XP3）、rpc（RpcServer/EditorServer）、
  script（Lua VM/绑定/GameState）、steam（条件编译）、storage（SaveManager/迁移/云 provider）
- 规则：每模块仅经 api/I*.h 暴露；组合根唯一例外；BackendRegistry 只 include 接口头；
  耦合阈值 entry/di/script ≤14、其他 ≤4（count_coupling.py --ci）

### DI 与数据流
- BackendRegistry 存非拥有 I* 指针，Engine 持 unique_ptr；init 四阶段幂等回滚
- .ks → tokenizer.lua（LPeg，KAG 3.0 命令语法）→ scheduler.lua（协程+流控）→ kag[cmd]
- KAG+Lua 混合：eval/emb/iscript 内嵌，kag.* 反向回调

## 二、能力清单（43 项 · 6 域）

| 状态 | 能力 |
|---|---|
| ✅ | 3 层合成+脏矩形、异步纹理+预算+LRU、2D GPU 粒子、视频（pl_mpeg+FFmpeg+音频+帧率步进）、GPU 降级、FreeType/CJK/ruby 文字、RTT、批量绘制 |
| ✅ | Lua 5.4 协程、KAG 68 命令、流控、指令预算、热重载、错误恢复、打字机/skip（auto/read-skip 为脚本层实现，未单测） |
| ✅ | 三总线音频、3D 音效、原始 PCM、加密存档（AES-GCM）、schema 迁移、CARC+签名、资产链、编辑器 RPC（HTTP 18 路由+stdio 14 操作）、调试器、无头模式 |
| ⚠️ | Live2D（D3D11 验证；GL 未验、Metal stub）、FFmpeg 真机调参、minigame GPU、Steam/云存档远程、非 D3D shader |

## 三、市场对比（9 引擎，在线核实）

| 引擎 | 脚本 | 渲染 | 存档 | 平台 | 优势/短板 |
|---|---|---|---|---|---|
| Ren'Py | 声明式+Python | 2D+ATL | 多槽+rollback | 最广 | 生态 8000+/免费；无 3D/Live2D |
| KiriKiri2/KAG3 | TJS2+KAG | 2D+粒子 | 内置 | Win | 日系标准；停更/GPL |
| 吉里吉里Z | KAG3 | GPU | 内置 | Win | E-mote 官方；多平台未完成 |
| NScripter | 行脚本 | 简单 | 内置 | Win | 经典；停更 |
| ONScripter | 兼容 | 老特效 | 自带 | 全平台 | 移动跑老作 |
| Tyrano | HTML5/拖拽 | Web+Live2D | Web | Web/移动 | 快发；性能上限 |
| Unity+Naninovel | NaniScript | Unity 全 | 开箱全套 | 含主机 | 全套；闭源付费 |
| Unity+Fungus | 节点 | Unity 全 | 基础 | 全平台 | 易上手；停更 |
| Godot+DialogueManager | 对话文件 | Godot 全 | 不内置 | 全平台 | 轻量；只管对话 |

## 四、定位与差距
- 定位：KiriKiri 路线的现代重构（KAG 标签 + Lua + bgfx 多后端）
- 差距：生态/可视化编辑器、rollback、移动发布、未验证面（Live2D GL/Metal、FFmpeg、Steam）、文档漂移

## 五、可实现功能建议（按优先级）
1. rollback 回滚（token 级状态快照，复用存档序列化）
2. 可视化 Web 编辑器（RPC 路由已全，缺前端）
3. 文档漂移校准（41→43 能力、28→30 接口、测试数）
4. 移动端完善（MobileAdapter 已接，缺构建脚本+IME）
5. 消息历史 UI（backlog 数据已有）
6. Steam 实机验证
7. Live2D OpenGL 路径验证 + minigame GPU CI
8. KAG3 脚本导入器（生态入口）


---

# 2026-08-05 更新：迭代后能力、性能与市场定位（60ae346b..HEAD 全量）

> 2026-08-03 至 08-05 期间 master 线性推进 119+ 个提交（60ae346b..HEAD 随更新增长），
> 全部 CI 三平台验证、review/security_review 覆盖、569/569 测试 + Lua 12/12 全绿。

## 一、迭代后能力增量（相对初版文档）

### 性能（实测，见下）
- 纯色纹理 RGBA 去重、纹理 path→id 缓存、字形查找合并探测、提交批级纹理解析缓存
- 打字机切片缓存、文本渲染完整 key 缓存（零字形遍历）、pen advance 缓存
- 粒子 O(1) 槽分配、Debug 热路径跳过格式化、Lua 指令钩子间隔 10 倍化
- 性能基准模块（test_benchmark.lua）作为回归守门 oracle

### 差异化功能（31 项）
- **标题菜单全家桶**：New/Load/Settings/Exit + Continue（autosave 槽 0）+ Endings 画廊
- **剧情系统**：[ending] 解锁/重播、[chapter] 章节选择+已读徽标、token 级 [rollback]
- **文本与节奏**：打字机、[auto] 自动前进（等语音）、read-skip + skip_auto 强制跳过、
  Ctrl 按住跳过、A 键 auto 热键、[wait]/[pt] 钳制
- **语音**：[ch voice=] 播放+backlog 存储、V 键游戏内重播、backlog F 键过滤
- **特效**：[scroll] ED 滚动、[flash]/[blur] 复活、KiriKiri 转场方法别名（dissolve/gradient/mask/slide）
- **KAG3 兼容**：13 个日系命令别名（r/s/delay/clear/ld/shake/play/voice/se…）+ [ct]/[waitforclick]
- **UI**：backlog 重写可用、F4 开发 HUD、toast 通知、快速存读档反馈、自动存档定时器+设置
- **其他**：立绘随说话人显示（[ch sprite=]）、内置 Web 编辑器（18 RPC 路由）、
  语言持久化、CWD 无关启动、存档槽位 API 全守卫

### 稳定性（多智能体审查 17+ 真实缺陷修复）
- **BLOCKER 级**：UI 整体不可见（NDC 缺失+setUniform OOB）、文本 1px 方块、
  纹理重复加载泄漏、音频 UAF、任务 High 优先级饿死、孤儿协程软锁×2、
  rollback 实际不可用×2、io.open 沙箱过严致跨场景失败
- **HIGH/MEDIUM**：解码缓冲泄漏、表达式缓存无界、异步取消竞态、历史跳转白名单、
  backlog 语音路径校验、token_index 钳制、槽位边界、scissor/opacity 渲染污染

## 二、性能实测（vs 2026-08-03 初始基线）

| 指标 | 初始基线 | 当前 | 提升 |
|---|---|---|---|
| tokenizer | 587ms / 146.75ms-per-1000tok | 478-541ms / 119.50-135.25ms | **快 8-19%** |
| scheduler | 4001 resumes / 14ms（≈286k tok/s） | 4001 resumes / 13.0ms（≈308k tok/s） | **快 ≈8%** |

零退化；基准文档：docs/plans/2026-08-04-006-perf-baseline-update.md

## 三、市场定位更新（9 引擎，2026-08 在线核实）

| 引擎 | 维护 | 许可 | 与 Caesura 对比 |
|---|---|---|---|
| Ren'Py 8.5.3 | 活跃 | MIT+LGPL | 生态/平台最全；Caesura 的 KAG 标签+GPU 多后端+内置编辑器为差异化 |
| 吉里吉里Z 1.4.0r2 | 低维护（2021 后无提交） | BSD | **Caesura 是 KiriKiri 路线的现代重构**（KAG 标签兼容 + Lua + bgfx 跨平台）——继承日系语法免学习，补其跨平台/维护短板 |
| 吉里吉里2/KAG3 | 停更（2010） | GPL/专有 | 同 KAG 语法；Caesura 活跃维护 + 跨平台 |
| NScripter | 停更（2018） | 专有免费 | 纯 2D 时代；Caesura 支持其 [r]/[s] 类基础命令 |
| ONScripter | 半休眠（2023） | GPL | 老游戏兼容层；Caesura 面向新作 |
| Tyrano V6 | 活跃 | 免费商用 | HTML5 多端；Caesura 原生性能 + C++ 后端更强 |
| Unity+Naninovel | 活跃 | 付费闭源 | 全套但付费；Caesura 免费开源 KAG 路线 |
| Unity+Fungus→Amanita | 官方停更/社区续 | MIT | 节点式；Caesura 文本脚本可维护性更强 |
| Godot+DialogueManager | 活跃 | MIT | 仅对话系统；Caesura 是完整 VN 引擎（标题/存档/画廊/结局/回滚） |

## 四、结论与剩余差距

- **定位**：KiriKiri 语法兼容 + 活跃维护 + 跨平台 + 免费开源的现代 VN 引擎——
  占据"日系老引擎（停更）→ 现代引擎"迁移空白
- **差异化已达成**：内置 Web 编辑器、token 级回滚、KAG3 兼容层、结局/章节/画廊闭环、
  31 项新功能（对比 9 引擎无一项同时具备）
- **剩余差距**：生态（作品/插件）需时间积累；E-mote 类商业中间件授权不可移植；
  移动端发布管线未验证；rollback 的内存成本在超长对话场景需压测
