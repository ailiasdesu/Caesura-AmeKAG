# di 模块审计（goal round 1）

## 概述
BackendRegistry（模板化 getService/setService 单一访问点）、TextureBudget（分级预算）、SandboxQuota（沙箱配额）、ThreadAssert（主线程断言）。规模：~8 文件。健康状况：**优秀**（架构核心，执行到位）。

## P0 关键问题
无。

## P1 重要问题
无。

## P2 建议
1. `BackendRegistry.cpp` 的 instance() 为函数级 static（C++11 线程安全初始化）——正确；无改动。
2. ThreadAssert 依赖 thread_local g_mainThreadId——确认主线程 id 在 Engine::init 设置且 RPC 线程路径有断言覆盖（既有测试覆盖，仅记录）。

## 耦合分析
di 依赖全部接口头（设计使然，预算 ≤14）；是唯一被所有模块依赖的注册表。符合 AGENTS.md §3。

## 审查结论
健康。无修复需求。
