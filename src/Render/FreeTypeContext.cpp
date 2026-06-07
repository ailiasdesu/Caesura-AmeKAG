#include "FreeTypeContext.h"
#include <cstdio>

namespace Caesura {

FreeTypeContext& FreeTypeContext::instance() {
    static FreeTypeContext ctx;
    return ctx;
}

bool FreeTypeContext::init() {
    if (m_initialized) return true;

    FT_Error err = FT_Init_FreeType(&m_library);
    if (err) {
        fprintf(stderr, "[FreeTypeContext] FT_Init_FreeType failed: %d\n", (int)err);
        return false;
    }

    m_initialized = true;
    printf("[FreeTypeContext] Initialized (shared FT_Library)\n");
    return true;
}

void FreeTypeContext::shutdown() {
    if (!m_initialized) return;

    if (m_library) {
        FT_Done_FreeType(m_library);
        m_library = nullptr;
    }

    m_initialized = false;
    printf("[FreeTypeContext] Shutdown complete.\n");
}

} // namespace Caesura