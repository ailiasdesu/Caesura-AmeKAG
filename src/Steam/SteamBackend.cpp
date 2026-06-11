// SteamBackend implementation — requires Steamworks SDK
// Compiled only when CAESURA_HAS_STEAM is defined (via CMake option)
#include "SteamBackend.h"
#include <cstring>

#ifdef CAESURA_HAS_STEAM
#include <steam/steam_api.h>
#endif

namespace Caesura {

SteamBackend::SteamBackend() = default;
SteamBackend::~SteamBackend() { shutdown(); }

bool SteamBackend::init() {
#ifdef CAESURA_HAS_STEAM
    if (m_initialized) return true;
    if (!SteamAPI_Init()) return false;
    m_initialized = true;
    return true;
#else
    (void)m_initialized;
    return false;
#endif
}

void SteamBackend::shutdown() {
#ifdef CAESURA_HAS_STEAM
    if (m_initialized) {
        SteamAPI_Shutdown();
        m_initialized = false;
    }
#endif
}

void SteamBackend::runCallbacks() {
#ifdef CAESURA_HAS_STEAM
    SteamAPI_RunCallbacks();
#endif
}

bool SteamBackend::isOverlayActive() const {
#ifdef CAESURA_HAS_STEAM
    return SteamUtils() && SteamUtils()->IsOverlayEnabled();
#else
    return false;
#endif
}

bool SteamBackend::unlockAchievement(const char* id) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !id) return false;
    SteamUserStats()->SetAchievement(id);
    return SteamUserStats()->StoreStats();
#else
    (void)id;
    return false;
#endif
}

bool SteamBackend::isAchievementUnlocked(const char* id) const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !id) return false;
    bool unlocked = false;
    SteamUserStats()->GetAchievement(id, &unlocked);
    return unlocked;
#else
    (void)id;
    return false;
#endif
}

bool SteamBackend::resetAchievement(const char* id) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !id) return false;
    SteamUserStats()->ClearAchievement(id);
    return SteamUserStats()->StoreStats();
#else
    (void)id;
    return false;
#endif
}

bool SteamBackend::resetAllAchievements() {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized) return false;
    SteamUserStats()->ResetAllStats(true);
    return SteamUserStats()->StoreStats();
#else
    return false;
#endif
}

bool SteamBackend::setStatInt(const char* name, int32_t value) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !name) return false;
    return SteamUserStats()->SetStat(name, value);
#else
    (void)name; (void)value;
    return false;
#endif
}

int32_t SteamBackend::getStatInt(const char* name) const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !name) return 0;
    int32_t val = 0;
    SteamUserStats()->GetStat(name, &val);
    return val;
#else
    (void)name;
    return 0;
#endif
}

bool SteamBackend::setStatFloat(const char* name, float value) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !name) return false;
    return SteamUserStats()->SetStat(name, value);
#else
    (void)name; (void)value;
    return false;
#endif
}

float SteamBackend::getStatFloat(const char* name) const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !name) return 0.0f;
    float val = 0.0f;
    SteamUserStats()->GetStat(name, &val);
    return val;
#else
    (void)name;
    return 0.0f;
#endif
}

bool SteamBackend::storeStats() {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized) return false;
    return SteamUserStats()->StoreStats();
#else
    return false;
#endif
}

bool SteamBackend::cloudWrite(const char* fileName, const void* data, int32_t size) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !fileName || !data || size <= 0) return false;
    return SteamRemoteStorage()->FileWrite(fileName, data, size);
#else
    (void)fileName; (void)data; (void)size;
    return false;
#endif
}

int32_t SteamBackend::cloudRead(const char* fileName, void* buffer, int32_t maxSize) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !fileName || !buffer) return 0;
    return SteamRemoteStorage()->FileRead(fileName, buffer, maxSize);
#else
    (void)fileName; (void)buffer; (void)maxSize;
    return 0;
#endif
}

int32_t SteamBackend::cloudFileSize(const char* fileName) const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !fileName) return 0;
    return SteamRemoteStorage()->GetFileSize(fileName);
#else
    (void)fileName;
    return 0;
#endif
}

bool SteamBackend::cloudFileExists(const char* fileName) const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !fileName) return false;
    return SteamRemoteStorage()->FileExists(fileName);
#else
    (void)fileName;
    return false;
#endif
}

bool SteamBackend::cloudDelete(const char* fileName) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !fileName) return false;
    return SteamRemoteStorage()->FileDelete(fileName);
#else
    (void)fileName;
    return false;
#endif
}

int32_t SteamBackend::cloudQuotaTotal() const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized) return 0;
    int32_t total = 0;
    SteamRemoteStorage()->GetQuota(&total, nullptr);
    return total;
#else
    return 0;
#endif
}

int32_t SteamBackend::cloudQuotaUsed() const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized) return 0;
    int32_t used = 0;
    SteamRemoteStorage()->GetQuota(nullptr, &used);
    return used;
#else
    return 0;
#endif
}

} // namespace Caesura
