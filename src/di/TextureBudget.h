#pragma once
#include "api/ITextureBudget.h"
#include <cstdint>

namespace Caesura {

// ============================================================================
// TextureBudget -- implements ITextureBudget
// ============================================================================

class TextureBudget : public ITextureBudget {
public:
    static TextureBudget& instance();

    TextureBudget(const TextureBudget&) = delete;
    TextureBudget& operator=(const TextureBudget&) = delete;

    void detect() override;
    void setTier(int tier) override;
    int getTier() const override { return m_tier; }
    uint32_t getBudgetMB() const override;
    uint64_t getBudgetBytes() const override { return uint64_t(getBudgetMB()) * 1024 * 1024; }
    const char* getTierName() const override;
    bool isAutoDetected() const override { return m_autoDetected; }

private:
    TextureBudget() = default;

    int m_tier = 1;
    bool m_autoDetected = true;
    uint64_t m_systemRAMBytes = 0;
};

} // namespace Caesura