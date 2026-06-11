// ISteamBackend — pure virtual interface for Steam SDK integration
// Pattern: mirrors Live2D (IAnimationBackend), optional module with #ifdef guard
// Steam SDK is NOT committed — users download Steamworks separately
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Caesura {

struct SteamAchievement {
    std::string id;
    bool unlocked = false;
    float unlockTime = 0.0f;
};

class ISteamBackend {
public:
    virtual ~ISteamBackend() = default;

    // ---- Lifecycle ----
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void runCallbacks() = 0;           // SteamAPI_RunCallbacks — call every frame
    virtual bool isOverlayActive() const = 0;   // pause input when overlay is up

    // ---- Achievements ----
    virtual bool unlockAchievement(const char* id) = 0;
    virtual bool isAchievementUnlocked(const char* id) const = 0;
    virtual bool resetAchievement(const char* id) = 0;         // for testing
    virtual bool resetAllAchievements() = 0;

    // ---- Stats ----
    virtual bool setStatInt(const char* name, int32_t value) = 0;
    virtual int32_t getStatInt(const char* name) const = 0;
    virtual bool setStatFloat(const char* name, float value) = 0;
    virtual float getStatFloat(const char* name) const = 0;
    virtual bool storeStats() = 0;              // flush to Steam servers

    // ---- Cloud Saves ----
    virtual bool cloudWrite(const char* fileName, const void* data, int32_t size) = 0;
    virtual int32_t cloudRead(const char* fileName, void* buffer, int32_t maxSize) = 0;
    virtual int32_t cloudFileSize(const char* fileName) const = 0;
    virtual bool cloudFileExists(const char* fileName) const = 0;
    virtual bool cloudDelete(const char* fileName) = 0;
    virtual int32_t cloudQuotaTotal() const = 0;     // total bytes available
    virtual int32_t cloudQuotaUsed() const = 0;      // bytes currently used

    virtual const char* name() const = 0;
};

} // namespace Caesura
