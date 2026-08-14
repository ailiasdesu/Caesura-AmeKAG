# archive 模块审计（goal round 1）

## 概述
CARC 加密归档（AES-256-GCM + Ed25519 签名 + zstd 压缩）、CryptoEngine、CRL 证书链、DeltaCARC 增量补丁、CarcAssetProvider 资产接入。规模：~10 实现文件 + 3 接口头。健康状况：**优秀**（安全关键模块，防御性编程到位）。

## P0 关键问题
无。

## P1 重要问题
无。

## P2 建议
1. `CARCReader.cpp` 多处分块 `m_stream.read(reinterpret_cast<char*>(...), size)` 后依赖后续操作隐式检查流状态；建议统一封装 `checkedRead(stream, dst, size) -> bool`（返回 false 即失败路径），与现有 checkedAdd 风格一致。工作量 S。
2. `DeltaCARC.cpp` 的 `read` 序列（key/nonce/tag/encrypted）无独立流状态检查，失败时部分字段可能残留旧值；建议同样走 checkedRead。工作量 S。
3. `CRLManager.cpp` payload reinterpret_cast 前未见显式长度校验注释；确认调用点已校验（验证通过），可加断言防回归。工作量 S。

## 耦合分析
archive 依赖：di(1) + storage(1)（count_coupling 输出 archive 2 模块）——预算 4 内。通过 IArchiveReader/IArchiveWriter/ICryptoEngine 暴露，无穿透。

## 审查结论
健康，无阻塞项。P2 三项（checkedRead 封装）可并入后续修复轮小批量处理。
