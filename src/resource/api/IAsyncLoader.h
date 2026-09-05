#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Caesura {

// Reserved once by AsyncLoader::init through SDL_RegisterEvents. Zero means
// no event type is available yet; consumers must not assign this value.
extern uint32_t CAESURA_EVENT_ASYNC_LOAD;

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
    // Publish completed results as SDL events. On successful publication the
    // consumer owns user.data1 (CompletedLoad*) and must delete it exactly once;
    // user.data2 identifies the publishing loader. Rejected events are released
    // by the loader. shutdown() reclaims only its still-queued SDL payloads.
    virtual bool poll() = 0;

    // Non-SDL delivery for hosts without an SDL event loop (headless/editor
    // mode): returns and removes all completed loads; the caller owns the
    // results and must dispatch them (texture upload + Lua callback).
    virtual std::vector<CompletedLoad> drainCompleted() = 0;

    virtual int  pendingCount() const = 0;
    virtual bool isRunning()   const = 0;
};

} // namespace Caesura
