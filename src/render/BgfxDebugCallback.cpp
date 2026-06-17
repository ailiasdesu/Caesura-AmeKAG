#include "BgfxDebugCallback.h"

BgfxDebugCallback g_bgfxDebugCallback;

std::atomic<bool> BgfxDebugCallback::s_deviceLost{false};

void setBgfxShuttingDown(bool shuttingDown) {
    g_bgfxDebugCallback.m_shuttingDown = shuttingDown;
}
