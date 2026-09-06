#include "ImageDecoder.h"
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <algorithm>

#include <stb/stb_image.h>   // declarations only; impl lives in render/stb_impl.cpp

namespace Caesura {

DecodedImage CpuImageDecoder::decode(const uint8_t* bytes, size_t size, size_t maxDecodedBytes) {
    return ImageDecoder::decode(bytes,size,maxDecodedBytes);
}

// Cap decoded dimensions (mirrors TextureManager::validateTextureDimensions):
// a forged header could declare absurd sizes and exhaust memory.
static constexpr uint32_t kMaxDim = 16384;
static constexpr uint64_t kMaxPixels = 256ull * 1024 * 1024;  // 256MP
static uint32_t little32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
}
static bool hasContainerHeader(const uint8_t* data, size_t size) {
    static constexpr uint8_t ktx[] = {0xab,0x4b,0x54,0x58,0x20,0x31,0x31,0xbb,0x0d,0x0a,0x1a,0x0a};
    if (size >= 128 && std::memcmp(data,"DDS ",4)==0) return true;
    if (size >= 64 && std::memcmp(data,ktx,sizeof(ktx))==0) return true;
    static constexpr uint8_t ktx2[] = {0xab,0x4b,0x54,0x58,0x20,0x32,0x30,0xbb,0x0d,0x0a,0x1a,0x0a};
    if (size >= 80 && std::memcmp(data,ktx2,sizeof(ktx2))==0) return true;
    if (size >= 52 && data[0]=='P' && data[1]=='V' && data[2]=='R' && data[3]==3) return true;
    if (size >= 12 && std::memcmp(data,"RIFF",4)==0 && std::memcmp(data+8,"WEBP",4)==0
        && uint64_t(little32(data+4))+8 <= size) return true;
    if (size >= 16 && little32(data)==0x01312f76) return true; // OpenEXR
    if (size >= 24 && std::memcmp(data+4,"ftyp",4)==0) return true; // HEIF/AVIF decoder
    return size >= 16 && data[0]==0x13 && data[1]==0xab && data[2]==0xa1 && data[3]==0x5c;
}
static bool dimensionsValid(int w, int h, size_t maxDecodedBytes) {
    if (w <= 0 || h <= 0) return false;
    if (static_cast<uint32_t>(w) > kMaxDim || static_cast<uint32_t>(h) > kMaxDim) return false;
    const auto pixels=static_cast<uint64_t>(w)*static_cast<uint64_t>(h);
    return pixels <= kMaxPixels && pixels <= maxDecodedBytes/4;
}

static uint16_t little16(const uint8_t* data) {
    return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
}

static bool isTgaHeader(const uint8_t* data, size_t size) {
    if (size < 17 || data[1] > 1) return false;
    const int type = data[2];
    if (data[1] == 1 ? (type != 1 && type != 9)
                    : (type != 2 && type != 3 && type != 10 && type != 11)) return false;
    const auto bits = data[16];
    return little16(data + 12) && little16(data + 14)
        && (bits == 8 || bits == 15 || bits == 16 || bits == 24 || bits == 32);
}

// stb's TGA reader tolerates short reads. Check each source pixel/packet span
// before decoding, including indexed and RLE images, without allocating pixels.
static bool completeTga(const uint8_t* data, size_t size, size_t maxDecodedBytes) {
    if (size < 18 || !dimensionsValid(little16(data + 12), little16(data + 14), maxDecodedBytes))
        return false;
    size_t offset = 18 + size_t(data[0]);
    if (data[1] == 1) {
        const size_t paletteBytes = size_t(little16(data + 5)) * ((data[7] + 7) / 8);
        // Match the bytes consumed by the vendored indexed TGA reader.
        offset += little16(data + 3) + paletteBytes;
    }
    if (offset > size) return false;
    const size_t stride = (data[16] + 7) / 8;
    size_t remaining = size_t(little16(data + 12)) * little16(data + 14);
    if (data[2] < 8) return remaining <= (size - offset) / stride;
    while (remaining) {
        if (offset == size) return false;
        const uint8_t packet = data[offset++];
        const size_t count = 1 + (packet & 127);
        const size_t bytes = (packet & 128 ? 1 : count) * stride;
        if (count > remaining || bytes > size - offset) return false;
        remaining -= count;
        offset += bytes;
    }
    return true;
}

