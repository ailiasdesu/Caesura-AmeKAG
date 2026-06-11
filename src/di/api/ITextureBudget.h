#pragma once
#include <cstdint>

namespace Caesura {

// ============================================================================
// ITextureBudget — pure virtual interface for texture memory budget
// ============================================================================
// TextureBudget implements this interface. BackendRegistry stores ITextureBudget*.

class ITextureBudget {
public:
    virtual ~ITextureBudget() = default;

    virtual void detect() = 0;
    virtual void setTier(int tier) = 0;
    virtual int getTier() const = 0;
    virtual uint32_t getBudgetMB() const = 0;
    virtual uint64_t getBudgetBytes() const = 0;
    virtual const char* getTierName() const = 0;
    virtual bool isAutoDetected() const = 0;
};

} // namespace Caesura
