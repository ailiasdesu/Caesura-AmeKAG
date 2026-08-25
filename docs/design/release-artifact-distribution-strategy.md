# Caesura (AmeKAG) — 发行工件与资产分发架构规范 (Release Artifact Strategy)

> **版本**：v1.0.0  
> **设计目标**：明确 Git 源码库、CI/CD 自动化构建管线与大型二进制工件分发的权责边界，防止 Git 仓库膨胀与打包内存溢出（OOM）。

---

## 1. 现状审计与痛点分析 (Current State Audit)

当前仓库在 `artifacts/dist/` 目录下生成了 Windows 便携式发行包（约 86~90 MB）：
- `artifacts/dist/windows/CaesuraAmeKAG-1.0.0-rc.1-win64.zip` (90,818,040 字节)
- `artifacts/dist/manifest.json` (768 字节)
- `artifacts/dist/checksums.txt` (54,664 字节，506 项 SHA-256 校验和)

**主要风险**：
1. **Git Pack 内存压力**：在 16GB 内存开发机上执行 `git push` 时，大于 50MB 的单文件可能导致 Git 打包线程触发 OOM 错误（需显式配置 `pack.windowMemory 256m`）。
2. **仓库克隆体积**：频繁在 Git 提交中替换二进制 ZIP 会导致 `.git/objects` 快速膨胀至数 GB。

---

## 2. 三层分发解耦架构 (Three-Tier Artifact Architecture)

```
┌──────────────────────────────────────────────────────────────┐
│ 1. Git Source Repository (轻量源码层 — 严格限制 < 10MB)       │
├──────────────────────────────────────────────────────────────┤
│  • 引擎与创作者工具源码 (src/, scripts/, web/)                │
│  • 打包与编译流水线脚本 (CMakeLists.txt, scripts/package_*.sh) │
│  • 架构清单与校验元数据 (manifest.json, checksums.txt)       │
│  • 官方 Showcase 轻量示例资产 (demo/example_game/)           │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│ 2. CI/CD Packaging Pipeline (自动化构建层 — 纯无状态)         │
├──────────────────────────────────────────────────────────────┤
│  • Windows: CPack ZIP 打包 (Debug/Release)                   │
│  • Web: Vite PWA 静态站打包 (artifacts/dist/web/)             │
│  • Android: Gradle APK / AAB 签名构建                        │
│  • Checksum & Sign: 自动计算 SHA-256 散列清单                │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│ 3. External Distribution (外部发布层 — CDN & GitHub Release) │
├──────────────────────────────────────────────────────────────┤
│  • GitHub Releases: 承载 .zip / .apk / .aab / .tar.gz        │
│  • Steamworks Backend: 承载 Steam 渠道分发与 Depot 打包       │
│  • S3 / Cloudflare R2: Web 在线试玩 PWA 静态资源分发         │
│  • Git LFS (可选): 针对未切分的长篇 CG 视频与高保真音频资产    │
└──────────────────────────────────────────────────────────────┘
```

---

## 3. 规范与准则 (Rules & Guidelines)

1. **Git 仓库收录范围**：
   - 允许收录：`manifest.json`、`checksums.txt`（作为发布证据链）、构建脚本、轻量测试资产。
   - 严禁提交：超过 10MB 的预编译二进制发行包（`.zip`、`.apk`、`.exe` 等）。
2. **本地打包与输出规则**：
   - 本地构建输出至 `artifacts/dist/`，该目录在日常开发中受 `.gitignore` 保护（仅在创建正式 Release 证据时提交清单）。
3. **CI/CD 发布建议**：
   - 推荐使用 GitHub Actions 监听 `v*` 标签，自动编译并上传至 GitHub Releases：
     ```bash
     gh release create v1.0.0 \
       artifacts/dist/windows/CaesuraAmeKAG-1.0.0-win64.zip \
       artifacts/dist/manifest.json \
       artifacts/dist/checksums.txt \
       --title "Caesura (AmeKAG) v1.0.0 Release"
     ```
