## Phase 5: 高级渲染 (P1)

**目标:** 28 混合模式、LUT 调色板、脏矩形 Scissor、文本批处理、视频播放。

**依赖:** Phase 2 | **验收:** 混合模式切换生效, LUT 调色正常, 脏矩形优化后帧率提升

### Task 5.1: 28 种混合模式着色器
**Files:** Create: `shaders/fs_blend/`, `src/render/ShaderCache.h`

```glsl
// shaders/fs_blend/varying.def.sc
vec4 v_color0 : COLOR0;
vec2 v_texcoord0 : TEXCOORD0;

// 以 Multiply 为例:
// shaders/fs_blend/fs_multiply.sc
$input v_texcoord0, v_color0
#include <bgfx_shader.sh>
SAMPLER2D(s_tex, 0);

void main() {
    vec4 base = texture2D(s_tex, v_texcoord0);
    vec4 blend = v_color0;
    gl_FragColor = vec4(base.rgb * blend.rgb, base.a * blend.a);
}
```

```cpp
// src/render/ShaderCache.h - CompositeShaderCache
#pragma once
#include <bgfx/bgfx.h>
#include <unordered_map>
#include <string>

namespace caesura {
class ShaderCache {
public:
    static ShaderCache& instance();
    void init();
    bgfx::ProgramHandle getProgram(int blendMode, uint32_t paletteHash = 0);
    void precompileCommon();  // Alpha: 预编译 10 种常用组合
private:
    std::string makeKey(int blend, uint32_t palHash);
    std::unordered_map<std::string, bgfx::ProgramHandle> m_cache;
    static constexpr int MAX_CACHE = 64;  // LRU 上限
};
}
```

**实现:** 运行时按需编译 (首次 1-3ms), LRU 淘汰。key = hash(blend_mode, palette_ops, lut_texture_id)。Alpha 预编译: Normal, Multiply, Screen, Overlay, Add, Subtract, Darken, Lighten, SoftLight, HardLight。

### Task 5.2: LUT 实时调色板
**Files:** Create: `shaders/fs_palette.sc`, `scripts/palette.lua`

```glsl
// shaders/fs_palette.sc
$input v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_lut, 1);  // LUT 纹理 (256x16 或 64x64x64 3D)
uniform vec4 u_lut_params;  // x=size, y=intensity

void main() {
    vec3 color = texture2D(s_tex, v_texcoord0).rgb;
    // 标准 3D LUT 查找 (简化: 2D 展开)
    float b = floor(color.b * (u_lut_params.x - 1.0));
    float rg = color.r * (u_lut_params.x - 1.0) + b * u_lut_params.x;
    vec2 lut_uv = vec2(rg / (u_lut_params.x * u_lut_params.x), color.g);
    vec3 graded = texture2D(s_lut, lut_uv).rgb;
    gl_FragColor = vec4(mix(color, graded, u_lut_params.y), 1.0);
}
```

```lua
-- scripts/palette.lua
local palette = {}
function palette.load(lut_path)
    -- 加载 LUT PNG -> bgfx texture
    return backend.create_lut_texture(lut_path)
end
function palette.apply(lut_id, intensity)
    backend.set_palette(lut_id, intensity or 1.0)
end
function palette.clear()
    backend.clear_palette()
end
return palette
```

### Task 5.3: 脏矩形 Scissor 优化
**Files:** Modify: `src/render/LayerManager.h` (add dirty rect tracking)

```cpp
// LayerManager 新增:
struct DirtyRect { uint16_t x, y, w, h; };
std::vector<DirtyRect> m_dirtyRects;

void LayerManager::updateDirtyRegions() {
    m_dirtyRects.clear();
    for (auto& layer : m_layers) {
        if (!layer.dirty) continue;
        // 计算图层的脏区域 (位置+缩放变化 -> 屏幕空间矩形)
        DirtyRect r = computeLayerRect(layer);
        m_dirtyRects.push_back(r);
    }
    // 合并脏矩形 -> 取并集
    DirtyRect u = unionRect(m_dirtyRects);
    float ratio = (u.w * u.h) / float(screenW * screenH);
    if (ratio > 0.75f) {
        bgfx::setScissor(0, 0, screenW, screenH);  // 退化全量
    } else {
        bgfx::setScissor(u.x, u.y, u.w, u.h);
    }
}

void LayerManager::markDirtyWithTransparency(LayerType t) {
    // 透明混合图层: 向下层递归标记受影响区域 (Erase/Alpha 强制全图层)
    for (int i = t; i >= 0; i--) {
        m_layers[i].dirty = true;
    }
}
```

### Task 5.4: 文本批处理 (MessageLayerCache)
**Files:** Modify: `src/render/FontRenderer.h`

```cpp
// FontRenderer 新增缓存结构:
struct MessageLayerCache {
    std::vector<PosUVColorVertex> vertices;     // 顶点缓冲
    std::vector<uint16_t> indices;              // 索引缓冲
    bool dirty = true;
    int dirtyStart = 0, dirtyEnd = 0;          // 增量更新范围
    std::string currentText;                    // 当前文本内容 (用于 diff)
    int currentFontSize;
};

// 增量更新: [ch] 仅标记新字符范围
void FontRenderer::updateCache(MessageLayerCache& cache, const std::string& newText) {
    if (newText == cache.currentText) return;
    size_t diff = 0;
    while (diff < cache.currentText.size() && diff < newText.size()
           && cache.currentText[diff] == newText[diff]) diff++;
    cache.dirtyStart = diff;  // 从第一个不同字符开始重建
    cache.dirtyEnd = newText.size();
    cache.currentText = newText;
    cache.dirty = true;
}
```

### Task 5.5: 视频播放 (pl_mpeg MPEG1)
**Files:** Create: `src/render/VideoPlayer.h`, `src/render/VideoPlayer.cpp`

```cpp
// src/render/VideoPlayer.h
#pragma once
#define PL_MPEG_IMPLEMENTATION
#include <pl_mpeg.h>
#include <bgfx/bgfx.h>

namespace caesura {
class VideoPlayer {
public:
    bool load(const std::string& path);
    void play();
    void stop();
    void update(float dt);  // 每帧调用
    bgfx::TextureHandle getFrameTexture();
    bool isPlaying() { return m_playing; }

    // PTS 同步: 窗口 = |PTS_audio - PTS_video| <= (1/fps)
    void syncWithAudio(double audioPts);

private:
    plm_t* m_plm = nullptr;
    bgfx::TextureHandle m_frameTex = BGFX_INVALID_HANDLE;
    uint8_t* m_rgbBuffer = nullptr;
    bool m_playing = false;
    double m_currentPts = 0;
};
}
```

```cpp
// 帧更新 + PTS 同步
void VideoPlayer::update(float dt) {
    if (!m_playing || !m_plm) return;
    plm_decode(m_plm, dt);
    plm_frame* frame = plm_decode_video(m_plm);
    if (frame) {
        plm_frame_to_rgb(frame, m_rgbBuffer, m_width * 3);
        // 更新 bgfx 纹理
        bgfx::updateTexture2D(m_frameTex, 0, 0, 0, 0,
            m_width, m_height,
            bgfx::copy(m_rgbBuffer, m_width * m_height * 4));
        m_currentPts = plm_get_time(m_plm);
    }
}
```

### Phase 5 检查点
- [x] 10 种混合模式预编译可用
- [x] LUT 调色板加载和应用
- [x] 脏矩形 Scissor 优化 (union>75% 退化)
- [x] 文本批处理 O(1) draw call
- [x] MPEG1 视频播放 + PTS 同步

---
