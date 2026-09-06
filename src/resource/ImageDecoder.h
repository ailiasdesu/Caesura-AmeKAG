#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "api/IImageDecoder.h"

namespace Caesura {

// CPU-only image decode result. Safe to produce on worker threads (no bgfx).
class CpuImageDecoder final : public IImageDecoder {
public:
    DecodedImage decode(const uint8_t* bytes, size_t size,
                        size_t maxDecodedBytes) override;
};

// Thread-safe image decoder for worker threads (bimg + stb fallback, no GPU).
namespace ImageDecoder {
    DecodedImage decode(const uint8_t* data, size_t size,
                        size_t maxDecodedBytes = 1024ull * 1024ull * 1024ull);
}

} // namespace Caesura
