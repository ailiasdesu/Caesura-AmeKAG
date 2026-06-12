#pragma once
#include <cstdint>
#include <string>

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

    // Creates a 1x1 solid colour texture, registers it internally, returns TextureManager ID.
    virtual uint32_t createSolidTexture(uint8_t r, uint8_t g, uint8_t b,
                                        uint8_t a = 255) = 0;
    // Returns placeholder texture's raw bgfx handle idx, or 0 if not created.
    virtual uint32_t getPlaceholderTexture() = 0;

    virtual void destroyTexture(uint32_t id) = 0;
    // Returns raw bgfx TextureHandle.idx as uint32_t for the given TextureManager ID.
    // Returns 0 if the ID is unknown or the texture is invalid.
    virtual uint32_t getTextureHandle(uint32_t id) const = 0;
    virtual void getTextureSizeById(uint32_t id, uint16_t& width,
                                    uint16_t& height) const = 0;
    virtual bool isValid(uint32_t id) const = 0;

    virtual uint64_t totalTextureBytes() const = 0;
    virtual void checkBudget(uint32_t id, uint16_t w, uint16_t h) = 0;
    virtual void trackTexture(uint32_t id, uint32_t bytes) = 0;
    virtual void untrackTexture(uint32_t id) = 0;
};

} // namespace Caesura
