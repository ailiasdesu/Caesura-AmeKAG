#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Caesura {

// ============================================================================
// IAsyncLoader — pure virtual interface for async asset loading
// ============================================================================
// AsyncLoader implements this interface. BackendRegistry stores IAsyncLoader*.

struct CompletedLoad {
    int         id = 0;
    std::string path;
    std::string type;
    bool        success = false;
    std::vector<uint8_t> rgba;
    uint16_t    width  = 0;
    uint16_t    height = 0;
    std::vector<uint8_t> data;
};

class IAsyncLoader {
public:
    virtual ~IAsyncLoader() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;

    virtual int  enqueue(const std::string& path, const std::string& type) = 0;
    virtual void cancelAll() = 0;
    virtual bool poll() = 0;

    virtual int  pendingCount() const = 0;
    virtual bool isRunning()   const = 0;
};

} // namespace Caesura
