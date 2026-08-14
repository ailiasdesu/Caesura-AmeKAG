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

} // namespace Caesura
