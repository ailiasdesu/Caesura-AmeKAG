# rpc 模块审计（goal round 3）

## 概述
双传输：stdio JSON-RPC（RpcServer，26 方法）+ HTTP EditorServer（22 路由）；自研 JSON escape 解码（\uXXXX BMP）；Bearer token 可选门禁；ConstantTime 比较。规模：~8 文件。健康状况：**良好**。

## P0 关键问题
无。

## P1 重要问题
无。

## P2 建议
1. `RpcServer.cpp:334/386` 自研 JSON escape 解码器（\uXXXX 仅 BMP）——19 轮 headless_rpc_smoke 覆盖；若需代理对字符（astral plane）支持可换标准库（P2 不迫切，注意"仅 BMP"注释明确）。
2. `EditorServer.cpp:273-286` Bearer token 门禁为可选（未配置时开放本地端口）——本地开发工具定位合理；文档已注明（editor-api-reference）。

## 耦合分析
rpc 依赖 archive(1)——预算 4 内；main.cpp 的 if-constexpr 链是共享耦合点（19 轮踩坑记录，改动需谨慎）。

## 审查结论
健康。无修复需求。
