# Live2D Integration Plan — Caesura (AmeKAG)

> 2026-06-08 | P2 | 预估 5d

## Legal Safety Design

**原则：引擎保持纯 MIT。Live2D 是可选插件，不污染协议。**

```cmake
option(CAESURA_LIVE2D "Live2D Cubism support (requires SDK)" OFF)

if(CAESURA_LIVE2D)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/Live2DCubismSDK/Core")
        message(FATAL_ERROR
            "Live2D Cubism SDK not found.\n"
            "  Download: https://www.live2d.com/en/download/cubism-sdk/\n"
            "  Extract to: external/Live2DCubismSDK/\n"
            "  Or disable: -DCAESURA_LIVE2D=OFF")
    endif()
    add_subdirectory(external/Live2DCubismSDK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_HAS_LIVE2D=1)
endif()
```

- 默认 `OFF`，引擎编译不依赖 Cubism SDK
- 用户主动 `-DCAESURA_LIVE2D=ON` 时才加载
- 所有 Live2D 代码在 `#ifdef CAESURA_HAS_LIVE2D` 守卫内
- README 注明："Live2D 需自行下载 SDK，接受其授权条款"

---

## Integration Steps

### Phase 1: nlohmann/json + CMake 框架 (1d)

| Step | 内容 |
|---|---|
| 1.1 | CMake FetchContent: nlohmann/json (MIT, header-only) |
| 1.2 | 替换 SaveManager → nlohmann::json |
| 1.3 | `CAESURA_LIVE2D` option + 守卫宏 |
| 1.4 | Live2D SDK 检测 + 错误提示 |

### Phase 2: Live2D 模块 (2d)

| Step | 内容 | 文件 |
|---|---|---|
| 2.1 | `Live2DModel`: .model3.json → 纹理/动作/参数 | `src/Animation/Live2D/Live2DModel.h/.cpp` |
| 2.2 | `Live2DRenderer`: bgfx 纹理四边形 | `src/Animation/Live2D/Live2DRenderer.h/.cpp` |
| 2.3 | `Live2DAnimator`: 动作/表情 + 参数插值 | `src/Animation/Live2D/Live2DAnimator.h/.cpp` |
| 2.4 | `#ifdef CAESURA_HAS_LIVE2D` 守卫所有文件 | — |

### Phase 3: Lua API (1d)

| API | 功能 |
|---|---|
| `KAG.load_live2d(path, name)` | 加载模型 |
| `KAG.show_live2d(name, x, y, scale)` | 显示 |
| `KAG.hide_live2d(name)` | 隐藏 |
| `KAG.play_live2d_motion(name, motion)` | 播放动作 |
| `KAG.set_live2d_expression(name, expr)` | 表情 |
| `KAG.set_live2d_param(name, param, value)` | 参数控制 |
| `KAG.set_live2d_opacity(name, opacity)` | 透明度 |

### Phase 4: 渲染 + Demo (1d)

---

## File Structure

```
external/Live2DCubismSDK/     ← 用户自行下载（不在 git 内）
external/nlohmann/            ← CMake FetchContent (MIT, 自动)
src/Animation/
├── IAnimationBackend.h       ← 抽象接口
├── NullAnimationBackend.h    ← 默认空实现
└── Live2D/                   ← 全部在 #ifdef CAESURA_HAS_LIVE2D 内
    ├── Live2DModel.h/.cpp
    ├── Live2DRenderer.h/.cpp
    ├── Live2DAnimator.h/.cpp
    └── Live2DBinding.h/.cpp
```

## Legal Checklist

- [x] Cubism SDK 不在仓库内（`.gitignore` + 用户下载）
- [x] 默认 `OFF`，不编译任何 Live2D 代码
- [x] 所有 Live2D 文件 `#ifdef` 守卫
- [x] README 注明可选依赖及授权条款
- [x] MIT 许可不因 Live2D 选项而改变
