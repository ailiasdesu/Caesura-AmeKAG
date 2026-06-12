// NullSteamBackend ¡ª no-op implementation of ISteamBackend
// Used when CAESURA_ENABLE_STEAM=OFF or Steamworks SDK not available
#pragma once
#include "api/ISteamBackend.h"

namespace Caesura {

class NullSteamBackend : public ISteamBackend {
public:
    bool init() override { return false; }
    void shutdown() override {}
    void runCallbacks() override {}
    bool isOverlayActive() const override { return false; }

    bool unlockAchievement(const char*) override { return false; }
    bool isAchievementUnlocked(const char*) const override { return false; }
    bool resetAchievement(const char*) override { return false; }
    bool resetAllAchievements() override { return false; }

    bool setStatInt(const char*, int32_t) override { return false; }
    int32_t getStatInt(const char*) const override { return 0; }
    bool setStatFloat(const char*, float) override { return false; }
    float getStatFloat(const char*) const override { return 0.0f; }
    bool storeStats() override { return false; }

    bool cloudWrite(const char*, const void*, int32_t) override { return false; }
    int32_t cloudRead(const char*, void*, int32_t) override { return 0; }
    int32_t cloudFileSize(const char*) const override { return 0; }
    bool cloudFileExists(const char*) const override { return false; }
    bool cloudDelete(const char*) override { return false; }
    int32_t cloudQuotaTotal() const override { return 0; }
    int32_t cloudQuotaUsed() const override { return 0; }

    const char* name() const override { return "NullSteam"; }
};

} // namespace Caesura
