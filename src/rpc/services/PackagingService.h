#pragma once
// PackagingService.h -- build/package domain core service (task book §14).

#include "ProjectService.h"

#include <functional>
#include <memory>
#include <string>

namespace Caesura {
namespace carc {
class IArchiveWriter;
}

namespace rpc {
namespace service {

class PackagingService {
public:
    // writerFactory: constructs an archive writer (owned by the engine);
    // may be null when the archive backend is unavailable.
    using ArchiveWriterFactory =
        std::function<std::unique_ptr<Caesura::carc::IArchiveWriter>()>;

    PackagingService(ProjectContext ctx, ArchiveWriterFactory writerFactory)
        : m_ctx(std::move(ctx)), m_writerFactory(std::move(writerFactory)) {}

    // POST /api/build
    ServiceResult build(const std::string& outputPath, const std::string& keyPath);
    // POST /api/package/web
    ServiceResult packageWeb(const std::string& storyPath,
                             const std::string& outName);

private:
    ProjectContext m_ctx;
    ArchiveWriterFactory m_writerFactory;
};

} // namespace service
} // namespace rpc
} // namespace Caesura
