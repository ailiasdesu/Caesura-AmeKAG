#pragma once
#include "api/IResourceGenerationTracker.h"

namespace Caesura {

// -- GenerationTracker -----------------------------------------------------
// Per-type monotonic generation counter.
// Call invalidate() on hot reload; check isCurrent() before use.

class GenerationTracker final : public IResourceGenerationTracker {
public:
    static constexpr int kMaxTypes = 8;

    uint32_t current(HandleType type) const override {
        int idx = static_cast<int>(type);
        return (idx >= 0 && idx < kMaxTypes) ? m_generations[idx] : 0;
    }

    // Increment generation for a type (call on hot reload)
    void invalidate(HandleType type) override {
        int idx = static_cast<int>(type);
        if (idx >= 0 && idx < kMaxTypes) {
            m_generations[idx]++;
        }
    }

    // Check if a handle's generation matches current
    bool isCurrent(const ResourceHandle& h) const override {
        return h.generation == current(h.type);
    }

    // Create a handle with the current generation stamp
    ResourceHandle makeHandle(HandleType type, uint32_t id) const override {
        return ResourceHandle{type, id, current(type)};
    }

private:
    uint32_t m_generations[kMaxTypes] = {};  // All zero-initialized
};

} // namespace Caesura
