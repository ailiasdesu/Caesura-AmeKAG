#include "TextureBudget.h"
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Caesura {

static constexpr uint32_t kBudgetMB[] = { 128, 256, 512, 1024, 2048, 4096 };
static constexpr const char* kTierNames[] = {
    "128MB (Low)", "256MB (Entry)", "512MB (Mid)", "1GB (Mainstream)", "2GB (High)", "4GB (DevOverride)"
};

TextureBudget& TextureBudget::instance() {
    static TextureBudget tb;
    return tb;
}

void TextureBudget::detect() {
#ifdef _WIN32
    MEMORYSTATUSEX memStatus = {};
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        m_systemRAMBytes = memStatus.ullTotalPhys;
    } else {
        m_systemRAMBytes = 4ULL * 1024 * 1024 * 1024; // fallback: assume 4GB
    }
#else
    // Linux/macOS: read /proc/meminfo or sysctl
    m_systemRAMBytes = 4ULL * 1024 * 1024 * 1024;
#endif

    uint64_t ramGB = m_systemRAMBytes / (1024ULL * 1024 * 1024);

    if (ramGB < 4)       m_tier = 0;
    else if (ramGB < 6)  m_tier = 1;
    else if (ramGB < 8)  m_tier = 2;
    else if (ramGB <= 16) m_tier = 3;
    else                  m_tier = 4;

    m_autoDetected = true;

    printf("[TextureBudget] System RAM: %llu GB -> Tier %d (%s, %u MB)\n",
           (unsigned long long)ramGB, m_tier, kTierNames[m_tier], kBudgetMB[m_tier]);
}

void TextureBudget::setTier(int tier) {
    if (tier < 0) {
        detect();
        return;
    }
    if (tier > 5) tier = 5;
    m_tier = tier;
    m_autoDetected = false;
    printf("[TextureBudget] Developer override: Tier %d (%s, %u MB)\n",
           m_tier, kTierNames[m_tier], kBudgetMB[m_tier]);
}

uint32_t TextureBudget::getBudgetMB() const {
    return kBudgetMB[m_tier];
}

const char* TextureBudget::getTierName() const {
    return kTierNames[m_tier];
}

} // namespace Caesura
