# Live2D Integration Plan — Caesura (AmeKAG)

> 2026-06-08 | P2 → 首项 | 预估 5d

## Prerequisites

### Cubism SDK for Native

**获取:** https://www.live2d.com/en/download/cubism-sdk/
- 手动下载，需接受授权协议
- 免费：年收入 < ¥10M（~$70k USD）
- 解压到 `external/Live2DCubismSDK/`
- 包含：`Core/`（头文件）+ 平台预编译库（Win/macOS/Linux/iOS/Android）

**nlohmann/json:** CMake FetchContent，header-only，MIT 许可

---

## Integration Steps

### Phase 1: 依赖接入 (1d)

| Step | 内容 | 文件 |
|---|---|---|
| 1.1 | CMake: add_subdirectory Cubism Core + nlohmann FetchContent | `CMakeLists.txt` |
| 1.2 | 验证：Win/macOS/Linux 三平台编译通过 | CI |
| 1.3 | `#include <nlohmann/json.hpp>` 替换所有手写 JSON | `SaveManager.cpp` 等 |

### Phase 2: Live2D 模块 (2d)

| Step | 内容 | 文件 |
|---|---|---|
| 2.1 | `Live2DModel` 类：加载 `.model3.json` → 解析纹理/动作/参数 | `src/Animation/Live2D/Live2DModel.h/.cpp` |
| 2.2 | `Live2DRenderer` 类：bgfx 纹理四边形渲染 | `src/Animation/Live2D/Live2DRenderer.h/.cpp` |
| 2.3 | `Live2DAnimator` 类：动作/表情切换 + 参数插值 | `src/Animation/Live2D/Live2DAnimator.h/.cpp` |
| 2.4 | 异步加载：Live2D 模型后台加载 → 主线程回调 | JobSystem 集成 |

### Phase 3: Lua API (1d)

| Step | 内容 |
|---|---|
| 3.1 | `KAG.load_live2d(modelPath, name)` → 加载模型 |
| 3.2 | `KAG.show_live2d(name, x, y, scale)` → 显示 |
| 3.3 | `KAG.hide_live2d(name)` → 隐藏 |
| 3.4 | `KAG.set_live2d_param(name, paramId, value)` → 参数控制 |
| 3.5 | `KAG.play_live2d_motion(name, motionName)` → 播放动作 |
| 3.6 | `KAG.set_live2d_expression(name, exprName)` → 表情切换 |
| 3.7 | `KAG.set_live2d_opacity(name, opacity)` → 透明度 |

### Phase 4: 渲染集成 (1d)

| Step | 内容 |
|---|---|
| 4.1 | bgfx View 分配（Live2D 独占 view 或与 fg 层共享） |
| 4.2 | Layer 系统集成：Live2D 节点挂载到 layer tree |
| 4.3 | Demo 验证：加载 Haru/Airi 示例模型 |

---

## File Structure

```
external/Live2DCubismSDK/     ← 手动放置
external/nlohmann/             ← CMake FetchContent
src/Animation/Live2D/
├── Live2DModel.h/.cpp
├── Live2DRenderer.h/.cpp
├── Live2DAnimator.h/.cpp
├── Live2DBinding.h/.cpp       ← Lua 绑定
└── CMakeLists.txt
```

---

## Risk

| 风险 | 缓解 |
|---|---|
| Cubism SDK 非 vendorable（需手动下载） | README 写明获取方式，CI 用条件编译 |
| macOS/Linux 预编译库兼容性 | Cubism 官方提供三平台 lib，版本锁定 |
| 渲染性能（Live2D 每帧更新网格） | bgfx 批量提交 + JobSystem 分担 |

---

## Success Criteria

- [ ] 加载 Haru 示例模型，渲染到屏幕
- [ ] 播放 idle 动作循环
- [ ] 切换表情（开心/生气）
- [ ] Lua API 全部 7 个函数可用
- [ ] 三平台 CI 通过（Live2D SDK 条件编译）
