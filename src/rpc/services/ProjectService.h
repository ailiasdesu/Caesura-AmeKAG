#pragma once
// ProjectService.h -- editor project-domain core service (task book §14:
// HTTP handlers must NOT embed business logic; they become thin transport
// wrappers over Core Services so the same logic can serve CLI / IDE /
// automation later).
//
// Lives in src/rpc/services (editor-internal, NOT api/) -- like the rest of
// the editor RPC layer it may evolve freely. ProjectContext (§7) is the only
// filesystem-root source; no business code may guess with fs::current_path.

#include "../ProjectContext.h"

#include <nlohmann_json.hpp>

#include <string>

namespace Caesura {
namespace rpc {
namespace service {

struct ServiceResult {
    int status = 200;
    nlohmann::json body = nlohmann::json::object();

    static ServiceResult ok(nlohmann::json b = nlohmann::json::object()) {
        return {200, std::move(b)};
    }
    static ServiceResult err(int code, const std::string& msg) {
        return {code, {{"error", msg}}};
    }
};

class ProjectService {
public:
    explicit ProjectService(ProjectContext ctx) : m_ctx(std::move(ctx)) {}

    // GET /api/project/templates
    ServiceResult listTemplates();
    // POST /api/project/create
    ServiceResult create(const std::string& templateId, const std::string& name);

private:
    ServiceResult createImpl(const std::string& templateId,
                             const std::string& name);

public:
    // POST /api/project/duplicate
    ServiceResult duplicate(const std::string& src, const std::string& name);
    // POST /api/project/import
    ServiceResult importProject(const std::string& srcPath, const std::string& name);
    // GET /api/project/list
    ServiceResult list();
    // GET /api/project/meta
    ServiceResult metaGet(const std::string& path);
    // POST /api/project/meta
    ServiceResult metaSave(const std::string& path, nlohmann::json meta);

private:
    ProjectContext m_ctx;
};

} // namespace service
} // namespace rpc
} // namespace Caesura
