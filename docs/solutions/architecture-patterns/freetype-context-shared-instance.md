---
title: "FreeTypeContext Singleton: Shared FT_Library for FontRenderer & TextRenderer"
date: 2026-06-07
category: architecture-patterns
module: Render/FreeTypeContext
problem_type: architecture_pattern
component: development_workflow
severity: medium
applies_when:
  - "Multiple subsystems need the same FreeType library instance"
  - "Font rendering and text layout share a font atlas"
  - "Engine owns subsystem lifecycle; subsystems should not independently init/destroy shared resources"
related_components: ["Render/FontRenderer", "Render/TextRenderer"]
tags: [freetype, singleton, font-rendering, shared-resource, engine-lifecycle]
---

# FreeTypeContext Singleton: Shared FT_Library

## Context

`FontRenderer` 和 `TextRenderer` 各自独立调用 `FT_Init_FreeType()` 和 `FT_Done_FreeType()`，导致双份 `FT_Library` 实例。规范 [10.2.68] 要求全局共享单一 FreeType 实例。

## Guidance

创建 `FreeTypeContext` 单例，由 `Engine::init()` 显式初始化（早于所有 Render 子系统），`Engine::shutdown()` 中销毁。所有消费者通过 `FreeTypeContext::instance().getLibrary()` 获取共享指针。

```cpp
// FreeTypeContext.h
class FreeTypeContext {
public:
    static FreeTypeContext& instance();
    bool init();
    void shutdown();
    FT_Library getLibrary() const { return m_library; }
private:
    FT_Library m_library = nullptr;
    bool m_initialized = false;
};
```

**接入模式（FontRenderer 示例）:**
```cpp
// 初始化：替换 FT_Init_FreeType
bool FontRenderer::loadFont(const char* fontPath, int fontSize) {
    m_ftLibrary = FreeTypeContext::instance().getLibrary();
    if (!m_ftLibrary) {
        fprintf(stderr, "[FontRenderer] FreeTypeContext not initialized\n");
        return false;
    }
    FT_Error ftErr = FT_New_Face(m_ftLibrary, fontPath.c_str(), 0, &m_ftFace);
    // ...
}

// 清理：不再调用 FT_Done_FreeType
~FontRenderer() {
    if (m_ftFace)    { FT_Done_Face(m_ftFace); m_ftFace = nullptr; }
    if (m_ftLibrary) { /* owned by FreeTypeContext */ m_ftLibrary = nullptr; }
}
```

**TextRenderer 同理** — `TTFState::ftLib` 从 FreeTypeContext 获取，析构时仅置空。

**Engine 集成:**
```cpp
// Engine::init() — 早于 FontRenderer/TextRenderer
if (!FreeTypeContext::instance().init()) {
    fprintf(stderr, "[Engine] FreeTypeContext init failed.\n");
    return false;
}

// Engine::shutdown() — 晚于所有 Render 子系统
FreeTypeContext::instance().shutdown();
```

## Why This Matters

- 节省 ~2MB 内存（单一 FT_Library vs 双份）
- 共享字体缓存和字形数据
- 生命周期由 Engine 统一管理，避免子系统间初始化顺序依赖
- Debug 构建中 `CAESURA_ASSERT_MAIN_THREAD()` 保证线程安全

## When to Apply

- 任何需要 FreeType 的新子系统都应通过 FreeTypeContext 获取，而非自行 `FT_Init_FreeType`
- 添加新字体渲染功能时检查是否已有共享实例

## Examples

**改造前 (FontRenderer):**
```cpp
FT_Error ftErr = FT_Init_FreeType(&m_ftLibrary);
// ... use m_ftLibrary ...
if (m_ftLibrary) { FT_Done_FreeType(m_ftLibrary); }
```

**改造后:**
```cpp
m_ftLibrary = FreeTypeContext::instance().getLibrary();
if (!m_ftLibrary) return false;
// ... use m_ftLibrary ...
// 析构时仅置空，不调用 FT_Done_FreeType
```

## Related

- `docs/solutions/build-errors/alpha-completion-build-fixes.md` — 接入过程中的编译损坏修复
- `docs/Caesura_功能实现规格说明书_整合版.md` [10.2.68] — 全局共享 FreeType 决策
