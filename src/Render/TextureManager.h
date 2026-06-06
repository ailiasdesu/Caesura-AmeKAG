#pragma once

#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <functional>

namespace Caesura {

using TextureCallback = std::function<void(bgfx::TextureHandle)>;

// ---------------------------------------------------------------------------
// TextureManager -- dedicated texture lifecycle manager
// ---------------------------------------------------------------------------
// Caches loaded textures by path / key, provides solid-color and placeholder
// texture factories, and tracks texture metadata (size, format).
//
// RenderBinding delegates all texture operations here so the caching policy
// lives in a single place instead of being scattered across Lua bindings.

class TextureManager {
public:
    static TextureManager& instance();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    bool initialize();
    void shutdown();

    // Load from disk (png, jpg, tga, bmp, etc.)
    // Uses bx::FileReader + bimg::imageParse; falls back to stb_image.
    // Returns texture id (opaque uint32_t), or 0 on failure.
    uint32_t loadTexture(const std::string& path);

    // Load from memory buffer (e.g. from XP3 archive)
    uint32_t loadTextureFromMemory(const uint8_t* data, uint32_t size,
                                   const std::string& cacheKey = "");

    // Create a 1x1 solid-color texture -- useful for fade-to-color
    bgfx::TextureHandle createSolidTexture(uint8_t r, uint8_t g, uint8_t b,
                                           uint8_t a = 255);

    // Register an externally-created bgfx texture into the cache.
    // Returns a cache id so it can be looked up later.
    uint32_t registerTexture(bgfx::TextureHandle tex);

    // Purple/black 16x16 checkerboard -- fallback for missing assets
    bgfx::TextureHandle getPlaceholderTexture();

    // Destroy a single cached texture; invalidates any outstanding handles
    void destroyTexture(uint32_t id);

    // Look up bgfx handle by cache id; returns BGFX_INVALID_HANDLE if not found
    bgfx::TextureHandle getTextureHandle(uint32_t id) const;

    // Query texture dimensions via bgfx::getTextureInfo
    void getTextureSize(bgfx::TextureHandle handle, uint16_t& width,
                        uint16_t& height) const;
    void getTextureSizeById(uint32_t id, uint16_t& width,
                            uint16_t& height) const;

    // Check whether a given id is in the cache and valid
    bool isValid(uint32_t id) const;

private:
    TextureManager() = default;

    bgfx::TextureHandle buildCheckerboardTexture();
    bgfx::TextureHandle loadFromFile(const std::string& path);
    bgfx::TextureHandle loadFromMemory(const uint8_t* data, uint32_t size);

    std::unordered_map<uint32_t, bgfx::TextureHandle> m_cache;
    bgfx::TextureHandle m_placeholderTex = BGFX_INVALID_HANDLE;
    uint32_t m_nextId = 1;
    bool m_initialized = false;
};

} // namespace Caesura

