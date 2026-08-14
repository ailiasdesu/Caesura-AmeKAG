# rpc 模块审计（goal round 3）

## 概述
双传输：stdio JSON-RPC（RpcServer，26 方法）+ HTTP EditorServer（22 路由）；自研 JSON escape 解码（\uXXXX BMP）；Bearer token 可选门禁；ConstantTime 比较。规模：~8 文件。健康状况：**良好**。

## P0 关键问题
无。

## P1 重要问题
无。

1. ~~`RpcServer.cpp:334/386` 自研 JSON escape 解码器（\uXXXX 仅 BMP）~~ ✅ round 29 已修复：共享 `appendUnicodeEscape` 支持代理对（`\uD83D\uDE00` → 4 字节 UTF-8），`readJsonString` 实况路径与 `jsonUnescape`（现存死代码）均接入；dangling surrogate/坏 hex 回退字面量。冒烟断言 `eval-unicode-escapes`（é中😀 往返）覆盖。

## 耦合分析
rpc 依赖 archive(1)——预算 4 内；main.cpp 的 if-constexpr 链是共享耦合点（19 轮踩坑记录，改动需谨慎）。

## 审查结论
健康。无修复需求。
