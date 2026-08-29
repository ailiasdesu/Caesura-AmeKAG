#include "ShaderCache.h"
#include "../debug/api/DebugLog.h"   // P1-6: api header instead of concrete DebugManager.h
#include <cstdio>
#include <algorithm>
#include <vector>   // t90: precompileCommon registry-snapshot iteration

namespace Caesura {

// t90: the canonical non-palette key (Normal blend mode, no palette) -- the
// deterministic alias target for unregistered legal-mode lookups.
static CompositeShaderKey NormalKey() {
    CompositeShaderKey k;
    k.blendMode  = static_cast<int>(BlendMode::Normal);
    k.usePalette = false;
    return k;
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

CompositeShaderCache& CompositeShaderCache::instance() {
    static CompositeShaderCache sc;
    return sc;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CompositeShaderCache::init() {
    if (m_initialized) return;

    m_initialized = true;
    printf("[ShaderCache] Initialized (max %zu). Programs registered by IRenderDevice.\n",
           MAX_ENTRIES);
}

void CompositeShaderCache::shutdown() {
    if (!m_initialized) return;

    for (auto& pair : m_cache) {
        if (bgfx::isValid(pair.second.program)) {
            bgfx::destroy(pair.second.program);
        }
    }
    m_cache.clear();
    m_lruList.clear();
    m_initialized = false;
    printf("[ShaderCache] Shutdown complete.\n");
}

// ---------------------------------------------------------------------------
// Registration -- called by IRenderDevice after createProgram
// ---------------------------------------------------------------------------

void CompositeShaderCache::registerProgram(const CompositeShaderKey& key, bgfx::ProgramHandle program) {
    if (!bgfx::isValid(program)) {
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[ShaderCache] registerProgram: invalid program for blend=%d palette=%d",
                  key.blendMode, (int)key.usePalette);
        return;
    }

    if (m_cache.size() >= MAX_ENTRIES) {
        evictOne();
    }

    m_lruList.push_front(key);
    CacheEntry entry;
    entry.program = program;
    entry.key     = key;
    entry.lruIt   = m_lruList.begin();
    m_cache[key]  = entry;
}

// ---------------------------------------------------------------------------
// LRU eviction
// ---------------------------------------------------------------------------

void CompositeShaderCache::evictOne() {
    if (m_lruList.empty()) return;

    // Back of list = least-recently-used
    CompositeShaderKey evictKey = m_lruList.back();
    m_lruList.pop_back();

    auto it = m_cache.find(evictKey);
    if (it != m_cache.end()) {
        if (bgfx::isValid(it->second.program)) {
            bgfx::destroy(it->second.program);
        }
        m_cache.erase(it);
    }
}

// ---------------------------------------------------------------------------
// Program lookup -- get or create
// ---------------------------------------------------------------------------

bgfx::ProgramHandle CompositeShaderCache::getProgram(const CompositeShaderKey& key) {
    // Touch: move key to front of LRU list
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        m_lruList.erase(it->second.lruIt);
        m_lruList.push_front(key);
        it->second.lruIt = m_lruList.begin();
        return it->second.program;
    }

    // Evict if full
    if (m_cache.size() >= MAX_ENTRIES) {
        evictOne();
    }

    // Palette not available in Alpha -- fall back to same blend mode without palette
    if (key.usePalette) {
        CompositeShaderKey fallbackKey = key;
        fallbackKey.usePalette = false;
        auto fb = m_cache.find(fallbackKey);
        if (fb != m_cache.end()) {
            printf("[ShaderCache] palette not available, falling back to blend=%d without palette\n",
                   key.blendMode);
            m_lruList.erase(fb->second.lruIt);
            m_lruList.push_front(fallbackKey);
            fb->second.lruIt = m_lruList.begin();
            return fb->second.program;
        }
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[ShaderCache] palette requested but no fallback for blend=%d",
                  key.blendMode);
        return BGFX_INVALID_HANDLE;
    }

    // Compile new variant (should not reach here if registered by BgfxRenderDevice)
    bgfx::ProgramHandle prog = compileVariant(key);
    if (!bgfx::isValid(prog)) {
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[ShaderCache] Failed to compile variant: blend=%d palette=%d",
                  key.blendMode, (int)key.usePalette);
        return BGFX_INVALID_HANDLE;
    }

    // Insert into cache
    m_lruList.push_front(key);
    CacheEntry entry;
    entry.program = prog;
    entry.key     = key;
    entry.lruIt   = m_lruList.begin();
    m_cache[key]  = entry;

    return prog;
}

