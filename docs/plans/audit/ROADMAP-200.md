# 200 轮迭代路线图 · 阶段 G：产品化（round 101 起）

> 承接 ROADMAP-100.md（round 1-100 完成）。阶段 G 目标（依据 engine-market-analysis-100-rounds.md §7/§8）：
> 补齐技术短板（真机验证、后处理特效、内容创作体验、资产兼容、生态种子），交付示例游戏并验证引擎完整形态运行表现。
> 门禁纪律与工作流同 100 轮冲刺：全量门禁每轮必跑、子代理并行、语义提交、节点 push + 三平台 CI。

| 轮次 | 内容 | 门禁 | 标记 |
|---|---|---|---|
| 101 | **阶段 G 启动：宏缺陷回归锁定 + 大型资源压测 + 示例游戏立项（3 子代理全收敛）**：①**嵌套宏定义收集确认已修复 + 回归锁定**——round 75（23cb8e6a）已同时落地收集侧深度计数（scheduler.lua 1159-1170 bdepth + compiler.lua 378-400 同款 + 含嵌套宏标记 DYNAMIC 排除内联），任务前提过时；本轮补 test_macro_scene D 段 8 断言锁定：outer 体完整收集 4 token（未被首个 endmacro 截断）、inner 惰性注册运行期可用（T:OUTER_BODY,T:INNER_BODY,M:DONE）、无悬空 endmacro 破坏后续流；test_macro_scene 17→25 ②**大型资源压测套件 test_scale_stress.lua（20 断言，登记主套件，Lua 130→131）**——5 维度全设预算且余量充足：A 4096×4096 瓦片图集 texel 记账 3.0ms（<1s）；B 音频句柄池 80000 次 alloc/free 并发上限 128 105ms（<2s）；C 9600 token 大场景 tokenizer 1.59s（<10s）+ scheduler 走满 90ms；D 500 页 backlog 累积堆增长 934.7KB（<4096KB）；E 408,972 字节 3000 行叙述流逐行 translate 1.476s（<10s）；确定性计数断言（瓦片 4096/texel 4096²/句柄复用>1000/帧数==tokens+1/[ch] 派发 4800/无 && 残留）；perf 文档追加 round 101 段；**踩坑记录：string.format 字面量 % 须 %%；340KB 单字符串喂 translate 会 O(n²) 挂起——正确模型是逐行翻译（scheduler 本就按 token 单独翻译）；500 页 backlog 堆增长约 935KB 健壮**；结论：大型资产维度无压力上限撞顶 ③**示例游戏立项《单程回信 The One-Way Reply》**——demo/example_game/DESIGN.md（253 行 8 节）：现代校园温情悬疑短篇，3 角色（主角/澪 Mio/潮 Ushio）、3 结局（归零/同行/守约）、8 流程节点约 17.5 分钟；能力展示清单逐项落点真实命令（文本/立绘/背景/音乐/语音、分支、双存档点、i18n 中英热切换、转场/粒子/后处理占位、循环读信、表达式分支、SMA 小游戏融合、backlog/跳过/自动、[ending] 多结局）；资产复用全部现有 + 6 项设计占位（安全降级）；脚本填充自 story.ks 单文件起步，每轮 ks_check + ks_bake + Web 校验 | Lua 131/131, 孤儿 20/20, C++ 963/963, 耦合/覆盖 PASS | (round 101 提交) |
