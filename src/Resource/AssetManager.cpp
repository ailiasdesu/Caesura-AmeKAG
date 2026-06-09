#include "AssetManager.h"
#include "DirAssetProvider.h"
#include "../CARC/CarcAssetProvider.h"
#include "../CARC/CARCReader.h"
#include <cstdio>
#include <memory>

namespace Caesura {

AssetManager& AssetManager::instance() {
    static AssetManager s;
    return s;
}

void AssetManager::init() {
    if (m_initialized) return;

    m_chain.addProvider(std::make_unique<caesura::DirAssetProvider>(""));
    m_chain.addProvider(std::make_unique<caesura::DirAssetProvider>("assets"));

    const char* carcFiles[] = {"data.carc", "game.carc", "patch.carc"};
    for (const char* fname : carcFiles) {
        auto reader = std::make_unique<carc::CARCReader>();
        if (reader->open(fname)) {
            m_chain.addProvider(
                std::make_unique<carc::CarcAssetProvider>(std::move(reader)));
            printf("[AssetManager] Registered CARC: %s\n", fname);
        }
    }

    m_initialized = true;
    printf("[AssetManager] Initialized.\n");
}

void AssetManager::shutdown() {
    m_initialized = false;
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
