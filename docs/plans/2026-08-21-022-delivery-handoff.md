# Caesura (AmeKAG) — 交接文档（2026-08-21 第 22 轮迭代 / round 116-117 起点）

> 面向后续 agent 的完整上下文。承接 021（round 100 起点）——022 记录 **rounds 116-117 完成的
> v1.0.1 发布**：动态图层引擎 v2 + 25 项代码审查处置（含 RD-1/ST-2 全部 high/medium 清零）+ README 双语
> + 契约同步 + v1.0.1 正式发布。**先读 AGENTS.md（模块边界铁律）+ 本文件 + ROADMAP-200.md（round 101 起）。**

---

## 1. 项目状态与当前基线（round 117 完成态 / 2026-08-21）

| 维度 | 基线 | 说明 |
|---|---|---|
| 版本 | **v1.0.1**（CMake + tag + GitHub Release 正式版） | v1.0.0 → v1.0.1：rounds 116-117 共 12 提交 |
| C++ 用例 | **987/987**（315556 断言） | round 116 动态图层 +7、ST-1 +1、RD-4 +1、ST-2 +3 |
| Lua 用例 | **132/132 + 24 孤儿** | 主套件 132 全过 + 孤儿 24（SMA 源码守护含在内） |
| Web (web/) | **298/298（20 文件）** | vitest 全绿 |
| Editor (editor/) | **530/530** | vitest 全绿（含 layerSnapshot 7 条） |
| ctest | 10/10 + AI smoke | headless_http/rpc/LSP 冒烟惯性通过 |
| 耦合 / 覆盖 | **PASS** | count_coupling.py --ci |
| 契约命令 | **123**（运行时覆盖 100%） | command-contracts.md 权威 |
| 接口普查 | **31 接口 / 390 纯虚方法** | api-stats.md（ILayerManager 16→21 方法） |
| 能力矩阵 | **82 项** | engine-capability-matrix.md（R11/S13/S14 已含） |
| 教程库 | 16 个 | tutorial_01–16（tween 为 16） |
| 示例游戏 | 《单程回信》三结局 | verify_sample_game.sh 5/5 + Web 站 31.08MB |
| CI | **三平台 success**（runs 32385392288 / 32390423309 / 32446410272） | Windows D+R / macOS / Linux / Package |

> 上一权威交接：`docs/plans/2026-08-16-021-delivery-handoff.md`（round 100 起点）。
> round 101-115 阶段 G 明细见 ROADMAP-200.md；round 116-117 见本文 §2。

---

## 2. 近期完成（rounds 116-117 摘要）

| 轮次 | 里程碑/主题 | 关键内容 |
|---|---|---|
| **116** | **动态图层 v2 + 25 项代码审查处置 + README 双语** | **① 动态图层引擎 v2（主代理接口契约）**——ILayerManager 升级：configureLayers/getLayerCount/getLayerName/findLayer/reorderLayer，数量/名称/排序可配置，LayerType(BG/FG/MSG=0/1/2) 保默认布局向后兼容；LayerManager 固定数组→vector；markDirty 同步 dirty；透明度脏区按运行时渲染顺序传播；EntryLifecycleBackends mock + 7 新用例。**② 审查处置 25 条中 11 修复**：A-1 CRL stoll 崩溃→try/catch；ST-1 坏存档崩溃→安全字段读取；R-1 stdio 无限缓冲→16MiB 上限；S1-1 SMA 字段读取失效→lua_getfield + indices 校验；RD-2 PostFx 句柄位→disable 稳定；RD-3 资产无上限→512MiB；S1-3 颜色 clamp；RD-5 透明度 clamp；RD-4 sphere 索引→segments 上限。**③ README 中英双语 636→745 行无 emoji，93 链接零断裂；能力矩阵 79→82、接口 385→390 同步**。契约重生成 api-stats 幂等 |
| **117** | **RD-1/ST-2 收尾 + v1.0.1 发布** | **① RD-1**：VideoPlayer 并发 UAF——m_videos 值改 shared_ptr、worker 捕获引用、close 逻辑销毁即时/物理 erase 延迟帧末 flushPendingClose、shutdown 先 flush。**② ST-2**：HttpCloudSaveProvider——https 支持（无 OpenSSL fail-closed）、Bearer token 鉴权、10MiB 拉取上限、makeClient 统一；+3 专项测试。**③ v1.0.1 发布**：CMake 1.0.1、CHANGELOG v1.0.1 段（10 commits）、triage 文档 RD-1/ST-2 移已修复、ROADMAP round 117；Release 门禁全绿（987/987 + Lua 132+24 + 耦合 + CPack 87.99MB 冒烟干净 + Web 31.08MB）；git tag v1.0.1 + gh release 正式发布（非 draft） |

