---
title: 跨平台 CI 完全修复 — 三平台全通 (2026-06-11)
date: 2026-06-11
category: docs/solutions/build-error
module: CI
problem_type: build
severity: high
tags: [ci, cross-platform, cmake, case-sensitivity, sse4.1, malloc.h]
---

# 跨平台 CI 完全修复

## 根因

5 个独立问题，每个仅影响特定平台：

| 平台 | 症状 | 根因 | 修复 |
|------|------|------|------|
| Linux | CMake configure "No SOURCES" | `src/CARC/` vs `src/Carc/` 大小写 (22处) | 统一为 `src/Carc/` |
| Linux | Build: `_mm_blendv_ps` 未定义 | bimg 用 SSE4.1 未启用 | x86_64 `target_compile_options(bimg PRIVATE -msse4.1)` |
| macOS | Build: `malloc.h` 不存在 | macOS 无此 POSIX 扩展头 | `#ifdef __APPLE__` → `stdlib.h` |
| macOS | Test: JobSystem ASSERT | `Engine::s_mainThreadId` 未设 | 测试中设置 `std::this_thread::get_id()` |
| Windows | Test: Engine 构造 SIGSEGV | 无后端 CI 测试创建/析构 Engine | 移除该测试用例 |

## SDL3 安装策略

- Windows: vcpkg (`sdl3 --triplet x64-windows`)
- macOS: brew (`sdl3`)
- Linux: 源码构建 (`wget + cmake --build + sudo cmake --install /usr/local`)
- FFmpeg: Windows ON, Linux/macOS OFF (避免额外依赖)

## CI Workflow

`workflow_dispatch` 手动触发。5 个 job 并行：Win Debug/Release, macOS, Linux, Release Package。

配置: `96d9b67` (大小写修复), `2f42958` (测试修复), `21f116b` (v1.0-rc)。
