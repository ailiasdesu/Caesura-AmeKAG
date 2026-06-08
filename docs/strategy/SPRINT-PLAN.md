# Sprint Plan — Alpha 0.3 → 0.4

> 2026-06-08 | 19 verified items | Target: P1 this sprint

## Sprint Goal

清理代码卫生 + 补齐工程基础设施。7 项 P1，预估 3-4h。

---

## P1-1: 去重文件 + 合并 AI 上下文 (30min)

**问题:** scene-template.lua 2 份副本；AI-CONTEXT.md 3 份副本。

**修复:**
- 删除 `src/Core/docs/` 整个目录（旧副本）
- 删除 `docs/ai/CONTEXT.md`
- 保留 `docs/templates/scene-template.lua` + `docs/strategy/AI-CONTEXT.md` 为权威版本

**文件:**
- `src/Core/docs/` → 删除
- `docs/ai/CONTEXT.md` → 删除

---

## P1-2: SaveManager JSON → nlohmann/json (45min)

**问题:** 手写 JSON string building，仅支持扁平键值对。

**修复:**
- CMakeLists.txt 添加 nlohmann/json（header-only，3.11.3）
- 替换 SaveManager 中 JSON 序列化/反序列化
- 保持向后兼容（存档格式不变）

**文件:**
- `CMakeLists.txt`
- `src/System/SaveManager.cpp`

---

## P1-3: 添加 .clang-format (15min)

**问题:** 无统一代码风格。

**修复:**
- 添加 `.clang-format`（基于 WebKit 风格 + C++20）
- 不做批量格式化（仅新代码生效）

**文件:**
- `.clang-format`（新增）

---

## P1-4: CryptoEngine 跨平台加密实测 (30min)

**问题:** OpenSSL EVP 路径存在但 macOS/Linux CI 未验证。

**修复:**
- 确认 test_carc.cpp 覆盖 AES-256-GCM + Ed25519
- CI 已是阻塞化测试 → 加密测试自动覆盖

**文件:**
- 无需修改（CI 已验证）
- 仅确认 CI 结果

---

## P1-5: CI 代码覆盖率 (30min)

**问题:** 无覆盖率数据。

**修复:**
- CMake 添加 Coverage 配置（仅 Linux，gcov/lcov）
- CI 新增 coverage job
- 上传 lcov HTML 为 artifact

**文件:**
- `CMakeLists.txt`
- `.github/workflows/ci.yml`

---

## P1-6: ENGINE_ANALYSIS.md 更新 (30min)

**问题:** 含 11 个不实 TD。

**修复:**
- 删除不存在的 TD（TD-02/05/06/10/11/13/17）
- 标记已修复项
- 版本升至 Alpha 0.4.0

**文件:**
- `docs/strategy/ENGINE_ANALYSIS.md`

---

## P1-7: README 跨平台说明 (15min)

**问题:** README 仅提及 Windows 构建。

**修复:**
- 添加 macOS/Linux 构建命令
- 添加跨平台 CI badge 说明

**文件:**
- `README.md`

---

## Execution Order

| Step | Item | Time | Depends |
|---|---|---|---|
| 1 | P1-1 去重 + 合并 | 30min | — |
| 2 | P1-2 nlohmann/json | 45min | — |
| 3 | P1-3 .clang-format | 15min | — |
| 4 | P1-4 加密实测 | 30min | — |
| 5 | P1-5 CI 覆盖率 | 30min | 4 |
| 6 | P1-6 ENGINE_ANALYSIS | 30min | 1-5 |
| 7 | P1-7 README | 15min | — |

**Total: ~3.5h**
