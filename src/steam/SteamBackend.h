// SteamBackend -- real Steamworks SDK integration
// Requires Steamworks SDK 1.60+. Compiled only when CAESURA_HAS_STEAM is defined.
#pragma once
#include "api/ISteamBackend.h"
#include <chrono>      // steady_clock (wall-clock throttle)
#include <cstdint>   // fixed-width types (GCC strict)

namespace Caesura {

class SteamBackend : public ISteamBackend {
public:
    SteamBackend();
    ~SteamBackend() override;

    bool init() override;
    void shutdown() override;
    void runCallbacks() override;
    bool isOverlayActive() const override;

    bool unlockAchievement(const char* id) override;
    bool isAchievementUnlocked(const char* id) const override;
    bool resetAchievement(const char* id) override;
    bool resetAllAchievements() override;

    bool setStatInt(const char* name, int32_t value) override;
    int32_t getStatInt(const char* name) const override;
    bool setStatFloat(const char* name, float value) override;
    float getStatFloat(const char* name) const override;
    bool storeStats() override;

    bool cloudWrite(const char* fileName, const void* data, int32_t size) override;
    int32_t cloudRead(const char* fileName, void* buffer, int32_t maxSize) override;
    int32_t cloudFileSize(const char* fileName) const override;
    bool cloudFileExists(const char* fileName) const override;
    bool cloudDelete(const char* fileName) override;
    int32_t cloudQuotaTotal() const override;
    int32_t cloudQuotaUsed() const override;
    int32_t cloudFileCount() const override;
    const char* cloudFileNameAt(int32_t index) const override;

    const char* name() const override { return "Steam"; }

private:
    bool m_initialized = false;
    bool m_statsRequested = false;
    bool m_statsReceived  = false;
    bool m_overlayActive  = false;
    bool m_statsDirty     = false;
    std::chrono::steady_clock::time_point m_lastStoreStats;  // throttle (wall clock);
    // default-constructed = epoch -> first flush always passes
    mutable char m_cloudName[1024] = {};  // cloudFileNameAt scratch buffer (per-instance)

#ifdef CAESURA_HAS_STEAM
    // Steam callback listeners (dispatched by SteamAPI_RunCallbacks on the
    // owner thread): overlay activation state and user-stats availability.
    STEAM_CALLBACK(SteamBackend, OnUserStatsReceived, UserStatsReceived_t,
                   m_callbackUserStatsReceived);
    STEAM_CALLBACK(SteamBackend, OnGameOverlayActivated, GameOverlayActivated_t,
                   m_callbackOverlayActivated);
#endif
};

} // namespace Caesura
