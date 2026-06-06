#include "TextureManager.h"
#include <bimg/decode.h>
#include <bx/file.h>
#include <bx/allocator.h>
#include <cstdio>
#include <cstring>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb/stb_image.h"

namespace Caesura {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

TextureManager& TextureManager::instance() {
    static TextureManager mgr;
    return mgr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool TextureManager::initialize() {
    if (m_initialized) return true;
    m_nextId = 1;
    m_cache.clear();
    buildCheckerboardTexture();
    m_initialized = true;
    printf("[TextureManager] Initialized.\n");
    return true;
}

void TextureManager::shutdown() {
    if (!m_initialized) return;

    for (auto& [id, tex] : m_cache) {
        if (bgfx::isValid(tex))
            bgfx::destroy(tex);
    }
    m_cache.clear();

    if (bgfx::isValid(m_placeholderTex)) {
        bgfx::destroy(m_placeholderTex);
        m_placeholderTex = BGFX_INVALID_HANDLE;
    }

    m_initialized = false;
    printf("[TextureManager] Shutdown complete.\n");
}

// ---------------------------------------------------------------------------
// Placeholder texture -- 16x16 purple/black checkerboard
// ---------------------------------------------------------------------------

bgfx::TextureHandle TextureManager::buildCheckerboardTexture() {
    if (bgfx::isValid(m_placeholderTex))
        return m_placeholderTex;

    const uint16_t size = 16;
    const uint32_t pixels = size * size;  // RGBA8 = 4 * 16 * 16 bytes
    uint32_t data[size * size];
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool white = ((x / 4) + (y / 4)) % 2 == 0;
            // Purple (0xFF8000FF) vs Black (0xFF000000)
            data[y * size + x] = white ? 0xFF8000FFu : 0xFF000000u;
        }
    }

    const bgfx::Memory* mem = bgfx::copy(data, sizeof(data));
    m_placeholderTex = bgfx::createTexture2D(
        size, size, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);

    if (!bgfx::isValid(m_placeholderTex)) {
        fprintf(stderr, "[TextureManager] Failed to create placeholder texture.\n");
    } else {
        printf("[TextureManager] Placeholder texture created (16x16 purple checkerboard).\n");
    }
    return m_placeholderTex;
}

bgfx::TextureHandle TextureManager::getPlaceholderTexture() {
    if (!bgfx::isValid(m_placeholderTex))
        buildCheckerboardTexture();
    return m_placeholderTex;
}

// ---------------------------------------------------------------------------
// Load from file
// ---------------------------------------------------------------------------

bgfx::TextureHandle TextureManager::loadFromFile(const std::string& path) {
    bx::FileReader reader;
    if (!bx::open(&reader, path.c_str())) {
        fprintf(stderr, "[TextureManager] File not found: %s\n", path.c_str());
        return BGFX_INVALID_HANDLE;
    }

    uint32_t size = (uint32_t)bx::getSize(&reader);
    std::vector<uint8_t> buf(size);
    bx::read(&reader, buf.data(), size, bx::ErrorAssert{});
    bx::close(&reader);

    return loadFromMemory(buf.data(), size);
}

bgfx::TextureHandle TextureManager::loadFromMemory(const uint8_t* data, uint32_t size) {
    bx::DefaultAllocator allocator;

    bimg::ImageContainer* img = bimg::imageParse(&allocator, data, size);

    if (!img) {
        // Fallback: stb_image
        int iw = 0, ih = 0, channels = 0;
        unsigned char* stbData = stbi_load_from_memory(
            data, (int)size, &iw, &ih, &channels, 4);
        if (!stbData) {
            fprintf(stderr, "[TextureManager] Decode failed (bimg + stb).\n");
            return BGFX_INVALID_HANDLE;
        }
        const bgfx::Memory* mem = bgfx::copy(stbData, (uint32_t)(iw * ih * 4));
        stbi_image_free(stbData);
        bgfx::TextureHandle tex = bgfx::createTexture2D(
            (uint16_t)iw, (uint16_t)ih, false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);
        if (!bgfx::isValid(tex)) {
            fprintf(stderr, "[TextureManager] GPU texture creation failed (stb).\n");
        }
        return tex;
    }

    bgfx::TextureFormat::Enum fmt = bgfx::TextureFormat::RGBA8;
    if (img->m_format == bimg::TextureFormat::BGRA8)
        fmt = bgfx::TextureFormat::BGRA8;
    else if (img->m_format == bimg::TextureFormat::RGB8)
        fmt = bgfx::TextureFormat::RGB8;

    const bgfx::Memory* mem = bgfx::makeRef(
        img->m_data, img->m_size,
        [](void*, void* ud) { bimg::imageFree((bimg::ImageContainer*)ud); },
        img);

    bgfx::TextureHandle tex = bgfx::createTexture2D(
        uint16_t(img->m_width), uint16_t(img->m_height),
        false, 1, fmt,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);

    if (!bgfx::isValid(tex)) {
        bimg::imageFree(img);
        fprintf(stderr, "[TextureManager] GPU texture creation failed (bimg).\n");
    }
    return tex;
}

