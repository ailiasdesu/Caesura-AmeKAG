// ===========================================================================
//  Caesura (AmeKAG) — ISaveProvider.cpp
//  Local filesystem save provider implementation.
// ===========================================================================

#include "ISaveProvider.h"
#include <fstream>
#include <cstdio>

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
    // Stub: pattern-based listing requires filesystem API.
    // The full implementation would use std::filesystem::directory_iterator.
    // For now, SaveManager uses its own slot-based iteration.
    (void)pattern;
    return {};
}

} // namespace Caesura