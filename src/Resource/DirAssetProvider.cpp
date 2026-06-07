#include "DirAssetProvider.h"
#include <sys/stat.h>
#include <fstream>

namespace caesura {

std::string DirAssetProvider::fullPath(const std::string& path) const
{
    if (m_rootDir.empty()) return path;
    if (m_rootDir.back() == '\\' || m_rootDir.back() == '/')
        return m_rootDir + path;
    return m_rootDir + "/" + path;
}

std::vector<uint8_t> DirAssetProvider::read(const std::string& path)
{
    std::string fp = fullPath(path);
    std::ifstream file(fp, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    std::streamsize size = file.tellg();
    if (size <= 0) return {};

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) return {};
    return data;
}

bool DirAssetProvider::exists(const std::string& path)
{
    std::string fp = fullPath(path);
    struct stat st;
    return (stat(fp.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

} // namespace caesura