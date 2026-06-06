#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <list>
#include <utility>

namespace Caesura {

// ---------------------------------------------------------------------------
// BlendMode — 28 Photoshop-compatible blend modes (matches fs_blend.sc)
// ---------------------------------------------------------------------------
enum class BlendMode : int {
    Normal       = 0,
    Multiply     = 1,
    Screen       = 2,
    Overlay      = 3,
    Darken       = 4,
    Lighten      = 5,
    ColorDodge   = 6,
    ColorBurn    = 7,
    HardLight    = 8,
    SoftLight    = 9,
    Difference   = 10,
    Exclusion    = 11,
    Hue          = 12,
    Saturation   = 13,
    Color        = 14,
    Luminosity   = 15,
    Add          = 16,
    Subtract     = 17,
    Divide       = 18,
    LinearBurn   = 19,
    LinearDodge  = 20,
    VividLight   = 21,
    LinearLight  = 22,
    PinLight     = 23,
    HardMix      = 24,
    Invert       = 25,
    Alpha        = 26,
    Erase        = 27,
    COUNT        = 28
};

// ---------------------------------------------------------------------------
// CompositeShaderKey — unique key for a shader variant
// ---------------------------------------------------------------------------
// Combines blend mode + whether palette LUT post-processing is required.
// Future extensions: can add more flags (dithering, tone-mapping, etc.)

struct CompositeShaderKey {
    int  blendMode    = 0;
    bool usePalette   = false;

    bool operator==(const CompositeShaderKey& o) const {
        return blendMode == o.blendMode && usePalette == o.usePalette;
    }
};

} // namespace Caesura

// std::hash specialization for CompositeShaderKey
namespace std {
    template<> struct hash<Caesura::CompositeShaderKey> {
        size_t operator()(const Caesura::CompositeShaderKey& k) const noexcept {
            // blendMode is 0-27 (5 bits) + palette flag (1 bit) = fits in uint32_t
            return static_cast<size_t>((static_cast<uint32_t>(k.blendMode) & 0xFF)
                                       | (k.usePalette ? 0x100u : 0u));
        }
    };
}

namespace Caesura {

// ---------------------------------------------------------------------------
// CompositeShaderCache — LRU cache of bgfx::ProgramHandle variants
// ---------------------------------------------------------------------------
// Each variant is keyed by CompositeShaderKey: {blend_mode, use_palette}.
// Max 64 entries; evicts least-recently-used when full.
// Precompiles the 10 most common combinations at init.
//
// Architecture invariant: bgfx::createProgram / bgfx::destroy must be called
// on the main thread only.  ShaderCache does NOT manage shader source
// compilation — it assumes pre-compiled shader binaries are loaded via
// EmbeddedShaders or a shader binary loader.
//
// Thread safety: not thread-safe by design — must be used from the main
// thread (bgfx requirement).

class CompositeShaderCache {
public:
    static CompositeShaderCache& instance();

    CompositeShaderCache(const CompositeShaderCache&) = delete;
    CompositeShaderCache& operator=(const CompositeShaderCache&) = delete;

    // -- Lifecycle ---------------------------------------------------------
    void init();
    void shutdown();

    // -- Lookup / create ---------------------------------------------------
    // Returns a bgfx program for the given key; creates + caches if missing.
    bgfx::ProgramHandle getProgram(const CompositeShaderKey& key);

    // -- Registration -------------------------------------------------------
    // Called by BgfxRenderDevice after creating programs via buildBgfxShader.
    // Registers a pre-created program for the given key without recompiling.
    void registerProgram(const CompositeShaderKey& key, bgfx::ProgramHandle program);

    // -- Precompile helpers ------------------------------------------------
    // Pre-loads the 10 most common combinations (Normal, Multiply, Screen,
    // Overlay, Darken, Lighten, Add, Subtract, Alpha, Erase) all without palette.
    void precompileCommon();

    // -- Stats -------------------------------------------------------------
    size_t size() const    { return m_cache.size(); }
    size_t maxSize() const { return MAX_ENTRIES; }

private:
    CompositeShaderCache() = default;

    // -- Internal helpers --------------------------------------------------
    bgfx::ProgramHandle compileVariant(const CompositeShaderKey& key);
    void evictOne();

    // -- LRU tracking ------------------------------------------------------
    static constexpr size_t MAX_ENTRIES = 64;

    struct CacheEntry {
        bgfx::ProgramHandle  program = BGFX_INVALID_HANDLE;
        CompositeShaderKey   key;
        // Iterator into m_lruList for O(1) touch
        std::list<CompositeShaderKey>::iterator lruIt;
    };

    std::unordered_map<CompositeShaderKey, CacheEntry> m_cache;
    std::list<CompositeShaderKey> m_lruList;   // front = most-recently-used

    bool m_initialized = false;
};

} // namespace Caesura