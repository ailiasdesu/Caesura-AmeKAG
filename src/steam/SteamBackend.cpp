// SteamBackend implementation — requires Steamworks SDK
// Compiled only when CAESURA_HAS_STEAM is defined (via CMake option)
#include <chrono>      // steady_clock (wall-clock throttle)
#include <cstring>
#include <memory>      // make_unique (SteamCallbacks bridge)

// The Steamworks SDK is intentionally included only here: SteamBackend.h
// stays SDK-free (its callback members hide behind the SteamCallbacks bridge
// defined below), so includers of that header need no STEAM_INCLUDE_DIR.
#ifdef CAESURA_HAS_STEAM
#include <steam/steam_api.h>
#endif

#include "SteamBackend.h"

namespace Caesura {

#ifdef CAESURA_HAS_STEAM
// Opaque callback bridge (declared in SteamBackend.h). CCallbackManual
// default-constructs unregistered, so construction never touches the
// SteamAPI; registration is driven by init()/shutdown().
struct SteamBackend::SteamCallbacks {
    CCallbackManual<SteamBackend, UserStatsReceived_t> statsReceived;
    CCallbackManual<SteamBackend, GameOverlayActivated_t> overlay;
};
#endif

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
    // Manual callback registration: only valid once the SteamAPI is live.
    // CCallback::Register unregisters first when already registered, so a
    // re-init after shutdown stays correct; the flag gates shutdown().
    if (!m_callbacks) m_callbacks = std::make_unique<SteamCallbacks>();
    m_callbacks->statsReceived.Register(this, &SteamBackend::OnUserStatsReceived);
    m_callbacks->overlay.Register(this, &SteamBackend::OnGameOverlayActivated);
    m_callbacksRegistered = true;
    // No explicit RequestCurrentStats(): modern Steamworks (1.60+) removed
    // ISteamUserStats::RequestCurrentStats — user stats are fetched
    // automatically after SteamAPI_Init and arrive via UserStatsReceived_t.
    return true;
#else
    (void)m_initialized;
    return false;
#endif
}

void SteamBackend::shutdown() {
#ifdef CAESURA_HAS_STEAM
    // Unregister BEFORE SteamAPI_Shutdown: no queued callback may fire into a
    // torn-down client. (CCallbackImpl's destructor also auto-unregisters
    // anything still flagged registered — this explicit path is the norm.)
    if (m_callbacksRegistered) {
        if (m_callbacks) {
            m_callbacks->statsReceived.Unregister();
            m_callbacks->overlay.Unregister();
        }
        m_callbacksRegistered = false;
    }
    m_callbacks.reset();
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
        const auto now = std::chrono::steady_clock::now();
        if (now - m_lastStoreStats >= std::chrono::seconds(1)) {
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
    // Do not call StoreStats() directly here (scripts could spam it): mark dirty
    // and let runCallbacks flush on the next tick (throttled to 1/sec).
    m_statsDirty = true;
    return true;  // accepted; actual flush happens on the next runCallbacks
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
    // SDK 1.65 GetQuota reports through uint64*; saturate to int32_max for
    // the interface's int32_t contract.
    uint64 total = 0;
    SteamRemoteStorage()->GetQuota(&total, nullptr);
    return total > static_cast<uint64>(INT32_MAX) ? INT32_MAX
                                                  : static_cast<int32_t>(total);
#else
    return 0;
#endif
}

int32_t SteamBackend::cloudQuotaUsed() const {
#ifdef CAESURA_HAS_STEAM
    if (!m_initialized) return 0;
    uint64 used = 0;
    SteamRemoteStorage()->GetQuota(nullptr, &used);
    return used > static_cast<uint64>(INT32_MAX) ? INT32_MAX
                                                 : static_cast<int32_t>(used);
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
    int32_t size = 0;
    const char* name = SteamRemoteStorage()->GetFileNameAndSize(index, &size);
    if (!name) return "";
    snprintf(m_cloudName, sizeof(m_cloudName), "%s", name);
    return m_cloudName;
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
