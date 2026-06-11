#include "BgfxDebugCallback.h"

BgfxDebugCallback g_bgfxDebugCallback;

void setBgfxShuttingDown(bool shuttingDown) {
    g_bgfxDebugCallback.m_shuttingDown = shuttingDown;
}
