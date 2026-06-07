#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Caesura {

// Global shared FreeType instance -- ensures FontRenderer and TextRenderer
// share a single FT_Library rather than each initializing their own.
class FreeTypeContext {
public:
    static FreeTypeContext& instance();

    FreeTypeContext(const FreeTypeContext&) = delete;
    FreeTypeContext& operator=(const FreeTypeContext&) = delete;

    bool init();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    FT_Library getLibrary() const { return m_library; }

private:
    FreeTypeContext() = default;

    FT_Library m_library = nullptr;
    bool m_initialized = false;
};

} // namespace Caesura