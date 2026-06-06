## Phase 2: 渲染管线基础 (P0+P1)

**目标:** 三层图层渲染、纹理加载缓存、Quad 绘制管线、FreeType 字体初始化。

**依赖:** Phase 0 (RenderBackend) | **验收:** bg/fg/message 三层渲染正常, PNG 纹理加载显示

### Task 2.1: TextureManager
**Files:** Create: `src/render/TextureManager.h`, `src/render/TextureManager.cpp`

```cpp
// src/render/TextureManager.h
#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <functional>

namespace caesura {
using TextureCallback = std::function<void(bgfx::TextureHandle)>;

class TextureManager {
public:
    static TextureManager& instance();
    bool initialize();
    void shutdown();
    bgfx::TextureHandle loadTexture(const std::string& path);
    bgfx::TextureHandle loadTextureFromMemory(const uint8_t* data, uint32_t size, const std::string& cacheKey = "");
    bgfx::TextureHandle createSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    bgfx::TextureHandle getPlaceholderTexture();
    void getTextureSize(bgfx::TextureHandle handle, uint16_t& width, uint16_t& height);
private:
    TextureManager() = default;
    bgfx::TextureHandle createCheckerboardTexture();
    std::unordered_map<std::string, bgfx::TextureHandle> m_cache;
    bgfx::TextureHandle m_placeholderTex = BGFX_INVALID_HANDLE;
    bool m_initialized = false;
};
}
```

**实现要点:** loadTexture 检查缓存 -> bx::FileReader 读文件 -> bimg::imageParse 解码 -> bgfx::createTexture2D RGBA8 -> 存入 m_cache。占位纹理: 16x16 紫黑棋盘格。

### Task 2.2: LayerManager
**Files:** Create: `src/render/LayerManager.h`, `src/render/LayerManager.cpp`

```cpp
// src/render/LayerManager.h
#pragma once
#include <bgfx/bgfx.h>
#include <array>

namespace caesura {
struct Layer { bgfx::TextureHandle tex = BGFX_INVALID_HANDLE; float x=0,y=0,sx=1,sy=1,opacity=1; bool visible=false,dirty=true; int blend=0; };

class LayerManager {
public:
    enum LayerType { BG=0, FG=1, MSG=2, COUNT=3 };
    static LayerManager& instance();
    void init(); void shutdown();
    Layer& get(LayerType t);
    void setTex(LayerType t, bgfx::TextureHandle tex);
    void render(int w, int h);
    void markAllDirty();
private:
    void quad(bgfx::TextureHandle tex, float opacity);
    Layer m_l[3]; bgfx::ProgramHandle m_prog = BGFX_INVALID_HANDLE;
};
}
```

**渲染管线:** 每帧 bgfx::touch(0) -> BG->FG->MSG Z-order -> allocTransientBuffers(4顶点+6索引) -> fill Quad NDC+UV -> setTexture + BLEND_ALPHA -> submit(0, program)

### Task 2.3: layers.lua
**Files:** Create: `scripts/layers.lua`

```lua
local layers = {}
local ids = { bg=0, fg=1, message=2 }
function layers.load(layer, path) local id=ids[layer]; if id then backend.set_layer_tex(id, path) end end
function layers.show(layer, v) local id=ids[layer]; if id then backend.set_layer_visible(id, v) end end
function layers.clear(layer) local id=ids[layer]; if id then backend.clear_layer(id) end end
function layers.clear_all() for k in pairs(ids) do backend.clear_layer(ids[k]) end end
return layers
```

### Task 2.4: FreeType FontRenderer 初始化
**Files:** Create: `src/render/FontRenderer.h`, `src/render/FontRenderer.cpp`

```cpp
// src/render/FontRenderer.h
#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <bgfx/bgfx.h>
#include <unordered_map>

namespace caesura {
struct Glyph { uint16_t x,y,w,h,adv,ox,oy; };

class FontRenderer {
public:
    static FontRenderer& instance();
    bool init(const std::string& fontPath);
    void shutdown();
    Glyph getGlyph(char32_t cp, int size);
    bgfx::TextureHandle atlas() { return m_atlas; }
    void renderText(const std::string& text, int size, float x, float y);
private:
    FT_Library m_ft=nullptr; FT_Face m_face=nullptr;
    bgfx::TextureHandle m_atlas = BGFX_INVALID_HANDLE;
    std::unordered_map<uint32_t,Glyph> m_cache;
    int m_layer=0; uint16_t m_px=0,m_py=0;
    static constexpr uint16_t AS=2048;
    uint8_t* m_buf=nullptr;
};
}
```

**初始化:** FT_Init_FreeType -> FT_New_Face(字体) -> FT_Set_Pixel_Sizes(0,48) -> TextureArray(2048,2048,4,RGBA8) -> 预填充 ASCII 32-126 到 Layer 0

### Phase 2 检查点
- [x] PNG 加载显示在 bg
- [x] bg/fg/message 三层独立
- [x] FreeType 图集初始化

---
