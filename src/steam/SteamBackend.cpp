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
    // User stats arrive asynchronously; unlock/stat calls before
    // UserStatsReceived_t may silently no-op, so request them up front.
    m_statsRequested = true;
    m_statsReceived  = false;
    SteamUserStats()->RequestCurrentStats();
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
    // Batch StoreStats: unlock/reset mark m_statsDirty and the network
    // round trip happens at most once per second here (never on the Lua
    // binding call path).
    if (m_statsDirty) {
        const double now = (double)clock() / CLOCKS_PER_SEC;
        if (now - m_lastStoreStats >= 1.0) {
            m_lastStoreStats = now;
            m_statsDirty = false;
            SteamUserStats()->StoreStats();
        }
    }
#endif
}

bool SteamBackend::isOverlayActive() const {
#ifdef CAESURA_HAS_STEAM
    // GameOverlayActivated_t callback tracks the ACTUAL overlay state;
    // IsOverlayEnabled() only reports feature availability (usually true),
    // so using it here would permanently pause game input.
    return m_overlayActive;
#else
    return false;
#endif
}

bool SteamBackend::unlockAchievement(const char* id) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !id) return false;
    if (!m_statsReceived) return false;  // stats not loaded yet; caller retries
    if (!SteamUserStats()->SetAchievement(id)) return false;
    m_statsDirty = true;
    return true;
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
    if (!m_statsReceived) return false;
    if (!SteamUserStats()->ClearAchievement(id)) return false;
    m_statsDirty = true;
    return true;
#else
    (void)id;
    return false;
#endif
}

bool SteamBackend::resetAllAchievements() {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized) return false;
    if (!m_statsReceived) return false;
    SteamUserStats()->ResetAllStats(true);
    m_statsDirty = true;
    return true;
#else
    return false;
#endif
}

bool SteamBackend::setStatInt(const char* name, int32_t value) {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || !name) return false;
    if (!SteamUserStats()->SetStat(name, value)) return false;
    m_statsDirty = true;  // P1-3: persist stats on the next runCallbacks flush
    return true;
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
    if (!SteamUserStats()->SetStat(name, value)) return false;
    m_statsDirty = true;  // P1-3: persist stats on the next runCallbacks flush
    return true;
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

int32_t SteamBackend::cloudFileCount() const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized) return 0;
    return SteamRemoteStorage()->GetFileCount();
#else
    return 0;
#endif
}

const char* SteamBackend::cloudFileNameAt(int32_t index) const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized || index < 0) return "";
    static char s_name[256];
    int32_t size = 0;
    const char* name = SteamRemoteStorage()->GetFileNameAndSize(index, &size);
    if (!name) return "";
    snprintf(s_name, sizeof(s_name), "%s", name);
    return s_name;
#else
    (void)index;
    return "";
#endif
}

#ifdef CAESURA_HAS_STEAM
void SteamBackend::OnUserStatsReceived(UserStatsReceived_t* pCallback) {
    if (!pCallback) return;
    m_statsReceived = pCallback->m_eResult == k_EResultOK;
}
void SteamBackend::OnGameOverlayActivated(GameOverlayActivated_t* pCallback) {
    if (!pCallback) return;
    m_overlayActive = pCallback->m_bActive != 0;
}
#endif

} // namespace Caesura