// ---------------------------------------------------------------------------
// Public load API
// ---------------------------------------------------------------------------

uint32_t TextureManager::loadTexture(const std::string& path) {
    if (!m_initialized) {
        fprintf(stderr, "[TextureManager] Not initialized.\n");
        return 0;
    }

    // Reject path traversal
    if (path.find("..") != std::string::npos) {
        fprintf(stderr, "[TextureManager] Path traversal blocked: %s\n", path.c_str());
        return 0;
    }

    if (path.empty()) {
        fprintf(stderr, "[TextureManager] Empty path.\n");
        return 0;
    }

    bgfx::TextureHandle tex = loadFromFile(path);
    if (!bgfx::isValid(tex)) {
        fprintf(stderr, "[TextureManager] Failed to load: %s\n", path.c_str());
        return 0;
    }

    bgfx::TextureInfo info;
    bgfx::calcTextureSize(info, uint16_t(0), uint16_t(0), 1, false, false, 1,
                          bgfx::TextureFormat::RGBA8);

    uint32_t id = m_nextId++;
    m_cache[id] = tex;
    printf("[TextureManager] Loaded: %s -> id=%u\n", path.c_str(), id);
    return id;
}

uint32_t TextureManager::loadTextureFromMemory(const uint8_t* data, uint32_t size,
                                               const std::string& cacheKey) {
    if (!m_initialized) {
        fprintf(stderr, "[TextureManager] Not initialized.\n");
        return 0;
    }

    bgfx::TextureHandle tex = loadFromMemory(data, size);
    if (!bgfx::isValid(tex)) {
        fprintf(stderr, "[TextureManager] Failed to load from memory.\n");
        return 0;
    }

    uint32_t id = m_nextId++;
    m_cache[id] = tex;
    if (!cacheKey.empty()) {
        printf("[TextureManager] Loaded from memory (key=%s) -> id=%u\n",
               cacheKey.c_str(), id);
    } else {
        printf("[TextureManager] Loaded from memory -> id=%u\n", id);
    }
    return id;
}

// ---------------------------------------------------------------------------
// Solid color texture
// ---------------------------------------------------------------------------

bgfx::TextureHandle TextureManager::createSolidTexture(uint8_t r, uint8_t g,
                                                        uint8_t b, uint8_t a) {
    uint32_t pixel = (uint32_t(a) << 24) | (uint32_t(b) << 16) |
                     (uint32_t(g) << 8) | uint32_t(r);
    const bgfx::Memory* mem = bgfx::alloc(4);
    *reinterpret_cast<uint32_t*>(mem->data) = pixel;
    bgfx::TextureHandle tex = bgfx::createTexture2D(
        1, 1, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);
    return tex;
}

// ---------------------------------------------------------------------------
// Register externally-created texture
// ---------------------------------------------------------------------------

uint32_t TextureManager::registerTexture(bgfx::TextureHandle tex) {
    if (!bgfx::isValid(tex)) return 0;
    uint32_t id = m_nextId++;
    m_cache[id] = tex;
    return id;
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------

void TextureManager::destroyTexture(uint32_t id) {
    auto it = m_cache.find(id);
    if (it != m_cache.end()) {
        if (bgfx::isValid(it->second))
            bgfx::destroy(it->second);
        m_cache.erase(it);
        printf("[TextureManager] Texture %u destroyed.\n", id);
    }
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------

bgfx::TextureHandle TextureManager::getTextureHandle(uint32_t id) const {
    auto it = m_cache.find(id);
    if (it != m_cache.end() && bgfx::isValid(it->second))
        return it->second;
    return BGFX_INVALID_HANDLE;
}

bool TextureManager::isValid(uint32_t id) const {
    auto it = m_cache.find(id);
    return it != m_cache.end() && bgfx::isValid(it->second);
}

// ---------------------------------------------------------------------------
// Size query
// ---------------------------------------------------------------------------

void TextureManager::getTextureSize(bgfx::TextureHandle handle,
                                     uint16_t& width, uint16_t& height) const {
    bgfx::TextureInfo info;
    bgfx::calcTextureSize(info, 0, 0, 1, false, false, 1,
                          bgfx::TextureFormat::RGBA8);
    width  = info.width;
    height = info.height;
}

void TextureManager::getTextureSizeById(uint32_t id,
                                         uint16_t& width, uint16_t& height) const {
    auto it = m_cache.find(id);
    if (it != m_cache.end() && bgfx::isValid(it->second)) {
        getTextureSize(it->second, width, height);
        return;
    }
    width = 0; height = 0;
}

} // namespace Caesura

