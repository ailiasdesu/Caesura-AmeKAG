#pragma once

#include <cstdint>

namespace Caesura {

enum class HandleType : uint8_t {
    TEXTURE = 0,
    AUDIO = 1,
    VIEWPORT = 2,
    RTT = 3,
    SHADER = 4,
    TRANSITION = 5,
    FONT_ATLAS = 6,
    VIDEO = 7,
};

inline const char* handleTypeName(HandleType type) {
    switch (type) {
        case HandleType::TEXTURE: return "TEXTURE";
        case HandleType::AUDIO: return "AUDIO";
        case HandleType::VIEWPORT: return "VIEWPORT";
        case HandleType::RTT: return "RTT";
        case HandleType::SHADER: return "SHADER";
        case HandleType::TRANSITION: return "TRANSITION";
        case HandleType::FONT_ATLAS: return "FONT_ATLAS";
        case HandleType::VIDEO: return "VIDEO";
    }
    return "UNKNOWN";
}

struct ResourceHandle {
    HandleType type = HandleType::TEXTURE;
    uint32_t id = 0;
    uint32_t generation = 0;

    explicit operator bool() const { return id != 0; }
    bool operator==(const ResourceHandle& other) const {
        return type == other.type && id == other.id && generation == other.generation;
    }
    bool operator!=(const ResourceHandle& other) const { return !(*this == other); }
};

class IResourceGenerationTracker {
public:
    virtual ~IResourceGenerationTracker() = default;

    virtual uint32_t current(HandleType type) const = 0;
    virtual void invalidate(HandleType type) = 0;
    virtual bool isCurrent(const ResourceHandle& handle) const = 0;
    virtual ResourceHandle makeHandle(HandleType type, uint32_t id) const = 0;
};

} // namespace Caesura
