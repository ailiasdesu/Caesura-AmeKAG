---
title: "CARC Security Completion & Code Quality Cleanup"
date: 2026-06-07
category: security-issues
module: Carc/CARCReader, Resource/IAssetProvider, Audio/SoLoudAudioEngine
problem_type: security_issue
component: development_workflow
symptoms:
  - "CARC archive not verified at startup despite config flag"
  - "CARCReader internal members inaccessible to CarcAssetProvider"
  - "SoLoud bus handles not validated with isValidVoiceHandle"
  - "GpuMonitor reports zero average for first 60 frames"
  - "RTTManager pool index stale after handle deletion"
root_cause: missing_workflow_step
resolution_type: code_fix
severity: medium
related_components: ["Audio/SoLoudAudioEngine", "Render/GpuMonitor", "Render/RTTManager", "Render/LayerManager", "Render/ParticleSystem", "System/SaveManager", "Scripting/VFXBinding"]
tags: [carc, signature-verification, async-thread, lru-cache, code-quality, dead-code]
---

# CARC Security Completion & Code Quality Cleanup

## Problem

Alpha 补完涉及 3 类未完成工作：CARC 安全验证基础架构存在但未接入引擎、多个子系统存在已知代码质量问题、规范中的新需求未实现。

## Solution

### CARC 安全补完

**1. CARCReader.h — friend 声明**
```cpp
class CARCReader {
    friend class CarcAssetProvider;  // 允许访问内部验证方法
public:
    // ...
};
```

**2. main.cpp — 异步启动验证线程**
```cpp
// 读取 config.carc_verify_on_startup + config.carc_path
// 默认查找 data.carc
std::thread([&engine]() {
    caesura::carc::CARCReader reader;
    if (reader.open(carcPath)) {
        bool sigOk = reader.verifySignature();
        printf("[main] CARC verify [%s]: %s\n", carcPath.c_str(),
               sigOk ? "SIGNATURE VALID" : "SIGNATURE INVALID");
    }
}).detach();
```
验证结果异步输出，不阻塞主线程启动。

### 代码质量清理

| 修复 | 文件 | 方法 |
|------|------|------|
| `== 0` → `isValidVoiceHandle()` | SoLoudAudioEngine.cpp | 替换 3 处总线句柄检测 |
| 波形 LRU 逐出 | SoLoudAudioEngine.cpp | `std::list` + `std::unordered_map`，满时 pop_back 最久未用 |
| GPU Monitor 累积均值 | GpuMonitor.cpp | 前 kWindowSize 帧用累积平均避免归零 |
| RTT pool 索引修复 | RTTManager.cpp | erase 后遍历 `m_handleToPoolIndex` 递减受影响索引 |
| 逆向包含路径 | main.cpp:7 | `../Render/TextureManager.h` → `Render/TextureManager.h` |
| `idx < 0` 死代码 | LayerManager.cpp | uint8_t 永不为负，移除检查 |
| submitFullscreenQuad 注释 | BgfxRenderDevice.cpp | 添加 `// reserved for future 3D RTT` |
| SaveManager 迁移模板 | SaveManager.cpp | `registerBuiltinMigrations()` + v1→v2 注释模板 |
| IAssetProvider::verify() | IAssetProvider.h | 新增纯虚接口，CarcAssetProvider/DirAssetProvider 实现 |
| ParticleSystem 动态分辨率 | ParticleSystem.cpp | `update(dt, w, h)` 替代硬编码 1280×720 |
| VFXBinding 适配 | VFXBinding.cpp | 传入屏幕尺寸到 ParticleSystem |

### 规范新增决策（代码实现完毕）

| 决策 | 内容 |
|------|------|
| [10.2.66] | BCryptGenRandom → 替代 std::mt19937 |
| [10.2.67] | flushAllRTT() → bgfx::destroy(fb) loop |
| [10.2.68] | FreeTypeContext 共享实例 |
| [10.2.69] | 音频波形 LRU 逐出 |
| [10.2.70] | ShaderCache 所有权分离 |
| [10.2.71] | 引擎关机顺序 |

## Why This Works

- CARC 异步验证不阻塞启动关键路径
- LRU 策略使频繁使用的波形保持在缓存中（vs 全量清除）
- 累积均值消除前 60 帧归零导致的性能数据偏差
- 动态分辨率使粒子系统随窗口 resize 自适应

## Related Issues

- `docs/solutions/runtime-errors/thread-safety-s_mainthreadid-crash.md` — SoLoudAudioEngine 断言基于此修复
- `docs/solutions/architecture-patterns/freetype-context-shared-instance.md` — FreeType 共享实例