static bool completeKtx(const uint8_t* data, size_t size, size_t maxDecodedBytes) {
    if (size < 64 || little32(data + 12) != 0x04030201
        || little32(data + 56) > 16) return false;
    bimg::ImageContainer header{};
    if (!bimg::imageParse(header, data, static_cast<uint32_t>(size))
        || header.m_format >= bimg::TextureFormat::Count
        || !dimensionsValid(static_cast<int>(header.m_width), static_cast<int>(header.m_height), maxDecodedBytes))
        return false;
    const auto& block = bimg::getBlockInfo(header.m_format);
    if (!block.blockWidth || !block.blockHeight || !block.blockSize) return false;
    uint64_t offset = header.m_offset;
    const uint64_t sides = uint64_t(header.m_numLayers) * (header.m_cubeMap ? 6 : 1);
    uint64_t width = header.m_width, height = header.m_height, depth = header.m_depth;
    if (!sides || depth > kMaxDim) return false;
    for (uint8_t mip = 0; mip < header.m_numMips; ++mip) {
        const uint64_t blocksX = std::max<uint64_t>(block.minBlockX, (width + block.blockWidth - 1) / block.blockWidth);
        const uint64_t blocksY = std::max<uint64_t>(block.minBlockY, (height + block.blockHeight - 1) / block.blockHeight);
        const uint64_t faceBytes = blocksX * blocksY * std::max<uint64_t>(1, depth) * block.blockSize;
        if (offset > size || size - offset < 4 || faceBytes > size) return false;
        const uint64_t declared = little32(data + offset);
        const uint64_t declaredSides = header.m_numLayers == 1 && header.m_cubeMap ? 1 : sides;
        if (faceBytes > (size - offset - 4) / sides || declared != faceBytes * declaredSides) return false;
        offset += 4 + faceBytes * sides;
        width >>= 1; height >>= 1; depth >>= 1;
    }
    return true;
}

static DecodedImage fromStb(const uint8_t* data, size_t size, size_t maxDecodedBytes) {
    DecodedImage out;
    int w = 0, h = 0, channels = 0;
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(data,static_cast<int>(size),&w,&h,&channels,4),stbi_image_free);
    if (!pixels || w <= 0 || h <= 0) {
        return out;
    }
    out.width  = static_cast<uint16_t>(w);
    out.height = static_cast<uint16_t>(h);
    if (!dimensionsValid(w, h,maxDecodedBytes)) {
        return {};
    }
    out.rgba.assign(pixels.get(),pixels.get()+static_cast<size_t>(w)*static_cast<size_t>(h)*4);
    out.ok = true;
    return out;
}

static DecodedImage fromBimg(bimg::ImageContainer* img, size_t maxDecodedBytes) {
    if (!img || !dimensionsValid(static_cast<int>(img->m_width),
                                 static_cast<int>(img->m_height),maxDecodedBytes)) {
        return {};
    }
    DecodedImage out;
    if (!img || img->m_width == 0 || img->m_height == 0) return out;

    const uint32_t w = img->m_width;
    const uint32_t h = img->m_height;
    out.width  = static_cast<uint16_t>(w);
    out.height = static_cast<uint16_t>(h);

    const uint8_t* src = static_cast<const uint8_t*>(img->m_data);
    if (!src) return out;

    // RE-2: fail-closed bounds check before reading pixel data. bimg's
    // ImageContainer::m_size is the byte size of the raw image data; a forged
    // KTX/DDS header could declare, say, RGBA8 dimensions whose w*h*4 span
    // overruns the actual m_data buffer, causing an out-of-bounds read
    // (assign()/the loops below would read past the end). Reject unless the
    // container holds enough bytes for the format's uncompressed span.
    const size_t pixelStride = (img->m_format == bimg::TextureFormat::RGB8) ? 3u : 4u;
    const size_t requiredBytes = static_cast<size_t>(w) * static_cast<size_t>(h) * pixelStride;
    if (img->m_size < requiredBytes) return {};  // not enough raw data for declared span (review RE-2)
    out.rgba.resize(static_cast<size_t>(w)*static_cast<size_t>(h)*4);

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

DecodedImage ImageDecoder::decode(const uint8_t* data, size_t size, size_t maxDecodedBytes) {
    DecodedImage out;
    if (!data || size == 0 || size > static_cast<size_t>((std::numeric_limits<int>::max)())
        || maxDecodedBytes < 4) return out;
    if (size >= 2 && data[0]=='B' && data[1]=='M') {
        if (size < 18 || little32(data+2)>size || little32(data+10)>size
            || uint64_t(little32(data+14))+14>size) return {};
    }

    // stb first: it covers the common game formats (PNG/JPEG/BMP/TGA/GIF)
    // and is hardened against malformed/edge-case inputs (a 1x1 PNG used
    // to crash bimg::imageParse with an access violation). bimg remains
    // the fallback for DDS/KTX-style containers stb cannot read.
    try {
        if (isTgaHeader(data, size) && !completeTga(data, size, maxDecodedBytes)) return {};
        int width=0,height=0,channels=0;
        if (stbi_info_from_memory(data,static_cast<int>(size),&width,&height,&channels)) {
            if (!dimensionsValid(width,height,maxDecodedBytes)) return {};
            return fromStb(data,size,maxDecodedBytes);
        }
        // A corrupt common format is not a container. In particular, routing
        // a truncated BMP to bimg can fault before it rejects the short header.
        if (!hasContainerHeader(data,size)) return {};
        if (size >= 12 && std::memcmp(data, "\xabKTX 11\xbb\r\n\x1a\n", 12) == 0
            && !completeKtx(data, size, maxDecodedBytes)) return {};
        bx::DefaultAllocator allocator;
        std::unique_ptr<bimg::ImageContainer,decltype(&bimg::imageFree)> img(
            bimg::imageParse(&allocator,data,static_cast<uint32_t>(size)),bimg::imageFree);
        if (img) out=fromBimg(img.get(),maxDecodedBytes);
    } catch (const std::bad_alloc&) {
        return {};
    }
    return out;
}

} // namespace Caesura
