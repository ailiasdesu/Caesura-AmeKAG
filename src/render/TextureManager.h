#pragma once

#include "api/ITextureManager.h"
#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <list>

namespace Caesura {

using TextureCallback = std::function<void(bgfx::TextureHandle)>;

// ============================================================================
// TextureManager -- implements ITextureManager
// ============================================================================

class TextureManager : public ITextureManager {
public:
    static TextureManager& instance();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    bool initialize() override;
    void shutdown() override;
    void setDevMode(bool dev) override;

    uint32_t loadTexture(const std::string& path) override;
    uint32_t loadTextureFromMemory(const uint8_t* data, uint32_t size,
                                   const std::string& cacheKey = "") override;
    uint32_t loadTextureFromRGBA(const uint8_t* rgba, uint16_t w, uint16_t h,
                                 const std::string& cacheKey = "") override;

    bgfx::TextureHandle createSolidTexture(uint8_t r, uint8_t g, uint8_t b,
                                           uint8_t a = 255) override;
    uint32_t registerTexture(bgfx::TextureHandle tex) override;
    bgfx::TextureHandle getPlaceholderTexture() override;

    void destroyTexture(uint32_t id) override;
    bgfx::TextureHandle getTextureHandle(uint32_t id) const override;
    void getTextureSize(bgfx::TextureHandle handle, uint16_t& width,
                        uint16_t& height) const override;
    void getTextureSizeById(uint32_t id, uint16_t& width,
                            uint16_t& height) const override;
    bool isValid(uint32_t id) const override;

    uint64_t totalTextureBytes() const override { return m_totalBytes; }
    void checkBudget(uint32_t id, uint16_t w, uint16_t h) override;
    void trackTexture(uint32_t id, uint32_t bytes) override;
    void untrackTexture(uint32_t id) override;

private:
    TextureManager() = default;

    bgfx::TextureHandle buildCheckerboardTexture();
    bgfx::TextureHandle loadFromFile(const std::string& path);
    bgfx::TextureHandle loadFromMemory(const uint8_t* data, uint32_t size);

    std::unordered_map<uint32_t, bgfx::TextureHandle> m_cache;
    bgfx::TextureHandle m_placeholderTex = BGFX_INVALID_HANDLE;
    bool m_devMode = true;
    std::unordered_map<uint32_t, uint32_t> m_textureSizes;
    std::list<uint32_t> m_textureLRU;
    uint64_t m_totalBytes = 0;
    uint32_t m_nextId = 1;
    bool m_initialized = false;
};

} // namespace Caesura