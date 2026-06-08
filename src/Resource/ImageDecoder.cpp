#include "ImageDecoder.h"
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <cstdio>

#include "../../external/stb/stb_image.h"

namespace Caesura {

static DecodedImage fromStb(const uint8_t* data, size_t size) {
    DecodedImage out;
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        data, static_cast<int>(size), &w, &h, &channels, 4);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) stbi_image_free(pixels);
        return out;
    }
    out.width  = static_cast<uint16_t>(w);
    out.height = static_cast<uint16_t>(h);
    out.rgba.assign(pixels, pixels + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    stbi_image_free(pixels);
    out.ok = true;
    return out;
}

static DecodedImage fromBimg(bimg::ImageContainer* img) {
    DecodedImage out;
    if (!img || img->m_width == 0 || img->m_height == 0) return out;

    const uint32_t w = img->m_width;
    const uint32_t h = img->m_height;
    out.width  = static_cast<uint16_t>(w);
    out.height = static_cast<uint16_t>(h);
    out.rgba.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);

    const uint8_t* src = img->m_data;
    if (!src) return out;

    if (img->m_format == bimg::TextureFormat::RGBA8) {
        out.rgba.assign(src, src + out.rgba.size());
        out.ok = true;
        return out;
    }
    if (img->m_format == bimg::TextureFormat::BGRA8) {
        for (uint32_t i = 0; i < w * h; ++i) {
            out.rgba[i * 4 + 0] = src[i * 4 + 2];
            out.rgba[i * 4 + 1] = src[i * 4 + 1];
            out.rgba[i * 4 + 2] = src[i * 4 + 0];
            out.rgba[i * 4 + 3] = src[i * 4 + 3];
        }
        out.ok = true;
        return out;
    }
    if (img->m_format == bimg::TextureFormat::RGB8) {
        for (uint32_t i = 0; i < w * h; ++i) {
            out.rgba[i * 4 + 0] = src[i * 3 + 0];
            out.rgba[i * 4 + 1] = src[i * 3 + 1];
            out.rgba[i * 4 + 2] = src[i * 3 + 2];
            out.rgba[i * 4 + 3] = 255;
        }
        out.ok = true;
        return out;
    }

    return out;
}

DecodedImage ImageDecoder::decode(const uint8_t* data, size_t size) {
    DecodedImage out;
    if (!data || size == 0) return out;

    bx::DefaultAllocator allocator;
    bimg::ImageContainer* img = bimg::imageParse(&allocator, data, static_cast<uint32_t>(size));
    if (img) {
        out = fromBimg(img);
        bimg::imageFree(img);
        if (out.ok) return out;
    }

    return fromStb(data, size);
}

} // namespace Caesura
