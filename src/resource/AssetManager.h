#pragma once

#include "ProviderChain.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Caesura {

// Singleton asset reader backed by ProviderChain (Dir + CARC).
// Thread-safe for concurrent reads from a single worker thread.
class AssetManager {
public:
    static AssetManager& instance();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    void init();
    void shutdown();

    std::vector<uint8_t> read(const std::string& path);
    bool exists(const std::string& path);

private:
    AssetManager() = default;

    caesura::ProviderChain m_chain;
    bool m_initialized = false;
};

} // namespace Caesura
