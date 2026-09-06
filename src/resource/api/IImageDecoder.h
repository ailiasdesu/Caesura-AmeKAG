#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Caesura {

struct DecodedImage {
    std::vector<uint8_t> rgba;
    uint16_t width = 0;
    uint16_t height = 0;
    bool ok = false;
};

class IImageDecoder {
public:
    virtual ~IImageDecoder() = default;
    virtual DecodedImage decode(const uint8_t* bytes, size_t size,
                                size_t maxDecodedBytes) = 0;
};

} // namespace Caesura
