# entry 模块审计（goal round 3）

## 概述
引擎组合根（Engine/EngineConfig）：分阶段初始化（platform→scripting→asset→backends）、失败回滚、ErrorUI（崩溃兜底 UI）、GPU 恢复、启动脚本/校验。规模：~10 文件。健康状况：**优秀**。

## P0 关键问题
无。

## P1 重要问题
无。

## P2 建议
1. `EngineConfig.h` 仅前向声明 + move-only + `std::exchange` 所有权转移——模式优秀；`EngineConfig(EngineConfig&&) = delete` 与自定义 move ctor 并存（显示删除拷贝、自定义移动），确认意图（限制所有权的语义明确）——记录级。
2. `Engine.cpp` 分阶段 init 失败即 shutdown 回滚 + double-init 守卫——健康。

## 耦合分析
entry 是组合根（预算 ≤14），实际跨模块依赖 14（19 轮耦合 PASS）。

## 审查结论
健康。无修复需求。
