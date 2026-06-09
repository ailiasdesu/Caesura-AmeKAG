---
title: "Multi-Core Adaptation: JobSystem + AsyncLoader Merge & Layer Rendering Crash"
date: 2026-06-08
last_reviewed: 2026-06-09
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
status: active
last_updated: 2026-06-08
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

### 1. ?????mu-q ????

| ?? | ?? |
|------|------|
| `src/Core/JobSystem.h` | `std::vector<WorkQueue>` ? `std::vector<std::unique_ptr<WorkQueue>>`?mutex ????? |
| `src/Core/JobSystem.cpp` | ?? `m_queues[x].method()` ? `m_queues[x]->method()` |
| `src/Resource/ImageDecoder.cpp:37` | `void*` ? `static_cast<const uint8_t*>` |

### 2. Layer rt/tex ???layers.lua?? ???????

**??**?`set_layer_image` ??? ID ??? `node.rt`?RTT ?????? `submit_batch` ? C++ ???? ID ? `m_rttMap` ? RTT?????????? ACCESS_VIOLATION?

```lua
// layers.lua ? BEFORE (broken)
function Layers.set_layer_image(node, tex_id, ...)
    node.rt = tex_id  -- ?? RTT ???
    ...
end

// layers.lua ? AFTER (fixed)
function Layers.set_layer_image(node, tex_id, ...)
    node.tex = tex_id  -- ??????
    ...
end
```

batch ???? `tex` ???
```lua
batch.commands[#batch.commands + 1] = {
    tex        = node.tex or 0,  -- ?????????
    rt         = node.rt,         -- RTT ??????
    ...
}
```

### 3. C++ ????resolveTexture ??????RenderBinding.cpp?? 2026-06-08

**Lua ??? `rt`/`tex` ????????????? C++ ?**?`submit_batch` ?7?????????????????????? TextureManager??? RTT viewport map?

```cpp
// ?? helper?????????? TextureManager??? RTT map
static bgfx::TextureHandle resolveTexture(uint32_t id, IRenderDevice* dev) {
    bgfx::TextureHandle tex = TextureManager::instance().getTextureHandle(id);
    if (!bgfx::isValid(tex) && dev && id != 0) {
        ViewportHandle vp{ id };
        tex = dev->getViewportTexture(vp);
    }
    return tex;
}
```

**??? 7 ???**????? `resolveTexture`??
| ?? | ??? | ??? |
|------|--------|--------|
| `submit_batch` | `getTexHandle` + ?? `rt` key ?? | `resolveTexture(texId)` + `rt` key ?? |
| `submit_blend` | `getTexHandle` ??? | `resolveTexture` ??? |
| `submit_transition` | `getTexHandle` ??? | `resolveTexture` ??? |
| `submit_vfx` | `getTexHandle` ??? | `resolveTexture` ??? |
| `stretch_blt` | `getTexHandle` ??? | `resolveTexture` ??? |
| `affine_blt` | `getTexHandle` ??? | `resolveTexture` ??? |
| `fill_viewport` | ?? `dev` ????? bug? | ?? `getRender(L)` |

### 4. ??

- `RenderBinding.cpp`??? `static int batchFrame` debug fprintf
- `Engine.cpp`??? dbgText ???D3D11 ? shader ?????

## Why This Works

**????**?

1. **Lua ?**?`set_layer_image` ? `rt` ? `tex` ?????batch ??????????
2. **C++ ?**?????`resolveTexture()` ????? ID ??? TextureManager??? ID??????? RTT viewport map?viewport ID??????????

????`node.rt` ??? ID ? `getViewportTexture(ViewportHandle{texId})` ? RTT map ???? ? ?? `BGFX_INVALID_HANDLE` ? ??? `draw=0`

????`resolveTexture(id)` ??????????? ? ?? Lua ????? ID ?? RTT view ID ?????? ? `draw=3`

## Prevention

- **Layer ????**?`rt`?RTT ???? `tex`??? ID??????????
- **submit_batch ??**?batch ???????? `tex` ? `rt` ????
- **C++ ???**??????? ID ??? binding ?????? `resolveTexture()`????? `getTexHandle()`
- **?????**???????/?????????????????mutex ????????????
- **JobSystem ????**????????????????????????

## Related Issues

- mu-q ?? `8d41cd6`?AsyncLoader?+ `3f7fd26`?JobSystem?
- `docs/solutions/runtime-errors/kag-backend-circular-delegation-stack-overflow.md`??????

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
