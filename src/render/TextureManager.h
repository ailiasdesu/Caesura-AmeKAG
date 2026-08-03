#pragma once

#include "api/ITextureManager.h"
#include "../di/api/IDeviceLostListener.h"
#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <list>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Caesura {

class ISandboxQuota;

using TextureCallback = std::function<void(bgfx::TextureHandle)>;

// ============================================================================
// TextureManager -- implements ITextureManager
// ============================================================================

class TextureManager : public ITextureManager, public IDeviceLostListener {
public:
    TextureManager() = default;

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    bool initialize() override;
    bool initialize(bool gpuAvailable) override;
    void shutdown() override;
    void setDevMode(bool dev) override;

    uint32_t loadTexture(const std::string& path) override;
    uint32_t loadTextureFromMemory(const uint8_t* data, uint32_t size,
                                   const std::string& cacheKey = "") override;
    uint32_t loadTextureFromRGBA(const uint8_t* rgba, uint16_t w, uint16_t h,
                                 const std::string& cacheKey = "") override;

    // RGBA(32-bit) -> registered texture id for solid-color dedup.
    std::unordered_map<uint32_t, uint32_t> m_solidCache;

    uint32_t createSolidTexture(uint8_t r, uint8_t g, uint8_t b,
                                uint8_t a = 255) override;
    uint32_t getPlaceholderTexture() override;

    void destroyTexture(uint32_t id) override;
    // Path -> last texture id cache: repeated loads of the same file (the
    // common VN pattern of reusing backgrounds/sprites) reuse the existing
    // GPU texture instead of re-decoding + re-uploading.
    std::unordered_map<std::string, uint32_t> m_pathToId;
    uint32_t getTextureHandle(uint32_t id) const override;
    void getTextureSizeById(uint32_t id, uint16_t& width,
                            uint16_t& height) const override;
    bool isValid(uint32_t id) const override;

    uint64_t totalTextureBytes() const override { return m_totalBytes; }
    bool checkBudget(uint32_t id, uint16_t w, uint16_t h) override;
    void trackTexture(uint32_t id, uint64_t bytes) override;
    void untrackTexture(uint32_t id) override;

    // IDeviceLostListener
    void onDeviceLost() override;
    void onDeviceRestored() override;

private:
    enum class RestoreSourceKind : uint8_t {
        Encoded,
        Rgba
    };

    struct RestoreSource {
        RestoreSourceKind kind = RestoreSourceKind::Encoded;
        std::vector<uint8_t> bytes;
        uint16_t width = 0;
        uint16_t height = 0;
    };

    class QuotaReservation;

    bgfx::TextureHandle buildCheckerboardTexture();
    bgfx::TextureHandle loadFromFile(const std::string& path,
                                     uint16_t& width, uint16_t& height,
                                     std::vector<uint8_t>& encodedBytes);
    bgfx::TextureHandle loadFromMemory(const uint8_t* data, uint32_t size,
                                       uint16_t& width, uint16_t& height);
    bgfx::TextureHandle createFromRGBA(const uint8_t* rgba,
                                       uint16_t width, uint16_t height);
    bgfx::TextureHandle restoreTexture(const RestoreSource& source,
                                       uint16_t& width, uint16_t& height);
    bool validateTextureDimensions(uint32_t width, uint32_t height,
                                   uint64_t& rgbaBytes) const;

    // Internally create bgfx texture and register in cache, return TM ID.
    uint32_t registerTexture(bgfx::TextureHandle tex,
                             uint16_t width, uint16_t height,
                             RestoreSource&& restoreSource,
                             QuotaReservation& quotaReservation);

    std::unordered_map<uint32_t, bgfx::TextureHandle> m_cache;
    bgfx::TextureHandle m_placeholderTex = BGFX_INVALID_HANDLE;
    bool m_devMode = true;
    bool m_gpuAvailable = true;
    std::unordered_map<uint32_t, uint64_t> m_textureSizes;
    std::unordered_map<uint32_t, std::pair<uint16_t, uint16_t>> m_textureDimensions;
    std::unordered_map<uint32_t, RestoreSource> m_restoreSources;
    std::unordered_map<uint32_t, ISandboxQuota*> m_quotaReservations;
    std::list<uint32_t> m_textureLRU;
    uint64_t m_totalBytes = 0;
    uint32_t m_nextId = 1;
    bool m_initialized = false;
};

} // namespace Caesura
