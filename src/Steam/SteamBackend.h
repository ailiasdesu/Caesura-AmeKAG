// SteamBackend — real Steamworks SDK integration
// Requires Steamworks SDK 1.60+. Compiled only when CAESURA_HAS_STEAM is defined.
#pragma once
#include "ISteamBackend.h"

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

    const char* name() const override { return "Steam"; }

private:
    bool m_initialized = false;
    bool m_statsRequested = false;
};

} // namespace Caesura
