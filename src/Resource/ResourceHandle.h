#pragma once
#include <cstdint>
#include <string>

namespace Caesura {

// -- HandleType ------------------------------------------------------------
// Categorizes every engine resource exposed to Lua through the backend API.
// Lua never touches raw bgfx/SoLoud handles -- all resources go through
// ResourceHandle with type + id + generation triple validation.

enum class HandleType : uint8_t {
    TEXTURE    = 0,  // bgfx::TextureHandle
    AUDIO      = 1,  // SoLoud voice handle
    VIEWPORT   = 2,  // bgfx::FrameBufferHandle
    RTT        = 3,  // bgfx::TextureHandle (offscreen render target)
    SHADER     = 4,  // bgfx::ProgramHandle
    TRANSITION = 5,  // Transition state
    FONT_ATLAS = 6,  // TextureArray layer
    VIDEO      = 7,  // VideoPlayer state
};

inline const char* handleTypeName(HandleType t) {
    switch (t) {
        case HandleType::TEXTURE:    return "TEXTURE";
        case HandleType::AUDIO:      return "AUDIO";
        case HandleType::VIEWPORT:   return "VIEWPORT";
        case HandleType::RTT:        return "RTT";
        case HandleType::SHADER:     return "SHADER";
        case HandleType::TRANSITION: return "TRANSITION";
        case HandleType::FONT_ATLAS: return "FONT_ATLAS";
        case HandleType::VIDEO:      return "VIDEO";
    }
    return "UNKNOWN";
}

// -- ResourceHandle --------------------------------------------------------
// Unified resource handle for all Lua-exposed engine resources.
//
// Triple validation contract:
//   id != 0  -- handle was ever allocated
//   generation == current -- resource not invalidated by hot reload
//
// On hot reload, the generation counter for a resource type is incremented.
// All old handles with stale generation automatically fail isValid().
// Lua can call backend.is_valid(type, id) to check before use.

struct ResourceHandle {
    HandleType type       = HandleType::TEXTURE;
    uint32_t   id         = 0;   // Unique within type
    uint32_t   generation = 0;   // Monotonic counter; invalidated on reload

    explicit operator bool() const { return id != 0; }
    bool operator==(const ResourceHandle& o) const {
        return type == o.type && id == o.id && generation == o.generation;
    }
    bool operator!=(const ResourceHandle& o) const { return !(*this == o); }
};

// -- GenerationTracker -----------------------------------------------------
// Per-type monotonic generation counter.
// Call invalidate() on hot reload; check isCurrent() before use.

class GenerationTracker {
public:
    static constexpr int kMaxTypes = 8;

    uint32_t current(HandleType type) const {
        int idx = static_cast<int>(type);
        return (idx >= 0 && idx < kMaxTypes) ? m_generations[idx] : 0;
    }

    // Increment generation for a type (call on hot reload)
    void invalidate(HandleType type) {
        int idx = static_cast<int>(type);
        if (idx >= 0 && idx < kMaxTypes) {
            m_generations[idx]++;
        }
    }

    // Check if a handle's generation matches current
    bool isCurrent(const ResourceHandle& h) const {
        return h.generation == current(h.type);
    }

    // Create a handle with the current generation stamp
    ResourceHandle makeHandle(HandleType type, uint32_t id) const {
        return ResourceHandle{type, id, current(type)};
    }

private:
    uint32_t m_generations[kMaxTypes] = {};  // All zero-initialized
};

} // namespace Caesura
