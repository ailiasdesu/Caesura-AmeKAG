# Caesura (AmeKAG) — 交接文档（2026-08-14 第 18 轮迭代）

> 面向后续 agent 的完整上下文。本轮为**大特性轮**：
> SMA S5 GPU 蒙皮（bgfx compute）+ SMA 播放控制与高级动画（IK/混合/部件变体）
> + IDE 可视化预览深化（/api/state 状态回显）+ 推送与 CI 三平台。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档。**

## 1. 本轮成果（分语义提交）

| 提交 | 内容 |
|---|---|
| `feat(render)` | **SMA S5 GPU 蒙皮**：IMeshRenderer 加 SkinMode(Auto/Cpu/Gpu)；SmaMeshRenderer 计算管线——静态 compute 输入 VB（D3D11 禁 DYNAMIC+SRV）、动态输出 VB（COMPUTE_WRITE）、骨骼缓冲 64+2 槽（64/65 携带绘制变换与视口）；骨骼打包 packBonePose（与 CPU 严格等价）；dispatch 同 view（compute 排序先于 draw）；计算 shader 直接输出 NDC（HLSL Buffer/RWBuffer 按绑定 stage 声明寄存器，**输出 u2**；GLSL std430 binding 0/1/2）；绘制复用引擎直通程序（零新 VS/uniform）；无 compute 能力回落 CPU；SmaBinding set_skin_mode/get_skin_mode |
| `test(render)` | packBonePoses↔applyBonePose 200 随机组等价 + compute shader 数学复刻 vs skinMesh 30 组随机网格 + 状态机 + **D3D11 GPU 子测试**（同一网格同一姿势 GPU/CPU 两帧读回逐像素比对，容差 ±1）——14/14 通过 |
| `feat(kag)` | **SMA 播放控制**：loop（设计文档 §4 承诺）/duration/rate/pause/resume/seek/play_anim/on_done_anim + sma_play 契约扩展 + sma_anim 契约 |
| `feat(kag)` | **SMA 高级动画**：2 骨 IK（ik2bones cosine-law 纯函数 + set_ik/clear_ik + [sma_ik]）、crossfade 混合（play_anim blend_time 双采样 LERP）、E-mote 部件/表情变体（parts 资产 + set_variant + [sma_variant]，单 mesh 路径兼容） |
| `feat(rpc+editor)` | **/api/state 引擎状态端点**：RpcStateResult 扩展（scene/token_index/nvl_mode/language/backlog_count/layer_count），main.cpp 经 kag_runner.get_ctx() Lua 快照填充；stdio getState + HTTP /api/state + /api/debug/getState 同步；VisualView Engine State 面板（随帧轮询 + 手动刷新）；headless_http_smoke 断言 |
| `docs` | 矩阵 D10、SMA 设计文档 §9/§9b、tour §15、editor-api-reference（/api/state）、command-contracts 重生成（81→85 命令）、api-stats（C++ 625/6119、Lua 绑定 168）、交接 018 |

## 2. 关键实现细节与坑（务必记住）

- **D3D11 compute 缓冲是 typed float4 视图**（bgfx 不设 STRUCTURED 标志）：
  HLSL 必须用 Buffer<float4>/RWBuffer<float4>，且**输出寄存器必须与绑定
  stage 一致**（绑定 stage 2 → register(u2)，否则输出写入未绑定槽静默丢失）。
- **D3D11 禁止 DYNAMIC 用法 + SRV 绑定**：compute 输入缓冲必须静态创建
  （数据在 createMesh 时已知）；逐帧更新的骨骼缓冲走 DYNAMIC+SRV 在实测中
  失败——本轮用静态输入 + 骨骼缓冲动态（当前通过；若未来 D3D11 真机出现
  骨骼异常，将骨骼缓冲改为 DEFAULT 用法 + 非 discard 更新）。
- **makeRef 生命周期**：bgfx::update/create 的引用内存必须存活到 frame()——
  一律 bgfx::copy 移交（本轮修复了 createMesh 索引缓冲与骨骼更新的悬垂）。
- **BgfxShaderManager::initEmbeddedShaders 的 static 守卫是潜在大坑**：函数级
  static 导致第二个 manager 实例（SmaMeshRenderer 自建）被静默跳过、句柄全
  无效——已改为成员级守卫（SMA CPU 绘制在真 GPU 上此前从未真正工作过）。
- **S5 数学等价**：packBonePose = (cos·scale, sin·scale, ox, oy)，shader 与
  CPU 公式同构；NDC 变换移入计算着色器（变换值经骨骼槽 64/65 传递），
  绘制复用已验证的直通程序——**完全绕开 VS uniform/cbuffer 路径**（该路径
  在本引擎 D3D11 上不可用，见下）。
- **VS uniform/cbuffer 在 D3D11 上不可用**（多次实验：手写 CSH/VSH + uniform
  表 + cbSize 均无法让 uniform 值到达 shader）——本轮架构以"数据全部走
  缓冲"规避；未来若需 VS uniform 需先解决此问题。

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| P1-6 Live2D GL/Steam 实机 | **待设备** | GL 需 Linux/macOS 硬件；Steam 需开发者账号 |
| P0-1 Metal 后端真机验证 | **待设备** | macOS 硬件；Metal 计算蒙皮当前回落 CPU |
| P0-3 移动真机验证 | **待设备** | Android/iOS 设备；管线与文档已就绪 |
| 真实 Ollama 端到端 | 已闭环 | 17 轮完成（本机 gemma3:4b） |

> 硬件三项维持"待设备"状态（本轮未实现，仅文档标注）；代码侧无可闭环项遗留。

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests 625/625（6119 断言，含 D3D11 GPU 蒙皮子测试）
→ Lua 118/118（test_sma 70 断言）→ ctest 11/11 → 耦合 PASS → api-stats 重生成。

## 5. 注意事项

- **GPU 子测试在 CI**：Windows（WARP）可跑；macOS/Linux 受 _WIN32 guard 保护不跑。
- **编辑器前端**：改动在 editor/src（rpc.ts state() + VisualView 面板）；
  需要 `pnpm build`/vite 重新构建后随 electron 使用（本机未重建 dist——
  仓库规则：editor/dist 不入库则 CI 无碍；确认 git 状态）。
- **sma.lua 新 API**：play_anim/set_ik/set_variant/pause/resume/seek/set_rate/
  set_skin_mode + sma_anim/sma_ik/sma_variant 命令；test_sma 70 断言覆盖。
- 历史交接：`2026-08-13-017-delivery-handoff.md` 为上一权威状态。