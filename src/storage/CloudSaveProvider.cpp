// CloudSaveProvider →→ ISaveProvider backed by Steam Remote Storage
#include "CloudSaveProvider.h"
#include "LocalFileSaveProvider.h"
#include "../steam/api/ISteamBackend.h"
#include "../debug/api/DebugLog.h"
#include <cstring>
#include <sstream>
#include <fstream>
#include <limits>
#include <iomanip>

namespace Caesura {

CloudSaveProvider::CloudSaveProvider(ISteamBackend* steam) : m_steam(steam) {}

// Cloud key for a slot path: Steam Remote Storage is a FLAT per-user namespace,
// so the directory component is stripped ("saves/save_2.json" -> "save_2.json").
// Mirrors HttpCloudSaveProvider::safeName. Every entry point normalizes through
// this, which also closes a real inconsistency: SaveManager hands out full paths
// ("<saveDir>/save_5.json") while pushToCloud used only the basename, so the
// same slot could end up as TWO different cloud objects depending on which call
// wrote it. It additionally keeps a local absolute path (drive letters,
// separators) out of the cloud key space.
std::string CloudSaveProvider::cloudKey(const std::string& slotPath) {
    const auto pos = slotPath.find_last_of("/\\");
    return pos == std::string::npos ? slotPath : slotPath.substr(pos + 1);
}

std::string CloudSaveProvider::readFile(const std::string& rawPath) {
    if (!m_steam) return "";
    const std::string path = cloudKey(rawPath);
    // Check if chunked
    std::string metaName = path + ".meta";
    if (m_steam->cloudFileExists(metaName.c_str())) {
        // Read chunk metadata and reassemble
        int32_t metaSize = m_steam->cloudFileSize(metaName.c_str());
        if (metaSize <= 0 || metaSize > 1024) return "";
        char metaBuf[1024] = {};
        m_steam->cloudRead(metaName.c_str(), metaBuf, sizeof(metaBuf));
        int32_t totalSize = 0;
        int32_t numChunks = 0;
        sscanf(metaBuf, "%d,%d", &totalSize, &numChunks);
        if (totalSize <= 0 || numChunks <= 0) return "";
        // Never trust .meta: reject absurd sizes and chunk counts up front.
        if (totalSize > kMaxChunkedSize) return "";
        const int64_t maxChunks =
            (static_cast<int64_t>(totalSize) + kChunkSize - 1) / kChunkSize;
        if (static_cast<int64_t>(numChunks) > maxChunks) return "";
        std::string result;
        result.reserve(totalSize);
        for (int32_t i = 0; i < numChunks; ++i) {
            std::ostringstream chunkName;
            chunkName << path << ".chunk" << std::setfill('0') << std::setw(3) << i;
            int32_t chunkLen = m_steam->cloudFileSize(chunkName.str().c_str());
            if (chunkLen <= 0) return "";
            // A chunk must never exceed the bytes still expected, so a lying
            // .meta/chunk cannot drive the buffer past totalSize.
            if (static_cast<int64_t>(chunkLen) >
                static_cast<int64_t>(totalSize) - static_cast<int64_t>(result.size()))
                return "";
            result.resize(result.size() + chunkLen);
            m_steam->cloudRead(chunkName.str().c_str(), &result[result.size() - chunkLen], chunkLen);
        }
        // Assembled length must exactly match the declared total.
        if (result.size() != static_cast<size_t>(totalSize)) return "";
        return result;
    }
    // Single file read
    int32_t size = m_steam->cloudFileSize(path.c_str());
    if (size <= 0) return "";
    std::string result(size, '\0');
    m_steam->cloudRead(path.c_str(), &result[0], size);
    return result;
}

bool CloudSaveProvider::writeFile(const std::string& rawPath, const std::string& content) {
    if (!m_steam) return false;
    const std::string path = cloudKey(rawPath);
    if (content.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) return false;
    int32_t size = static_cast<int32_t>(content.size());
    // Small files: direct write
    if (size <= kChunkSize) {
        return m_steam->cloudWrite(path.c_str(), content.data(), size);
    }
    // Large files: split into chunks
    int32_t numChunks = (size + kChunkSize - 1) / kChunkSize;
    std::ostringstream meta;
    meta << size << "," << numChunks;
    std::string metaStr = meta.str();
    std::string metaName = path + ".meta";
    if (!m_steam->cloudWrite(metaName.c_str(), metaStr.data(), (int32_t)metaStr.size())) return false;
    for (int32_t i = 0; i < numChunks; ++i) {
        std::ostringstream chunkName;
        chunkName << path << ".chunk" << std::setfill('0') << std::setw(3) << i;
        int64_t offset = static_cast<int64_t>(i) * kChunkSize;
        int32_t chunkLen = (offset + kChunkSize > size) ? (size - static_cast<int32_t>(offset)) : kChunkSize;
        if (!m_steam->cloudWrite(chunkName.str().c_str(), content.data() + offset, chunkLen)) {
            // Rollback: remove the just-written meta and the chunks written so
            // far, so a failed write never leaves a .meta pointing at missing
            // chunks (which would read back as an empty save forever).
            m_steam->cloudDelete(metaName.c_str());
            for (int32_t j = 0; j <= i; ++j) {
                std::ostringstream n;
                n << path << ".chunk" << std::setfill('0') << std::setw(3) << j;
                m_steam->cloudDelete(n.str().c_str());
            }
            return false;
        }
    }
    return true;
}

bool CloudSaveProvider::deleteFile(const std::string& rawPath) {
    if (!m_steam) return false;
    const std::string path = cloudKey(rawPath);
    // Delete chunks if present
    std::string metaName = path + ".meta";
    if (m_steam->cloudFileExists(metaName.c_str())) {
        int32_t metaSize = m_steam->cloudFileSize(metaName.c_str());
        if (metaSize > 0 && metaSize <= 1024) {
            char metaBuf[1024] = {};
            m_steam->cloudRead(metaName.c_str(), metaBuf, sizeof(metaBuf));
            int32_t totalSize = 0, numChunks = 0;
            sscanf(metaBuf, "%d,%d", &totalSize, &numChunks);
            // Same guards as readFile: a forged .meta must not drive a
            // multi-billion-iteration delete loop.
            if (totalSize <= 0 || totalSize > kMaxChunkedSize) {
                // Forged meta: still remove it (and the main file below) so a
                // later small re-save to this path is not poisoned by it.
                m_steam->cloudDelete(metaName.c_str());
                return m_steam->cloudDelete(path.c_str());
            }
            const int64_t maxChunks =
                (static_cast<int64_t>(totalSize) + kChunkSize - 1) / kChunkSize;
            if (numChunks <= 0 || static_cast<int64_t>(numChunks) > maxChunks) {
                m_steam->cloudDelete(metaName.c_str());
                return m_steam->cloudDelete(path.c_str());
            }
            for (int32_t i = 0; i < numChunks; ++i) {
                std::ostringstream chunkName;
                chunkName << path << ".chunk" << std::setfill('0') << std::setw(3) << i;
                m_steam->cloudDelete(chunkName.str().c_str());
            }
        }
        m_steam->cloudDelete(metaName.c_str());
    }
    return m_steam->cloudDelete(path.c_str());
}

std::vector<std::string> CloudSaveProvider::listFiles(const std::string&) {
    // Steam Remote Storage doesn't support directory listing
    return {};
}

// push = LOCAL FILE -> CLOUD. One direction, no merge, no timestamp compare:
// the on-disk file wins and the cloud copy of that slot is replaced. Pushing a
// stale local save therefore overwrites a newer cloud save -- that is the
// CALLER's decision. Nothing in the engine calls this on its own (only the
// explicit Lua KAG.cloud_push binding), so there is no automatic path that can
// clobber cloud data behind the player's back.
bool CloudSaveProvider::pushToCloud(const std::string& slotPath) {
    if (!m_steam) {
        DEBUG_WARN(SubSys::Storage, ErrCode::Storage_SaveWriteFailed,
                   "[CloudSaveProvider] pushToCloud(%s) refused: no Steam backend "
                   "(Steamworks unavailable or not initialized)", slotPath.c_str());
        return false;
    }
    // Read the LOCAL file only. The previous revision fell back to reading the
    // CLOUD copy and writing it straight back, so a cloud->cloud no-op reported
    // success and "nothing local to push" was indistinguishable from a real
    // upload.
    std::ifstream in(slotPath, std::ios::binary);
    if (!in.is_open()) {
        DEBUG_WARN(SubSys::Storage, ErrCode::Storage_SaveReadFailed,
                   "[CloudSaveProvider] pushToCloud(%s): no local file to push",
                   slotPath.c_str());
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    if (content.empty()) {
        DEBUG_WARN(SubSys::Storage, ErrCode::Storage_SaveReadFailed,
                   "[CloudSaveProvider] pushToCloud(%s): local file is empty; "
                   "refusing to replace the cloud copy with nothing",
                   slotPath.c_str());
        return false;
    }
    // Same ceiling readFile() enforces on the way back: never ship a payload
    // upstream that this engine would refuse to reassemble.
    if (content.size() > static_cast<size_t>(kMaxChunkedSize)) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveWriteFailed,
                  "[CloudSaveProvider] pushToCloud(%s) refused: %zu bytes exceeds "
                  "the %d-byte chunked ceiling", slotPath.c_str(), content.size(),
                  static_cast<int>(kMaxChunkedSize));
        return false;
    }
    return writeFile(cloudKey(slotPath), content);
}

