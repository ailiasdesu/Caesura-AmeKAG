// CloudSaveProvider — ISaveProvider backed by Steam Remote Storage
#include "CloudSaveProvider.h"
#include "../steam/ISteamBackend.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace Caesura {

CloudSaveProvider::CloudSaveProvider(ISteamBackend* steam) : m_steam(steam) {}

std::string CloudSaveProvider::readFile(const std::string& path) {
    if (!m_steam) return "";
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
        std::string result;
        result.reserve(totalSize);
        for (int32_t i = 0; i < numChunks; ++i) {
            std::ostringstream chunkName;
            chunkName << path << ".chunk" << std::setfill('0') << std::setw(3) << i;
            int32_t chunkLen = m_steam->cloudFileSize(chunkName.str().c_str());
            if (chunkLen <= 0) return "";
            result.resize(result.size() + chunkLen);
            m_steam->cloudRead(chunkName.str().c_str(), &result[result.size() - chunkLen], chunkLen);
        }
        return result;
    }
    // Single file read
    int32_t size = m_steam->cloudFileSize(path.c_str());
    if (size <= 0) return "";
    std::string result(size, '\0');
    m_steam->cloudRead(path.c_str(), &result[0], size);
    return result;
}

bool CloudSaveProvider::writeFile(const std::string& path, const std::string& content) {
    if (!m_steam) return false;
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
        int32_t offset = i * kChunkSize;
        int32_t chunkLen = (offset + kChunkSize > size) ? (size - offset) : kChunkSize;
        if (!m_steam->cloudWrite(chunkName.str().c_str(), content.data() + offset, chunkLen)) return false;
    }
    return true;
}

bool CloudSaveProvider::deleteFile(const std::string& path) {
    if (!m_steam) return false;
    // Delete chunks if present
    std::string metaName = path + ".meta";
    if (m_steam->cloudFileExists(metaName.c_str())) {
        int32_t metaSize = m_steam->cloudFileSize(metaName.c_str());
        if (metaSize > 0 && metaSize <= 1024) {
            char metaBuf[1024] = {};
            m_steam->cloudRead(metaName.c_str(), metaBuf, sizeof(metaBuf));
            int32_t totalSize = 0, numChunks = 0;
            sscanf(metaBuf, "%d,%d", &totalSize, &numChunks);
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

bool CloudSaveProvider::pushToCloud(const std::string& slotPath) {
    std::string content = readFile(slotPath);
    if (content.empty()) return false;
    return writeFile(slotPath, content);
}

bool CloudSaveProvider::pullFromCloud(const std::string& slotPath) {
    // pullFromCloud is handled by readFile
    (void)slotPath;
    return false;
}

} // namespace Caesura
