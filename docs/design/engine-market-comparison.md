# Caesura (AmeKAG) 引擎架构、能力与市场对比（2026-08-03）

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
