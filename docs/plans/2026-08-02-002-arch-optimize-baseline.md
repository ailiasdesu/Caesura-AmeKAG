# Caesura (AmeKAG) — closeout 008：架构优化基线（arch-optimize 首次审计）（2026-08-02）

> 范围：应用 arch-optimize 技能五阶段工作流，建立架构质量基线。
> 工具：`.reasonix/skills/arch-optimize/scripts/`（arch_scan / dep_graph / risk_diagnose / quality_metrics / regression_guard）。

## 阶段一 架构感知
- arch_scan：C++ 为主（17 C++ 目录）、src 含 10 子模块、入口点与构建系统识别（2.3MB JSON，含 build 噪音）
- dep_graph：**circular_deps = []（零循环依赖）**——符合 AGENTS.md 模块边界铁律；main 扇出 9（组合根）、entry 2、render/audio/archive 各 1

## 阶段二 风险诊断（src/，201 文件，178 发现）
- 分布：R1 认知过载 116、R2 变更传播 166、R3 知识重复 66、R4 偶发复杂 8（R5/R6 无——无循环依赖/依赖方向正确）
- 启发式 health_score 0/危险（92C+84W）——**正则+花括号粗粒度统计，假阳性率高**，不作为绝对评分
- 热点模块：script/bindings 72、render 48、archive 40、minigame 34、debug 30

## 阶段三 质量度量（基线）
| 模块 | MI | CC | health |
|------|-----|-----|--------|
| script/bindings | 43.46 | 1.77 | 60 |
| render | 33.16 | 1.48 | 95 |
| archive | 37.11 | 1.52 | 100 |
| debug | 24.39 | 3.69 | 65 |

平均 MI 24-43（均 >20 高可维护）、平均 CC 1.5-3.7（远低于 15）——**架构健康度良好**。

## 阶段四 改进计划（5 项上限取 4，0 项需立即实施）
| # | 项 | 处置 |
|---|-----|------|
| 1 | SaveBinding luaTableToJson/jsonToLuaTable（CC 21/23）| **假阳性 Dismiss**：实查为线性类型分发 switch（技能假阳性防护：线性+清晰命名≠认知过载） |
| 2 | debug 模块 health 65 | Monitored：启发式误报为主，复查后按需拆分 |
| 3 | render buildBgfxShader（CC 8）| Monitored：Warning 级观察 |
| 4 | 质量基线记录 | 本文档 + `.reasonix/arch_baseline.json` |

## 阶段五 回归防护（基线）
- **564/564 测试**（2770 assertions）、ctest 10/10、HTTP smoke 21/21、耦合度 PASS
- CI 三平台全绿（run 30726580418 success：Linux/Windows Debug/Windows Release/macOS/Package）
- 基线 JSON：`.reasonix/arch_baseline.json`（后续迭代用 regression_guard compare 对比零退化率）
- 注：regression_guard record 无法自动解析 doctest 输出（非标准框架），基线手动记录

## 后续迭代指引
- 每个改进迭代：变更前 compare 基线 → 实施（≤5 项）→ 全测试 → compare 确认零退化率 100% → CI 绿 → 提交
- 新增代码门禁：MI ≥ 15、无循环依赖、测试覆盖
