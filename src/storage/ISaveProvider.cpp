// ===========================================================================
//  Caesura (AmeKAG) — ISaveProvider.cpp
//  Local filesystem save provider implementation.
// ===========================================================================

#include "LocalFileSaveProvider.h"
#include <fstream>
#include <cstdio>
#include <filesystem>

namespace Caesura {

std::string LocalFileSaveProvider::readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    in.seekg(0, std::ios::end);
    auto sz = in.tellg();
    if (sz <= 0 || static_cast<size_t>(sz) > 10 * 1024 * 1024) return "";
    std::string content(static_cast<size_t>(sz), '\0');
    in.seekg(0, std::ios::beg);
    in.read(&content[0], sz);
    if (!in.good()) return "";
    return content;
}

bool LocalFileSaveProvider::writeFile(const std::string& path, const std::string& content) {
    // ST-3: size cap + atomic write. This provider is used directly by the
    // cloud-pull path (pullFromCloud), so it must enforce the same 10 MiB
    // limit that SaveManager::MAX_SAVE_SIZE enforces on its own layout.
    // Writing unencrypted payloads larger than this would also be rejected on
    // read-back by readFile() below.
    static constexpr size_t kMaxWriteSize = 10 * 1024 * 1024;  // 10 MiB, matches SaveManager MAX_SAVE_SIZE (review ST-3)
    if (content.size() > kMaxWriteSize) return false;

    // Atomic write: write to a temp file next to the target, flush, then
    // atomically rename into place. A crash mid-write leaves the previous
    // save intact instead of truncating it.
    const std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.c_str(), static_cast<std::streamsize>(content.size()));
    out.flush();
    if (!out.good()) {
        out.close();
        std::filesystem::remove(tmpPath);  // no stale partial temp file
        return false;
    }
    out.close();

    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        // On Windows, rename fails when the destination already exists; fall
        // back to remove-then-rename so we never leave a stale temp behind.
        std::error_code rmEc;
        std::filesystem::remove(path, rmEc);
        std::filesystem::rename(tmpPath, path, ec);
        if (ec) {
            std::filesystem::remove(tmpPath);
            return false;
        }
    }
    return true;
}

bool LocalFileSaveProvider::deleteFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

std::vector<std::string> LocalFileSaveProvider::listFiles(const std::string& pattern) {
    std::vector<std::string> result;
    std::string p = pattern;
    auto slash = p.find_last_of("/\\");
    std::string dirPath = (slash != std::string::npos) ? p.substr(0, slash) : ".";
    std::string glob = (slash != std::string::npos) ? p.substr(slash + 1) : pattern;
    bool matchAll = (glob == "*" || glob == "*.*");

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) continue;
            std::string fn = entry.path().filename().string();
            if (matchAll || fn == glob) {
                result.push_back(entry.path().string());
            }
        }
    } catch (const std::exception&) {
        // Directory may not exist — return empty
    }
    return result;
}

bool LocalFileSaveProvider::pushToCloud(const std::string&) {
    return false;
}

bool LocalFileSaveProvider::pullFromCloud(const std::string&) {
    return false;
}

} // namespace Caesura
