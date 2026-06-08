---
title: "Multi-Core Adaptation: JobSystem + AsyncLoader Merge & Layer Rendering Crash"
date: 2026-06-08
category: runtime-errors
module: Core
problem_type: runtime_error
component: development_workflow
symptoms:
  - "ACCESS_VIOLATION (0xC0000005) crash at ~20s during demo phase 1→2 transition"
  - "draw=0: layers submit_batch using RTT view ID where tex texture ID expected"
  - "Crash triggers after RTT 5 creation (1x4 progress bar), before phase 2"
  - "bgfx debug text shader FATAL on D3D11 (pre-existing, non-fatal)"
  - "`set_layer_image` overwrites `node.rt` (RTT handle) with texture ID, breaking submit_batch fallback"
root_cause: logic_error
resolution_type: code_fix
severity: critical
tags:
  - multi-core
  - jobsystem
  - asyncloader
  - work-stealing
  - layers
  - rtt
  - submit-batch
  - texture-handle
  - bgfx
related_components:
  - "JobSystem (C++)"
  - "AsyncLoader (C++)"
  - "TextureManager (C++)"
  - "layers.lua"
  - "RenderBinding (C++)"
  - "BgfxRenderDevice (C++)"
  - "Engine.cpp"
---

# Multi-Core Adaptation: JobSystem + AsyncLoader Merge & Layer Rendering Crash

## Problem

合并 mu-q 的多核提交（JobSystem 工作窃取线程池 + AsyncLoader 纹理异步解码）后，demo 在阶段 1→2 过渡时发生 ACCESS_VIOLATION 崩溃。同时 layers 渲染管道因 `set_layer_image` 覆盖 RTT 句柄导致 `draw=0`。

## Symptoms

- Demo 启动后 ~20s，阶段 1→2 转换时在 RTT 5 创建后立即 ACCESS_VIOLATION 崩溃
- `draw=0`：layers 的 `submit_batch` 使用 RTT view ID 查找 tex 纹理 ID，键名不匹配
- `[bgfx FATAL]` D3D11 debug text shader 编译失败（预存问题，非致命）
- 合并后编译错误：`std::vector<WorkQueue>` mutex 不可移动、`void*` 类型转换

## What Didn't Work

- 禁用 bgfx debug text overlay：崩溃依旧，说明 crash 不在 debug text 路径
- 移除 `submit_batch` 中的 debug fprintf：无影响
- 单独诊断 RTT 创建路径：`createRenderTarget(1,4)` 本身成功，崩溃在下游

## Solution

### 1. 编译修复（mu-q 合并后）

| 文件 | 修复 |
|------|------|
| `src/Core/JobSystem.h` | `std::vector<WorkQueue>` → `std::vector<std::unique_ptr<WorkQueue>>`（mutex 不可移动） |
| `src/Core/JobSystem.cpp` | 所有 `m_queues[x].method()` → `m_queues[x]->method()` |
| `src/Resource/ImageDecoder.cpp:37` | `void*` → `static_cast<const uint8_t*>` |

### 2. Layer rt/tex 分离（layers.lua）

**根因**：`set_layer_image` 用纹理 ID 覆盖了 `node.rt`（RTT 句柄），导致 `submit_batch` 在 C++ 侧用纹理 ID 去 `m_rttMap` 查 RTT，返回无效句柄后触发 ACCESS_VIOLATION。

```lua
// layers.lua — BEFORE (broken)
function Layers.set_layer_image(node, tex_id, ...)
    node.rt = tex_id  -- 覆盖 RTT 句柄！
    ...
end

// layers.lua — AFTER (fixed)
function Layers.set_layer_image(node, tex_id, ...)
    node.tex = tex_id  -- 独立纹理字段
    ...
end
```

batch 命令新增 `tex` 字段：
```lua
batch.commands[#batch.commands + 1] = {
    tex        = node.tex or 0,  -- 新增：直接纹理查找
    rt         = node.rt,         -- RTT 句柄用于回退
    ...
}
```

### 3. 清理

- `RenderBinding.cpp`：移除 `static int batchFrame` debug fprintf
- `Engine.cpp`：移除 dbgText 调用（D3D11 上 shader 编译失败）

## Why This Works

`submit_batch` 的 C++ 代码先尝试 `getTexHandle(texId)` 从 TextureManager 查找纹理，失败后才回退到 `getViewportTexture(rtId)` 查找 RTT map。

修复前：`node.rt` 是纹理 ID → `getViewportTexture(ViewportHandle{texId})` 在 RTT map 中找不到 → 返回 `BGFX_INVALID_HANDLE` → 崩溃或 `draw=0`

修复后：`tex` 字段携带纹理 ID → `getTexHandle(texId)` 直接命中 → `blitTexture` 正常绘制 → `draw=3`

## Prevention

- **Layer 节点设计**：`rt`（RTT 句柄）和 `tex`（纹理 ID）必须分离，不可互用
- **submit_batch 协议**：batch 命令必须同时携带 `tex` 和 `rt` 两个字段
- **多核合并后**：任何涉及指针/所有权的代码都必须检查线程安全性（mutex 不可移动、类型转换明确）
- **JobSystem 集成测试**：合并异步加载后需验证主线程渲染管道不受竞态影响

## Related Issues

- mu-q 提交 `8d41cd6`（AsyncLoader）+ `3f7fd26`（JobSystem）
- `docs/solutions/runtime-errors/kag-backend-circular-delegation-stack-overflow.md`（前序修复）
