#include "AssetManager.h"
#include "DirAssetProvider.h"
#include <cstdio>
#include <memory>

namespace Caesura {

AssetManager::~AssetManager() {
    shutdown();
}

void AssetManager::init() {
    if (m_initialized) return;

    m_chain.addProvider(std::make_unique<Caesura::DirAssetProvider>(""));
    m_chain.addProvider(std::make_unique<Caesura::DirAssetProvider>("assets"));

    m_initialized = true;
    printf("[AssetManager] Initialized (dir providers).\n");
}

void AssetManager::addProvider(std::unique_ptr<Caesura::IAssetProvider> provider) {
    m_chain.addProvider(std::move(provider));
}

void AssetManager::shutdown() {
    m_initialized = false;
    m_chain.clear();
}

std::vector<uint8_t> AssetManager::read(const std::string& path) {
    if (!m_initialized) return {};
    return m_chain.read(path);
}

bool AssetManager::exists(const std::string& path) {
    if (!m_initialized) return false;
    return m_chain.exists(path);
}

std::vector<uint8_t> AssetManager::readAsset(const std::string& path, size_t maxBytes) {
    if (!m_initialized || maxBytes == 0 || path.empty() || path.front() == '/'
        || path.find('\0') != std::string::npos || path.find('\\') != std::string::npos
        || path.find(':') != std::string::npos || path.find("..") != std::string::npos) {
        return {};
    }
    for (const auto& provider : m_chain.providers()) {
        try {
            if (!provider->exists(path)) continue;
            auto bytes = provider->read(path);
            if (bytes.size() > maxBytes) return {};
            return bytes;
        } catch (...) {
            // An uncertain or failed selected source cannot authorize a read
            // from a different layer during transaction preparation.
            return {};
        }
    }
    return {};
}

} // namespace Caesura
