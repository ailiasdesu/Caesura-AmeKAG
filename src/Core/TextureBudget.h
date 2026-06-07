#pragma once
#include <cstdint>

namespace Caesura {

// ---------------------------------------------------------------------------
// TextureBudget — 6-tier adaptive texture memory budget
// Spec [10.2.65]: auto-detect from system RAM, developer-overridable.
// Called once at Engine::init(), read-only after.
// ---------------------------------------------------------------------------
// Tier mapping:
//   0: 128 MB  (RAM < 4 GB)
//   1: 256 MB  (RAM 4-6 GB, integrated GPU default)
//   2: 512 MB  (RAM 6-8 GB, entry discrete GPU)
//   3: 1 GB    (RAM 8-16 GB, mainstream)
//   4: 2 GB    (RAM > 16 GB, high-end)
//   5: 4 GB    (developer override)

class TextureBudget {
public:
    static TextureBudget& instance();

    TextureBudget(const TextureBudget&) = delete;
    TextureBudget& operator=(const TextureBudget&) = delete;

    // Detect system RAM and select budget tier (call once at init)
    void detect();

    // Override tier manually (from config.lua or dev tools)
    // 0-5: specific tier, -1: reset to auto-detect
    void setTier(int tier);

    // Current tier (0-5)
    int getTier() const { return m_tier; }

    // Budget in megabytes for the current tier
    uint32_t getBudgetMB() const;

    // Budget in bytes
    uint64_t getBudgetBytes() const { return uint64_t(getBudgetMB()) * 1024 * 1024; }

    // Human-readable tier name
    const char* getTierName() const;

    // Whether tier was auto-detected (vs. developer override)
    bool isAutoDetected() const { return m_autoDetected; }

private:
    TextureBudget() = default;

    int m_tier = 1;              // default: 256 MB (HD 620 tier)
    bool m_autoDetected = true;
    uint64_t m_systemRAMBytes = 0;
};

} // namespace Caesura
