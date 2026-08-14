#pragma once

#include "ProviderChain.h"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace Caesura {

// Asset reader backed by ProviderChain (Dir + CARC).
// Thread-safe for concurrent reads from a single worker thread.
class AssetManager {
public:
    AssetManager() = default;
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    void init();
    void shutdown();

    // Inject a pre-built asset provider (e.g. CARC from Engine)
    void addProvider(std::unique_ptr<Caesura::IAssetProvider> provider);

    std::vector<uint8_t> read(const std::string& path);
    bool exists(const std::string& path);

private:
    Caesura::ProviderChain m_chain;
    bool m_initialized = false;
};

} // namespace Caesura
