#pragma once
// AssetService.h -- asset listing domain core service (task book §14).

#include "../ProjectContext.h"
#include "ProjectService.h"

#include <nlohmann_json.hpp>

#include <string>

namespace Caesura {
namespace rpc {
namespace service {

class AssetService {
public:
    explicit AssetService(ProjectContext ctx) : m_ctx(std::move(ctx)) {}

    // GET /api/assets?type=...
    ServiceResult listAssets(const std::string& type);

private:
    ProjectContext m_ctx;
};

} // namespace service
} // namespace rpc
} // namespace Caesura
