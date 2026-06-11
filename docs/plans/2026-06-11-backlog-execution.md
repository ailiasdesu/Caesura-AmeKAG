---
title: Backlog 执行计划 — Audio 测试闭环 + Steam SDK 集成
type: feature
status: planned
date: 2026-06-11
origin: docs/BACKLOG.md
prerequisites: v1.0-rc (OK), CI 三平台全通 (OK)
---

# Backlog 执行计划

## Summary

BACKLOG.md 包含两类工作：Audio format 测试闭环（4 项，~50 行 C++），Steam SDK 集成（12 项，~800 行新代码）。

## Implementation Units

### Phase A: Audio Format Test Closure (A-01 ~ A-04)

| IU | 内容 | 文件 |
|----|------|------|
| A1 | 真实格式加载测试: 4 个 silent 测试文件 (.wav/.ogg/.mp3/.flac)，验证 play* 返回有效 handle | 	ests/cpp/test_audio.cpp, 	ests/audio/ |
| A2 | 不支持格式测试: 随机字节文件 + .mid + .aiff 应返回 0 不崩溃 | 	ests/cpp/test_audio.cpp |
| A3 | 文档: README 列出支持格式, KAG-API.md audio 章节加格式说明 | README.md, docs/api/KAG-API.md |
| A4 | 测试素材: 4 个 1 秒 silent 音频文件放入 	ests/audio/ | 	ests/audio/ (新目录) |

**Acceptance**: 测试通过, 格式表在 README 中可见, 测试文件 check in。

### Phase B: Steam SDK Integration (S-01 ~ S-12)

| IU | 内容 | 文件 |
|----|------|------|
| S1 | CMake option + SDK discovery (mirror Live2D pattern) | CMakeLists.txt |
| S2 | ISteamBackend pure virtual interface (3 groups: achievements/stats/cloud) | src/Steam/ISteamBackend.h |
| S3 | SteamBackend real implementation (SteamAPI_Init, achievements, stats) | src/Steam/SteamBackend.h/.cpp |
| S4 | NullSteamBackend no-op (all methods return false/0/empty) | src/Steam/NullSteamBackend.h/.cpp |
| S5 | SteamBinding Lua bindings (steam.unlock_achievement, steam.set_stat, etc.) | src/Scripting/SteamBinding.cpp/.h |
| S6 | CloudSaveProvider (ISaveProvider via ISteamRemoteStorage) | src/System/CloudSaveProvider.h/.cpp |
| S7 | Engine main loop integration (SteamAPI_RunCallbacks per frame, overlay pause) | src/Core/Engine.cpp |
| S8 | CRL bypass when Steam active | src/CARC/CRLManager.cpp |
| S9 | Packaging: SteamPipe depot structure | CMakeLists.txt or 	ools/steam_depot.py |
| S10 | Unit tests with NullBackend | 	ests/cpp/test_steam.cpp |
| S11 | Documentation: steam-integration.md quick-start | docs/steam-integration.md |

**Acceptance**: -DCAESURA_ENABLE_STEAM=ON configures OK, NullBackend works without SDK, Lua steam.* calls functional, tests pass, docs exist.

## Key Decisions

- Steam SDK NOT committed (same pattern as Live2D Cubism)
- #ifdef CAESURA_HAS_STEAM guards all Steam-specific code
- Cloud saves split into chunks if >256KB (Steam Remote Storage limit)
- NullBackend always linked; real backend linked only when CAESURA_ENABLE_STEAM=ON

## Test Plan

- Phase A: CaesuraTests 新增 6 个 audio 测试用例
- Phase B: 	est_steam.cpp 10+ 用例 (achievement/stat/cloud mock)

## Assumptions

- Phase A 先做（简单快速），Phase B 后做（代码量大）
- Steam SDK 版本: Steamworks 1.60+
- CDN/distribution 不在本计划范围
