#pragma once
#include <cstdint>

namespace Caesura {

// Opaque texture identifier — replaces bgfx::TextureHandle in public render/api/ interfaces.
// Stores the bgfx TextureHandle.idx (uint16_t) value in uint32_t.
// Consumers that need a bgfx handle construct it: bgfx::TextureHandle{uint16_t(id)}.
struct TextureId {
    uint32_t id = 0;
    bool valid() const { return id != 0; }
    bool operator==(const TextureId& o) const { return id == o.id; }
    bool operator!=(const TextureId& o) const { return id != o.id; }
};

// Opaque program identifier — replaces bgfx::ProgramHandle in public render/api/ interfaces.
// Stores the bgfx ProgramHandle.idx (uint16_t) value in uint32_t.
struct ProgramId {
    uint32_t id = 0;
    bool valid() const { return id != 0; }
    bool operator==(const ProgramId& o) const { return id == o.id; }
    bool operator!=(const ProgramId& o) const { return id != o.id; }
};

} // namespace Caesura
