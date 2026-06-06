#include "ShaderCache.h"
#include <cstdio>
#include <algorithm>

namespace Caesura {

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

    precompileCommon();

    m_initialized = true;
    printf("[ShaderCache] Initialized with %zu precompiled variants (max %zu).\n",
           m_cache.size(), MAX_ENTRIES);
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
// Program lookup — get or create
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

    // Compile new variant
    bgfx::ProgramHandle prog = compileVariant(key);
    if (!bgfx::isValid(prog)) {
        fprintf(stderr, "[ShaderCache] Failed to compile variant: blend=%d palette=%d\n",
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
    // Most common blend modes without palette — preloaded eagerly
    static const BlendMode kCommonModes[] = {
        BlendMode::Normal,
        BlendMode::Multiply,
        BlendMode::Screen,
        BlendMode::Overlay,
        BlendMode::Darken,
        BlendMode::Lighten,
        BlendMode::Add,
        BlendMode::Difference,
        BlendMode::Exclusion,
        BlendMode::SoftLight
    };

    for (auto mode : kCommonModes) {
        CompositeShaderKey key;
        key.blendMode  = static_cast<int>(mode);
        key.usePalette = false;
        getProgram(key);
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
    // In a full implementation, this selects the right pre-compiled program.
    // Currently all blend modes share one program (mode passed via uniform),
    // so we return the fallback. The render device is responsible for loading
    // the actual program and calling registerProgram().
    //
    // For palette + blend combos, a second pass program would be compiled
    // from fs_blend.sc + fs_palette.sc macros, or a multi-pass approach used.
    (void)key;  // suppress unused warning
    return BGFX_INVALID_HANDLE;
}

} // namespace Caesura