---

## 3. 当前待办 / low 级加固（round 117 末）

round 116 triage 文档（`docs/plans/audit/r116-code-review-triage.md`）记录剩余 low 级项，
round 117 末已全部落实（见 ROADMAP-200 round 118 记录）：

| 编号 | 状态 | 内容 |
|---|---|---|
| A-4 | **已修复** | CryptoEngine sign/verify 长度校验（Ed25519 私钥=64B 断言 fail-closed） |
| A-5 | **已修复** | CARC 索引 nonce 派生 [version 4B][sha256(公钥)前 8B]，消除 GCM nonce 复用 |
| R-2 | **已修复** | RpcServer parseId int64 累积 + INT_MAX 钳制（消除 UB） |
| R-3 | **已确认** | EditorServer 已有 loopback + bearer token 门禁（补注释说明语义） |
| ST-3 | **已修复** | LocalFileSaveProvider::writeFile 原子写（tmp+rename）+ 10MiB 上限 |
| RE-2 | **已修复** | ImageDecoder fromBimg 校验 m_size 覆盖跨度（fail-closed） |

> 端口门禁验证：Crypto 24/24、CARC 45/45、Storage 49/49、Decoder 2/2、Editor 25/25、RpcServer 26/26，全量 987/987 + Lua 132+24 全绿。

---

## 4. 门禁（每轮强制）

全量重建零错误 → C++ 987/987 → Lua 132/132 + 孤儿 24/24 → web 298/298 → editor 530/530
→ ctest（+AI smoke 跳过）→ 耦合 PASS → 覆盖检查 PASS → 语义提交。
推送策略：多轮本地累积语义提交，到目标节点统一 push + 一次三平台 CI。

---

## 5. 注意事项

- **bundle sweep 是 bundle 路径真实守卫**：20/20 场景；改 scheduler/compiler 涉及 [save]/[load] 必跑 web sweep + flow。
- **跨场景切换 = 高风险改动面**：round 97/99 已修复跨场景 load resume、循环栈泄漏、预算；触碰 scheduler 跨场景分支先跑 test_flow_edge_scene + test_scene_switch_budget。
- **动态图层接口契约由主代理独占**：任何 ILayerManager 扩展须经组合根/契约流程（AGENTS.md §3/§10）。
- **CARC 索引 nonce 派生（A-5）读写两侧严格一致**：CARCWriter.cpp L155-159 与 CARCReader.cpp L305-310 必须同步改动，否则归档无法解密。
- **RD-1 shared_ptr 生命周期**：close() 物理 erase 延迟到 updateAll() 帧末 flushPendingClose；shutdown 先 flush；勿在 close 内直接 erase（会 UAF）。
- **编辑器/Web 改动**：editor/src（tsc + vitest）、web/（vitest + flow + sweep）；产物不入库。
- **文档权威性**：ROADMAP-200.md = round 101+ 轮次记录权威；本 handoff = 交接现状；engine-capability-matrix.md = 能力矩阵（82）。
- **版本流程**：release-process.md §6 命令序列已含 v1.0.1 实测（ZIP 名随 CPACK_PACKAGE_FILE_NAME）；gh release 直接正式发布（非 draft）。
- 历史交接：021（round 100 起点）→ 022（本文件，round 117 完成态）。
