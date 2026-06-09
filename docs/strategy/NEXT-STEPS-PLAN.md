# Caesura (AmeKAG) 下一步计划

> 更新: 2026-06-09 | 引擎完成度: ~82% (Alpha+)

## 执行顺序

### A1. TD-17: submit_batch RTT→tex 键名修复 [10min]
- layers.lua `node.rt`/`node.tex` 分离，batch 协议对齐 C++ `resolveTexture()`
- 修复 `draw=0`

### A2. TD-12: CryptoEngine 跨平台加密 [30min]
- 方案 C: Win(BCrypt) | Linux(OpenSSL EVP) | macOS(CommonCrypto)
- `#ifdef` 守卫，零额外依赖

### D1. TD-15: pl_mpeg → FFmpeg 视频解码
- 多线程解码，不阻塞主线
- 更多格式支持 + 硬件解码潜力
- 替换现有 pl_mpeg（单头文件、功能有限）

### D2. TD-14: CARC 差分更新 (Delta CARC)
- bsdiff / zstd 差分生成 + 应用
- 减少热更新下载量

### D3. TD-21: MiniGame 3D 后端实现
- IMiniGameBackend 接口已有，实现 Null→具体
- 3D 小游戏提升交互体验（用户核心需求）

### C1. TD-22: 跨平台 CI 收尾
- 前置: A2 完成
- 参考: docs/solutions/build-error/cross-platform-ci-fixes.md

## 移交共同开发者

| 任务 | 原因 |
|------|------|
| B1 Live2D OpenGLShared | 需 Linux 实测 |
| B2 Live2D Metal | 需 macOS 实测 |
| TD-20 移动端适配 | 需真机环境 |