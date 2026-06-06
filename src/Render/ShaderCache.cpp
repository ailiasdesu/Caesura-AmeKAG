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

    m_initialized = true;
    printf("[ShaderCache] Initialized (max %zu). Programs registered by BgfxRenderDevice.\\n",
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
// Registration -- called by BgfxRenderDevice after createProgram
// ---------------------------------------------------------------------------

void CompositeShaderCache::registerProgram(const CompositeShaderKey& key, bgfx::ProgramHandle program) {
    if (!bgfx::isValid(program)) {
        fprintf(stderr, "[ShaderCache] registerProgram: invalid program for blend=%d palette=%d\n",
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
        fprintf(stderr, "[ShaderCache] palette requested but no fallback for blend=%d\n",
                key.blendMode);
        return BGFX_INVALID_HANDLE;
    }

    // Compile new variant (should not reach here if registered by BgfxRenderDevice)
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
    // All programs are pre-compiled by BgfxRenderDevice::initEmbeddedShaders()
    // and registered via registerProgram(). If we reach here, the program
    // was not registered -- this is a configuration error.
    fprintf(stderr, "[ShaderCache] compileVariant: unregistered variant blend=%d palette=%d. "
            "Did BgfxRenderDevice forget to register it?\n",
            key.blendMode, (int)key.usePalette);
    (void)key;
    return BGFX_INVALID_HANDLE;
}

} // namespace Caesura