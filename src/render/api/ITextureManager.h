#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <cstdint>

namespace Caesura {

// ============================================================================
// ITextureManager — pure virtual interface for texture lifecycle
// ============================================================================
// TextureManager implements this interface. BackendRegistry stores ITextureManager*.

class ITextureManager {
public:
    virtual ~ITextureManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void setDevMode(bool dev) = 0;

    virtual uint32_t loadTexture(const std::string& path) = 0;
    virtual uint32_t loadTextureFromMemory(const uint8_t* data, uint32_t size,
                                           const std::string& cacheKey = "") = 0;
    virtual uint32_t loadTextureFromRGBA(const uint8_t* rgba, uint16_t w, uint16_t h,
                                         const std::string& cacheKey = "") = 0;

    virtual bgfx::TextureHandle createSolidTexture(uint8_t r, uint8_t g, uint8_t b,
                                                   uint8_t a = 255) = 0;
    virtual uint32_t registerTexture(bgfx::TextureHandle tex) = 0;
    virtual bgfx::TextureHandle getPlaceholderTexture() = 0;

    virtual void destroyTexture(uint32_t id) = 0;
    virtual bgfx::TextureHandle getTextureHandle(uint32_t id) const = 0;
    virtual void getTextureSize(bgfx::TextureHandle handle, uint16_t& width,
                                uint16_t& height) const = 0;
    virtual void getTextureSizeById(uint32_t id, uint16_t& width,
                                    uint16_t& height) const = 0;
    virtual bool isValid(uint32_t id) const = 0;

    virtual uint64_t totalTextureBytes() const = 0;
    virtual void checkBudget(uint32_t id, uint16_t w, uint16_t h) = 0;
    virtual void trackTexture(uint32_t id, uint32_t bytes) = 0;
    virtual void untrackTexture(uint32_t id) = 0;
};

} // namespace Caesura