// ---------------------------------------------------------------------------
// Precompile the 10 Alpha minimum combinations
// ---------------------------------------------------------------------------

void CompositeShaderCache::precompileCommon() {
    // t90: precompile what is ACTUALLY registered (same-source list) instead of
    // a hardcoded kCommonModes array -- the old hardcoded list ({0..5,16,10,11,9})
    // drifted from the engine's registration subset ({0..9}) and produced the
    // round-3 'unregistered variant blend=10/11' [ERROR] noise. Iterating the
    // registry kills the drift class: the alias branch stays as a defensive
    // fallback for hypothetical post-init legal-mode lookups.
    // Snapshot first: getProgram touches the LRU list (mutates the container
    // we are iterating -- a straight range-for would be undefined behavior).
    std::vector<CompositeShaderKey> keys;
    keys.reserve(m_cache.size());
    for (const auto& pair : m_cache) keys.push_back(pair.first);
    for (const auto& key : keys) {
        if (!key.usePalette) getProgram(key);
    }
}

// ---------------------------------------------------------------------------
// Internal: compile a shader variant
// ---------------------------------------------------------------------------
// In a full implementation, this would use bgfx::createProgram from
// pre-compiled shader binaries (via EmbeddedShaders). For the current
// build, we use the same embedded blend program for all blend modes
// since the mode is passed as a uniform (u_blendParams.w). Palette
// variants would chain a second pass or use a combined program.
//
// IMPORTANT: bgfx does NOT support real-time GLSL compilation at runtime.
// This function assumes the shader binaries are pre-compiled and loaded
// from EmbeddedShaders or from disk. The variant key determines which
// pre-compiled binary to select from the pool.
//
// For now, we return a null handle that the caller can fall back from.
// The actual program creation is done by the render device via
// EmbeddedShaders / file-load path and injected here.

bgfx::ProgramHandle CompositeShaderCache::compileVariant(const CompositeShaderKey& key) {
    // t86: ONE program serves every LEGAL blend mode (fs_blend.sc design;
    // the mode is passed as a uniform, u_blendParams.w, at draw time). A legal
    // non-palette key that was not individually registered (Difference 10 /
    // Exclusion 11 / Add 16 are in precompileCommon's ten-most-common list but
    // the engine registers a different subset) therefore resolves to the
    // SAME registered non-palette program -- expected aliasing, not an error.
    if (!key.usePalette
        && key.blendMode >= 0
        && key.blendMode < static_cast<int>(BlendMode::COUNT)) {
        // Deterministic target (t90): prefer the Normal key (the canonical
        // non-palette entry) so a legal-mode alias always resolves to the mode
        // program, never to an arbitrary first entry of an unordered scan.
        auto normal = m_cache.find(NormalKey());
        if (normal != m_cache.end()) {
            ++m_aliasedVariants;
            DEBUG_INFO(SubSys::Render, ErrCode::Ok,
                       "[ShaderCache] blend=%d uses the registered Normal "
                       "program (uniform-driven aliasing, deterministic target).",
                       key.blendMode);
            return normal->second.program;
        }
        for (const auto& pair : m_cache) {
            if (!pair.first.usePalette && pair.first.blendMode >= 0) {
                ++m_aliasedVariants;
                DEBUG_INFO(SubSys::Render, ErrCode::Ok,
                           "[ShaderCache] blend=%d uses the registered non-palette "
                           "program (uniform-driven aliasing).", key.blendMode);
                return pair.second.program;
            }
        }
    }

    // All Programs registered by IRenderDevice::initEmbeddedShaders()
    // and registered via registerProgram(). If we reach here, the program
    // was not registered -- fall back to Normal blend mode.
    DEBUG_ERR(SubSys::Render, ErrCode::Ok,
              "[ShaderCache] compileVariant: unregistered variant blend=%d palette=%d. "
              "Falling back to Normal.",
              key.blendMode, (int)key.usePalette);

    // Try Normal fallback
    CompositeShaderKey fallbackKey;
    fallbackKey.blendMode  = static_cast<int>(BlendMode::Normal);
    fallbackKey.usePalette = false;
    auto fb = m_cache.find(fallbackKey);
    if (fb != m_cache.end()) {
        return fb->second.program;
    }

    return BGFX_INVALID_HANDLE;
}

} // namespace Caesura
