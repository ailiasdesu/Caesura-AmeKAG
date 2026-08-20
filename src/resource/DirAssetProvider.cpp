#include "DirAssetProvider.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Caesura {

std::string DirAssetProvider::fullPath(const std::string& path) const
{
    if (path.empty()) return {};

    const fs::path requested(path);
    if (requested.is_absolute() || requested.has_root_name()) return {};
    for (const auto& part : requested) {
        if (part == "..") return {};
    }

    std::error_code ec;
    fs::path root = m_rootDir.empty()
        ? fs::current_path(ec)
        : fs::absolute(fs::path(m_rootDir), ec);
    if (ec) return {};
    root = fs::weakly_canonical(root, ec);
    if (ec) return {};

    const fs::path candidate = fs::weakly_canonical(root / requested, ec);
    if (ec) return {};
    const fs::path relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return {};
    for (const auto& part : relative) {
        if (part == "..") return {};
    }
    return candidate.string();
}

std::vector<uint8_t> DirAssetProvider::read(const std::string& path)
{
    const std::string fp = fullPath(path);
    if (fp.empty()) return {};

    std::ifstream file(fp, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    const std::streamsize size = file.tellg();
    if (size <= 0) return {};

    // Cap a single asset read (review RD-3): a corrupt or misleading file
    // must not trigger a multi-GB heap allocation. 512 MiB matches the
    // documented per-asset ceiling used by the packaged formats.
    constexpr std::streamsize kMaxAssetBytes = 512ull * 1024ull * 1024ull;
    if (size > kMaxAssetBytes) return {};

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) return {};
    return data;
}

bool DirAssetProvider::exists(const std::string& path)
{
    const std::string fp = fullPath(path);
    if (fp.empty()) return false;
    std::error_code ec;
    return fs::is_regular_file(fs::path(fp), ec) && !ec;
}

} // namespace Caesura
