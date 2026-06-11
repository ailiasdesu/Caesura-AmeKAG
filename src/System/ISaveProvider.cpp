// ===========================================================================
//  Caesura (AmeKAG) — ISaveProvider.cpp
//  Local filesystem save provider implementation.
// ===========================================================================

#include "ISaveProvider.h"
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
    return content;
}

bool LocalFileSaveProvider::writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.c_str(), static_cast<std::streamsize>(content.size()));
    return out.good();
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

} // namespace Caesura
