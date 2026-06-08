#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Caesura {

// CPU-only image decode result. Safe to produce on worker threads (no bgfx).
struct DecodedImage {
    std::vector<uint8_t> rgba;
    uint16_t width  = 0;
    uint16_t height = 0;
    bool ok = false;
};

// Thread-safe image decoder for worker threads (bimg + stb fallback, no GPU).
namespace ImageDecoder {
    DecodedImage decode(const uint8_t* data, size_t size);
}

} // namespace Caesura
