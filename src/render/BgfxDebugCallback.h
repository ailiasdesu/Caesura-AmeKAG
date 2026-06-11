#pragma once

#include <bgfx/bgfx.h>
#include <bx/bx.h>
#include <cstdio>
#include <cstdarg>

// BgfxDebugCallback -- captures bgfx internal error/warning messages.
// Extracted from BgfxRenderDevice (U1).

class BgfxDebugCallback : public bgfx::CallbackI {
public:
    bool m_shuttingDown = false;

    void fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) override {
        BX_UNUSED(_code);
        if (!m_shuttingDown) {
            fprintf(stderr, "[bgfx WARN] %s(%d): %s\n", _filePath, (int)_line, _str);
        }
    }
    void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override {
        char buf[2048];
        vsnprintf(buf, sizeof(buf), _format, _argList);
        fprintf(stderr, "[bgfx] %s(%d): %s\n", _filePath, (int)_line, buf);
    }
    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char*, uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, const void*, uint32_t, bool) override {}
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
};

extern BgfxDebugCallback g_bgfxDebugCallback;
void setBgfxShuttingDown(bool shuttingDown);
