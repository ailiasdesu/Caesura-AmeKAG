#include "BgfxDebugCallback.h"
#include "../../external/stb/stb_image_write.h"
#include <vector>

BgfxDebugCallback g_bgfxDebugCallback;

std::atomic<bool> BgfxDebugCallback::s_deviceLost{false};

void setBgfxShuttingDown(bool shuttingDown) {
    g_bgfxDebugCallback.m_shuttingDown = shuttingDown;
}

// screenShot — bgfx delivers the frame readback after the next frame
// advance; write it as PNG so requestScreenshot() callers (editor frame
// capture, demo export) get a real file. RGBA8 is the only format bgfx
// produces for requestScreenShot on the supported backends.
void BgfxDebugCallback::screenShot(const char* _name, uint32_t _width,
                                   uint32_t _height, uint32_t _depth,
                                   bgfx::TextureFormat::Enum _format,
                                   const void* _data, uint32_t _size,
                                   bool _flipY) {
    if (!_data || _width == 0 || _height == 0) {
        fprintf(stderr, "[bgfx] screenshot skipped (no data)\n");
        return;
    }
    if (_format != bgfx::TextureFormat::RGBA8
        && _format != bgfx::TextureFormat::BGRA8) {
        fprintf(stderr, "[bgfx] screenshot format %d unsupported, skipping\n",
                (int)_format);
        return;
    }
    BX_UNUSED(_depth, _size);
    // D3D11 delivers BGRA8 backbuffers; stb PNGs want RGBA -- swap R/B.
    // The swap buffer is sized for a single frame (1280x720x4 ≈ 3.7MB).
    static std::vector<uint8_t> swapBuf;
    const size_t px = (size_t)_width * (size_t)_height;
    const uint8_t* src = static_cast<const uint8_t*>(_data);
    if (_format == bgfx::TextureFormat::BGRA8) {
        if (swapBuf.size() < px * 4) swapBuf.resize(px * 4);
        for (size_t i = 0; i < px; ++i) {
            swapBuf[i * 4 + 0] = src[i * 4 + 2];
            swapBuf[i * 4 + 1] = src[i * 4 + 1];
            swapBuf[i * 4 + 2] = src[i * 4 + 0];
            swapBuf[i * 4 + 3] = src[i * 4 + 3];
        }
        src = swapBuf.data();
    }
    stbi_flip_vertically_on_write(_flipY ? 1 : 0);
    const int ok = stbi_write_png(_name, (int)_width, (int)_height, 4,
                                  src, (int)_width * 4);
    stbi_flip_vertically_on_write(0);
    if (!ok) {
        fprintf(stderr, "[bgfx] screenshot write failed: %s\n", _name);
    }
}