// pull = CLOUD -> LOCAL FILE. One direction, no merge, no timestamp compare:
// the cloud copy wins and REPLACES the local file. This is the only call that
// can destroy an on-disk save the player made offline, so it is guarded three
// ways: an absent/empty cloud file aborts BEFORE the local file is touched; the
// write goes through LocalFileSaveProvider (temp file + atomic rename under the
// same 10 MiB ceiling as SaveManager), so a crash or an oversized cloud payload
// cannot truncate the existing save; and nothing in the engine invokes it
// automatically (only the explicit Lua KAG.cloud_pull binding).
bool CloudSaveProvider::pullFromCloud(const std::string& slotPath) {
    if (!m_steam) {
        DEBUG_WARN(SubSys::Storage, ErrCode::Storage_SaveReadFailed,
                   "[CloudSaveProvider] pullFromCloud(%s) refused: no Steam backend "
                   "(Steamworks unavailable or not initialized)", slotPath.c_str());
        return false;
    }
    const std::string content = readFile(cloudKey(slotPath));
    if (content.empty()) {
        DEBUG_WARN(SubSys::Storage, ErrCode::Storage_SaveReadFailed,
                   "[CloudSaveProvider] pullFromCloud(%s): cloud copy absent or "
                   "empty; local file left untouched", slotPath.c_str());
        return false;
    }
    // Atomic + size-capped write (ST-3). A raw ofstream here would truncate the
    // player's local save the moment it opened the file.
    LocalFileSaveProvider local;
    if (!local.writeFile(slotPath, content)) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveWriteFailed,
                  "[CloudSaveProvider] pullFromCloud(%s): local write rejected "
                  "(oversized payload or unwritable path); previous local save kept",
                  slotPath.c_str());
        return false;
    }
    return true;
}

} // namespace Caesura
