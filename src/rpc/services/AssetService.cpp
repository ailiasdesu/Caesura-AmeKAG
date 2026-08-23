#include "AssetService.h"

#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace Caesura {
namespace rpc {
namespace service {

ServiceResult AssetService::listAssets(const std::string& type) {
    Json arr = Json::array();
    const std::vector<std::pair<std::string, std::string>> dirs = {
        {"bg", "image"}, {"fg", "image"}, {"char", "image"},
        {"ui", "image"}, {"bgm", "audio"}, {"voice", "audio"},
        {"se", "audio"}, {"scripts", "script"},
    };
    auto includeDir = [&](const std::string& dir, const std::string& coarse) {
        if (type.empty()) return true;
        if (type == coarse) return true;
        if (type == dir) return true;
        return false;
    };
    for (const auto& [dir, coarse] : dirs) {
        if (!includeDir(dir, coarse)) continue;
        const fs::path path = m_ctx.assetRoot() / dir;
        std::error_code ec;
        if (!fs::exists(path, ec)) continue;
        try {
            for (const auto& entry : fs::directory_iterator(path, ec)) {
                std::error_code fe;
                if (!entry.is_regular_file(fe) || fe) continue;
                const std::string name = entry.path().filename().string();
                Json obj;
                obj["path"] = "assets/" + dir + "/" + name;
                obj["name"] = name;
                obj["type"] = coarse;
                obj["kind"] = dir;
                arr.push_back(std::move(obj));
            }
        } catch (...) {}
    }
    return ServiceResult::ok(arr);
}

} // namespace service
} // namespace rpc
} // namespace Caesura
