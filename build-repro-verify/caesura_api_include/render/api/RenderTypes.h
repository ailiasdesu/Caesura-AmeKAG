#pragma once
#include <cstdint>

namespace Caesura {

constexpr uint16_t INVALID_RENDER_HANDLE_INDEX = UINT16_MAX;

struct RenderTextureHandle {
    uint16_t idx = INVALID_RENDER_HANDLE_INDEX;
    bool isValid() const { return idx != INVALID_RENDER_HANDLE_INDEX; }
    bool valid() const { return isValid(); }
    explicit operator bool() const { return isValid(); }
    bool operator==(const RenderTextureHandle& o) const { return idx == o.idx; }
    bool operator!=(const RenderTextureHandle& o) const { return idx != o.idx; }
};

struct RenderProgramHandle {
    uint16_t idx = INVALID_RENDER_HANDLE_INDEX;
    bool isValid() const { return idx != INVALID_RENDER_HANDLE_INDEX; }
    bool valid() const { return isValid(); }
    explicit operator bool() const { return isValid(); }
    bool operator==(const RenderProgramHandle& o) const { return idx == o.idx; }
    bool operator!=(const RenderProgramHandle& o) const { return idx != o.idx; }
};

struct RenderUniformHandle {
    uint16_t idx = INVALID_RENDER_HANDLE_INDEX;
    bool isValid() const { return idx != INVALID_RENDER_HANDLE_INDEX; }
    bool valid() const { return isValid(); }
    explicit operator bool() const { return isValid(); }
    bool operator==(const RenderUniformHandle& o) const { return idx == o.idx; }
    bool operator!=(const RenderUniformHandle& o) const { return idx != o.idx; }
};

// Opaque texture identifier used by older call sites.
struct TextureId {
    uint32_t id = 0;
    bool valid() const { return id != 0; }
    bool operator==(const TextureId& o) const { return id == o.id; }
    bool operator!=(const TextureId& o) const { return id != o.id; }
};

// Opaque program identifier used by older call sites.
struct ProgramId {
    uint32_t id = 0;
    bool valid() const { return id != 0; }
    bool operator==(const ProgramId& o) const { return id == o.id; }
    bool operator!=(const ProgramId& o) const { return id != o.id; }
};

} // namespace Caesura
