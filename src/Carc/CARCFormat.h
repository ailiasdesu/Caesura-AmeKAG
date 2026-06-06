// Caesura CARC Container Format
// Modern archive format: AES-256-GCM encrypted, Ed25519 signed, miniz compressed.
#pragma once
#include <cstdint>

namespace caesura::carc {

// --- Constants ---
inline constexpr uint32_t CARC_MAGIC = 0x43524143; // 'CARC'
inline constexpr uint32_t CARC_VERSION = 1;
inline constexpr size_t PUBLICKEY_SIZE = 32;
inline constexpr size_t SIGNATURE_SIZE = 64;
inline constexpr size_t AES_KEY_SIZE = 32;
inline constexpr size_t AES_NONCE_SIZE = 12;
inline constexpr size_t AES_TAG_SIZE = 16;
inline constexpr size_t PATH_HASH_SIZE = 32;

// --- Header (64 bytes) ---
#pragma pack(push, 1)
struct CARCHeader {
    uint32_t magic;          // 4
    uint32_t version;        // 4  (total 8)
    uint64_t contentOffset;  // 8  (total 16)
    uint64_t contentSize;    // 8  (total 24)
    uint64_t indexOffset;    // 8  (total 32)
    uint64_t indexSize;      // 8  (total 40)
    uint32_t numFiles;       // 4  (total 44)
    uint8_t  reserved[20];   // 20 (total 64)
};
static_assert(sizeof(CARCHeader) == 64, "CARCHeader must be 64 bytes");
#pragma pack(pop)

// --- Per-file entry (116 bytes with pack(1)) ---
#pragma pack(push, 1)
struct FileEntry {
    uint8_t  pathHash[PATH_HASH_SIZE];  // 32
    uint64_t offset;                     // 8  (40)
    uint64_t compressedSize;             // 8  (48)
    uint64_t originalSize;               // 8  (56)
    uint8_t  aesKey[AES_KEY_SIZE];       // 32 (88)
    uint8_t  nonce[AES_NONCE_SIZE];      // 12 (100)
    uint8_t  tag[AES_TAG_SIZE];          // 16 (116)
};
#pragma pack(pop)

// --- Key types ---
using PublicKey = uint8_t[PUBLICKEY_SIZE];
using Signature = uint8_t[SIGNATURE_SIZE];
using PrivateKey = uint8_t[64];

} // namespace caesura::carc