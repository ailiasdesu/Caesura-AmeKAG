# debug 模块审计（goal round 1）

## 概述
结构化日志（DebugManager 单例，AGENTS.md 允许 DEBUG_* 宏直访）、帧性能分析（PROFILE_SCOPE 采样，已有 4096 帧 cap）、DebugProtocol（RPC 调试协议）、HotReload。规模：~6 文件。健康状况：**良好**。

## P0 关键问题
无。

## P1 重要问题
无。

## P2 建议
1. `DebugManager.h:86` instance() 单例——按 AGENTS.md §7.4 属允许例外（DEBUG_* 宏），但建议确认外部模块未直接 include DebugManager.h 绕过 IDebugManager（耦合扫描未见穿透，仅记录）。工作量 S。
2. `escapeJson` 手写字节转义——功能正确且有意避免反斜杠字面量，可留；如需维护性可换标准 JSON 库（P2，不迫切）。工作量 S。

## 耦合分析
debug 依赖 0 模块（独立）；其他模块依赖 debug（di:2 等）——预算 4 内。

## 审查结论
健康。无修复需求。
